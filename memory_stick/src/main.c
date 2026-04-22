#include <utils/hello.h>
#include <utils/utils.h>

void* atender_cliente(void *arg);
void atender_cpu(int cpu_fd);

t_log * logger;

int main(int argc, char* argv[]) { //MEMORY STICK
    // ejemplo para ejecutar: ./bin/memory_stick "./memory_stick.config" 32
    if(argc < 3){
        printf("Se esperaban mas parametros. Ejemplo: ./bin/memory_stick ./memory_stick.config 32");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];
    int tamanio = atoi(argv[2]);

    t_config* config;
    char *ip;
    char *puerto;

    logger = iniciar_logger("memory_stick.log", "ProcesoKernelMemory", LOG_LEVEL_INFO);
    
    // inciar config
    config = iniciar_config(ruta_config);
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }

    // conectarse a kernel memory
    ip = config_get_string_value(config, "IP");
    char *puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    handshake_cliente(conexion_kernel_memory, logger);

    // mandar info propia al kernel memory
    printf("Enviando a km\n");
    t_paquete *paquete = crear_paquete(STICK_KM__CONEXION);
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    enviar_paquete(paquete, conexion_kernel_memory);

    //iniciar como servidor
    puerto = config_get_string_value (config, "PUERTO_MEMORY_STICK"); //31551
    int memory_stick_fd = iniciar_servidor(puerto);
    if(memory_stick_fd == -1){
        log_error(logger, "No se pudo iniciar el servidor");
        exit(EXIT_FAILURE);
    }

    // esperar clientes
    while(true){
        int *cliente_fd = esperar_cliente(memory_stick_fd);
        log_info(logger, "Me llego un cliente, %d\n", *cliente_fd);
        handshake_servidor(*cliente_fd, logger);
        //creamos el hilo para atender multiples CPUs
        pthread_t thread;
        pthread_create(&thread, NULL, atender_cliente, cliente_fd);
        pthread_detach(thread);
    }
    
    return 0;
}

void* atender_cliente(void *arg){
    int *cliente_fd_ptr = (int *) arg;
    int cliente_fd = *cliente_fd_ptr;
    op_code cod_op = recibir_operacion(cliente_fd);
    switch(cod_op){
        case CPU_SCH__CONEXION: {
            char *msg = recibir_mensaje(cliente_fd);
            log_info(logger, "Me llego el CPU, mensaje recibido: %s", msg);
            free(msg);
            atender_cpu(cliente_fd);
            break;
        }default:
            log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
    }

    return NULL;
}

void atender_cpu(int cpu_fd){
    log_info(logger, "****Atendiendo al cpu");
    while(1){
        op_code cod_op = recibir_operacion(cpu_fd);
        if(cod_op == -1){
            log_warning(logger, "se desconecto cpu");
            break;
        }
    }
}
