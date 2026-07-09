#include <utils/hello.h>
#include <utils/utils.h>
#include "base_stick.h"
void atender_pedido_escritura(int dir_fisica, void* contenido, int tamanio, int cliente_fd);
void atender_pedido_lectura(int dir_fisica, int tamanio, int cliente_fd);
void* atender_pedidos(void* arg);
void* atender_cliente(void *arg);

t_log * logger;
void* espacio_mem_principal=NULL;
int conexion_kernel_memory;
int tamanio_stick;
void* memoria_reservada = NULL;
char* memoria_principal = NULL;
int memory_delay = 0;

int main(int argc, char* argv[]) {
    // ejemplo para ejecutar: ./bin/memory_stick ./memory_stick.config 32
    if(argc < 3){
        printf("Se esperaban mas parametros. Ejemplo: ./bin/memory_stick ./memory_stick.config 32");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];
    tamanio_stick = atoi(argv[2]);
    memoria_reservada = calloc(tamanio_stick, 1);
    memoria_principal = (char*) memoria_reservada; //Para ir avanzando de a 1 byte
    t_config* config = iniciar_config(ruta_config);

    int id_stick = config_get_int_value(config, "STICK_ID");
    char log_path[64];
    snprintf(log_path, sizeof(log_path), "memory_stick_%d.log", id_stick);

    logger = iniciar_logger(log_path, "ProcesoMemorySticks", LOG_LEVEL_INFO);

    // imprimir_bytes(memoria_reservada, tamanio_stick);

    memory_delay = config_get_int_value(config, "MEMORY_DELAY");

    // conectarse a kernel memory
    char *ip_km = config_get_string_value(config, "IP_KM");
    char *puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    conexion_kernel_memory = crear_conexion(ip_km, puerto_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    log_info(logger, "## Conectado a Kernel Memory");
    handshake_cliente(conexion_kernel_memory, logger);

    t_cliente* cliente_km = iniciar_cliente(conexion_kernel_memory, CLIENTE_KERNEL_MEMORY);
    pthread_t hilo = crear_hilo_o_exit(atender_pedidos, cliente_km, "atender_pedidos", logger);
    pthread_detach(hilo);

    //iniciar como servidor
    char *puerto = config_get_string_value (config, "PUERTO_MEMORY_STICK");
    int memory_stick_fd = iniciar_servidor_o_exit(puerto, logger);
    
    // mandar info propia al kernel memory
    char* ip_stick = config_get_string_value(config, "IP_STICK");
    t_paquete *paquete = crear_paquete(STICK_KM__CONEXION);
    agregar_a_paquete(paquete, &tamanio_stick, sizeof(tamanio_stick));
    agregar_string_a_paquete(paquete, puerto);
    agregar_string_a_paquete(paquete, ip_stick);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    
    //atender_pedido_escritura(dir_fisica, &contenido);
    
    // esperar clientes
    while(true){
        int *cliente_fd = esperar_cliente(memory_stick_fd);
        log_debug(logger, "Me llego un cliente, %d", *cliente_fd);
        handshake_servidor(*cliente_fd, logger);
        //creamos el hilo para atender multiples CPUs
        pthread_t thread = crear_hilo_o_exit(atender_cliente, cliente_fd, "atender_cliente", logger);
        pthread_detach(thread);
    }
    return 0;
}

void* atender_pedidos(void* arg){
    log_debug(logger, "atendiendo pedidos de cliente");
    t_cliente* cliente = (t_cliente*) arg;
    int cliente_fd = cliente->fd;
    while(1){
        op_code cod_op = recibir_operacion(cliente_fd);
        if(cod_op == -1){
            if(cliente->tipo == CLIENTE_KERNEL_MEMORY){
                log_error(logger, "Error: se desconecto Kernel Memory");
                free(cliente);
                exit(EXIT_FAILURE);
            }
        log_warning(logger, "se desconecto CPU");
        free(cliente);
        break; // salgo del hilo
        }
        switch(cod_op){
            case X_STICK__ESCRITURA:{
                //Recibe los datos para hacer la escritura
                t_list* lista_paquete = recibir_paquete(cliente_fd);
                int dir_fisica = *(int*)list_get(lista_paquete, 0);
                int tamanio_cont = *(int*)list_get(lista_paquete, 1);
                void* contenido = list_get(lista_paquete, 2);
                atender_pedido_escritura(dir_fisica, contenido, tamanio_cont, cliente_fd);
                list_destroy_and_destroy_elements(lista_paquete, free);
                break;
            }case X_STICK__LECTURA:{
                //Recibe la direccion y tamanio a leer
                t_list* lista_paquete = recibir_paquete(cliente_fd);
                int dir_fisica = *(int*)list_get(lista_paquete, 0);
                int tamanio_cont = *(int*)list_get(lista_paquete, 1);
                log_info(logger, "## Lectura de %d bytes", tamanio_cont);
                atender_pedido_lectura(dir_fisica, tamanio_cont, cliente_fd);
                list_destroy_and_destroy_elements(lista_paquete, free);
                break;
            }default:{
                log_warning(logger, "Operacion desconocida, cod_op: %d", cod_op);
            }
        }
    }
    return NULL;
}

void atender_pedido_escritura(int dir_fisica, void* contenido, int tamanio, int cliente_fd) {
    
    // Validar que la dirección + el contenido no se pase de la memoria total
    if (dir_fisica + tamanio > tamanio_stick) {
        log_error(logger, "Error: Intento de escribir fuera de memoria");
        return;
    }

    void* destino = memoria_principal + dir_fisica;
    memcpy(destino, contenido, tamanio);
    usleep(memory_delay * 1000);
    log_info(logger, "## Escritura de %d bytes", tamanio);
    // imprimir_bytes(memoria_reservada, tamanio_stick);
    enviar_operacion(cliente_fd, STICK_X__OK);
    return;
}

void atender_pedido_lectura(int dir_fisica, int tamanio, int cliente_fd) {
    if ((dir_fisica + tamanio) > tamanio_stick) {
        log_error(logger, "Segmentation fault: Intento de leer fuera de memoria. Dir: %d", dir_fisica);
        return;
    }
    void* origen = memoria_principal + dir_fisica;
    // char* contenido_leido = malloc(tamanio +1); //El contenido que se ingresa siempre es un string (?
    // memcpy(contenido_leido, origen, tamanio);
    log_info(logger, "## Lectura de %d bytes", tamanio);
    usleep(memory_delay * 1000);
    // imprimir_bytes(origen, tamanio);
    t_paquete* paquete_respuesta = crear_paquete(STICK_X__OK);
    agregar_a_paquete(paquete_respuesta, origen, tamanio);
    enviar_paquete_y_liberarlo(paquete_respuesta, cliente_fd);
    return;
}

void* atender_cliente(void *arg){
    log_debug(logger, "atendiendo");
    int *cliente_fd_ptr = (int *) arg;
    int cliente_fd = *cliente_fd_ptr;
    free(cliente_fd_ptr);
    op_code cod_op = recibir_operacion(cliente_fd);
    switch(cod_op){
        case CPU_STICK__CONEXION: {
            t_list* lista_paquete = recibir_paquete(cliente_fd);
            int* cpu_id = list_get(lista_paquete, 0);
            log_info(logger, "## CPU %d Conectada", *cpu_id);
            
            // t_cliente* cliente_cpu = iniciar_cliente(cliente_fd, CLIENTE_CPU);
            // atender_pedidos(cliente_cpu);
            break;
        }
        default:
            log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
    }
    return NULL;
}