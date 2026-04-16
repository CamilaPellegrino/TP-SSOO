#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char* argv[]) {
    // ejemplo para ejecutar: ./bin/kernel_scheduler ./kernel_scheduler.config
    if(argc < 2){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/kernel_scheduler ./kernel_scheduler.config");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];
    
    saludar("kernel_scheduler");

    t_log *logger;
    t_config *config;
    
    char *ip;
    char *puerto_kernel_memory;
    char *puerto_kernel_scheduler;

    // crear logger
    logger = iniciar_logger("kernel_scheduler.log", "ProcesoKernelScheduler", LOG_LEVEL_INFO );
    
    // crear config
    config = iniciar_config(ruta_config); 
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }
    // conectar a kernel memory
    ip = config_get_string_value (config, "IP");
    puerto_kernel_memory = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");

    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    log_info(logger, "## Conectado a Kernel Memory");
    
    // handshake a kernel memory
    handshake_cliente(conexion_kernel_memory, logger);

    // iniciar servidor
    puerto_kernel_scheduler = config_get_string_value (config, "PUERTO_KERNEL_SCHEDULER");
    int kernel_scheduler_fd = iniciar_servidor(puerto_kernel_scheduler);

    if(kernel_scheduler_fd == -1){
        log_error(logger, "No se pudo iniciar el servidor");
        exit(EXIT_FAILURE);
    }
    printf("enviando msg a mem\n");
    enviar_mensaje("Hola mundo desde sche", kernel_scheduler_fd, SCHE_CONEXION);

    // esperar clientes
    while(true){
        int cliente_fd = esperar_cliente(kernel_scheduler_fd);
        log_info(logger, "Me llego un cliente, %d", cliente_fd);

        handshake_servidor(cliente_fd, logger);
    }


    return 0;
}
