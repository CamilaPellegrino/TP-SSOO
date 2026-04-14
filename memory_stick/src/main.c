#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char* argv[]) { //MEMORY STICK
    if(argc < 2){ // ejemplo para ejecutar: ./bin/memory_stick "./memory_stick.config"
        printf("Se esperaban mas parametros. Ejemplo: ./bin/memory_stick \"./memory_stick.config\"");
        exit(EXIT_FAILURE);
    }
    t_log * logger;
    t_config* config;
    char *ruta_config = argv[1];
    char *ip;
    char *puerto;

    logger = iniciar_logger("./logs/memory_stick.log", "ProcesoKernelMemory", LOG_LEVEL_INFO);
    
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
    
    //iniciar como servidor
    puerto = config_get_string_value (config, "PUERTO_MEMORY_STICK"); //31551
    int memory_stick_fd = iniciar_servidor(puerto);
    if(memory_stick_fd == -1){
        log_error(logger, "No se pudo iniciar el servidor");
        exit(EXIT_FAILURE);
    }

    while(true){
        int cliente_fd = esperar_cliente(memory_stick_fd);
        log_info(logger, "Me llego un cliente, %d\n", cliente_fd);
        
    }
    
    return 0;
}
