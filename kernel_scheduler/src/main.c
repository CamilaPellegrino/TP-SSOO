#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char* argv[]) {
    if(argc < 2){ // ejemplo para ejecutar: ./bin/kernel_scheduler "./kernel_scheduler.config"
        printf("Se esperaban mas parametros");
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
    logger = iniciar_logger("./logs/kernel_scheduler.log", "ProcesoKernelScheduler", LOG_LEVEL_INFO );
    config = iniciar_config(ruta_config); 

    // conectar a kernel memory
    ip = config_get_string_value (config, "IP");
    puerto_kernel_memory = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");

    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);

    // lanzar error si no se pudo conectar a kernel memory:
    exit_si_error_conexion(conexion_kernel_memory, logger, "No se pudo conectar a kernel_memory");
    
    log_info(logger, "## Conectado a Kernel Memory");
    // iniciar servidor
    puerto_kernel_scheduler = config_get_string_value (config, "PUERTO_KERNEL_SCHEDULER");
    int kernel_scheduler_fd = iniciar_servidor(puerto_kernel_scheduler);

    while(true){
        int cliente_fd = esperar_cliente(kernel_scheduler_fd);
        log_info(logger, "Me llego un cliente, %d\n", cliente_fd);
    }
    return 0;
}
