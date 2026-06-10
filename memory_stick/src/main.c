#include <utils/hello.h>
#include <utils/utils.h>
#include "base_stick.h"
void atender_pedido_escritura(int dir_fisica, void* contenido);
void atender_pedido_lectura(int dir_fisica, int cliente_fd);
void* atender_pedidos(void* arg);
void* atender_cliente(void *arg);

t_log * logger;
void* espacio_mem_principal=NULL;
int conexion_kernel_memory;
int tamanio;
void* memoria_reservada = NULL;


int main(int argc, char* argv[]) {
    // ejemplo para ejecutar: ./bin/memory_stick ./memory_stick.config 32
    if(argc < 3){
        printf("Se esperaban mas parametros. Ejemplo: ./bin/memory_stick ./memory_stick.config 32");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];
    tamanio = atoi(argv[2]);
    memoria_reservada = malloc(tamanio);

    logger = iniciar_logger("memory_stick.log", "ProcesoMemorySticks", LOG_LEVEL_INFO);
    
    // inciar config
    t_config* config = iniciar_config(ruta_config);

    // conectarse a kernel memory
    char *ip = config_get_string_value(config, "IP");
    char *puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
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
    t_paquete *paquete = crear_paquete(STICK_KM__CONEXION);
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    agregar_string_a_paquete(paquete, puerto);
    agregar_string_a_paquete(paquete, ip);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    
    int dir_fisica = 10;
    char contenido[]="Hola buenos dias";
    printf("contenido %ld\n", strlen(contenido));
    atender_pedido_escritura(dir_fisica, &contenido);
    
    // esperar clientes
    while(true){
        int *cliente_fd = esperar_cliente(memory_stick_fd);
        log_info(logger, "Me llego un cliente, %d", *cliente_fd);
        handshake_servidor(*cliente_fd, logger);
        //creamos el hilo para atender multiples CPUs
        pthread_t thread = crear_hilo_o_exit(atender_cliente, cliente_fd, "atender_cliente", logger);
        pthread_detach(thread);
    }
    return 0;
}

void* atender_pedidos(void* arg){
    log_info(logger, "atendiendo pedidos de cliente");
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
                //Recibe los datos de quien quien hacer la escritura
                //atender_pedido_escritura(dir_fisica, contenido);
                break;
            }case X_STICK__LETURA:{
                //atender_pedido_lectura(dir_fisica, &contenido);
                break;
            }default:{
                log_warning(logger, "Operacion desconocida, cod_op: %d", cod_op);
            }
        }
    }
    return NULL;
}

void atender_pedido_escritura(int dir_fisica, void* contenido) {
    
    // Validar que la dirección + el contenido no se pase de la memoria total
    if (dir_fisica + sizeof(&contenido) > tamanio) {
        log_error(logger, "Intento de escribir fuera de memoria.");
        return;
    }
    
    void* destino = (char*)memoria_reservada + dir_fisica;
    memcpy(destino, contenido, sizeof(&contenido));
    log_info(logger, "## Escritura de <%ld> bytes", sizeof(contenido));

    // Castear la memoria principal y el contenido a char* (punteros de 1 byte)
    char* ram = (char*)memoria_reservada;
    
    // 4. Leer la memoria para verificar (arreglado para no tirar Segmentation Fault)
    //int* puntero_comprobacion = (int*)(&ram[dir_fisica]);
    log_info(logger,"Comprobacion -> Leyendo el entero completo en dir %d: %s", dir_fisica, &ram[dir_fisica]);
    
    return;
}

void atender_pedido_lectura(int dir_fisica, int cliente_fd) {
    // 1. Validar que la dirección de inicio + lo que vamos a leer no se pase del límite
    if ((dir_fisica + sizeof(int)) > tamanio) {
        log_error(logger, "Segmentation fault! Intento de leer fuera de memoria. Dir: %d", dir_fisica);
        // Aquí podrías enviar un mensaje de error al CPU
        return;
    }
    // 2. Calculamos la dirección exacta apuntando al byte correcto
    void* origen = (char*)espacio_mem_principal + dir_fisica;
    
    // 3. Variable para guardar lo que leamos
    int valor_leido;
    
    // 4. Copiamos el tamaño EXACTO de un entero desde la memoria a nuestra variable
    memcpy(&valor_leido, origen, sizeof(int));
    
    // 5. Log de éxito
    log_info(logger, "Lectura exitosa. Se leyo el valor %d en la direccion %d", valor_leido, dir_fisica);
    
    // 6. Enviar el valor leído de vuelta al CPU (Ejemplo conceptual)
    // t_paquete* paquete_respuesta = crear_paquete(RESPUESTA_LECTURA_OK);
    // agregar_a_paquete(paquete_respuesta, &valor_leido, sizeof(int));
    // enviar_paquete_y_liberarlo(paquete_respuesta, cliente_fd);
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
            log_info(logger, "## CPU <%d> Conectada", *cpu_id);
            
            t_cliente* cliente_cpu = iniciar_cliente(cliente_fd, CLIENTE_CPU);
            atender_pedidos(cliente_cpu);
            break;
        }
        default:
            log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
    }
    return NULL;
}

