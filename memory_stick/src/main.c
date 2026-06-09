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

int main(int argc, char* argv[]) {
    // ejemplo para ejecutar: ./bin/memory_stick "./memory_stick.config" 32
    if(argc < 3){
        printf("Se esperaban mas parametros. Ejemplo: ./bin/memory_stick ./memory_stick.config 32");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];
    tamanio = atoi(argv[2]);
    espacio_mem_principal = malloc(tamanio * sizeof(int));

    

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
    char *puerto = config_get_string_value (config, "PUERTO_MEMORY_STICK"); //31551
    int memory_stick_fd = iniciar_servidor_o_exit(puerto, logger);
    
    // mandar info propia al kernel memory
    t_paquete *paquete = crear_paquete(STICK_KM__CONEXION);
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    agregar_string_a_paquete(paquete, puerto);
    agregar_string_a_paquete(paquete, ip);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);

    //pruebas caseras, ELIMINAR UNA VEZ FINALIZADO
    int dir_fisica = 23;
    int contenido = 1234;
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
                //atender_pedido_escritura(dir_fisica, &contenido);
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
    
    // 1. Validar que la dirección de inicio + tamaño del int (4 bytes) no pase el límite
    if ((dir_fisica + sizeof(int)) > tamanio) {
        log_error(logger, "Segmentation fault! Intento de escribir fuera de memoria.");
        return;
    }

    // 2. Calculamos la dirección exacta apuntando al byte correcto
    void* destino = (char*)espacio_mem_principal + dir_fisica;
    
    // 3. Copiamos el tamaño EXACTO del entero (4 bytes)
    // memcpy distribuirá estos 4 bytes automáticamente en dir_fisica, dir_fisica+1, etc.
    memcpy(destino, contenido, sizeof(int));
    
    // 4. Log de éxito
    log_info(logger, "Escritura exitosa de %zu bytes en la direccion %d", sizeof(int), dir_fisica);
    
    // --- VERIFICACIÓN DE LECTURA ---
    
    // Casteamos la RAM a unsigned char* para leerla estrictamente byte por byte (valores de 0 a 255)
    unsigned char* ram = (unsigned char*)espacio_mem_principal;
    
    printf("\n--- Mostrando la RAM byte por byte (Posiciones %d a %d) ---\n", dir_fisica, dir_fisica + 3);
    for(int i = dir_fisica; i < dir_fisica + sizeof(int); i++) {
        printf("pos %d -> %d\n", i, ram[i]);
    }
    
    // Para recuperar y leer el entero de forma completa (los 4 bytes unidos de nuevo):
    int valor_recuperado;
    memcpy(&valor_recuperado, destino, sizeof(int));
    printf("\nEntero completo recuperado desde la pos %d: %d\n", dir_fisica, valor_recuperado);
    
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
            log_info(logger, "Me llego el CPU");
            t_cliente* cliente_cpu = iniciar_cliente(cliente_fd, CLIENTE_CPU);
            atender_pedidos(cliente_cpu);
            break;
        }
        default:
            log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
    }
    return NULL;
}

