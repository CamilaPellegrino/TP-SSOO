#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char* argv[]) { //KERNEL MEMORY
    if(argc < 2){ // ejemplo para ejecutar: ./bin/kernel_memory "./kernel_memory.config"
        printf("Se esperaban mas parametros. Ejemplo: ./bin/kernel_memory \"./kernel_memory.config\"");
        exit(EXIT_FAILURE);
    }    
    char *ruta_config = argv[1];
    char* puerto;
    t_log* logger;
    t_config* config;
    
    logger = iniciar_logger("./logs/kernel_memory.log", "ProcesoKernelMemory", LOG_LEVEL_INFO );
    config = iniciar_config(ruta_config);
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }

    // iniciar servidor
    puerto = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    int kernel_memory_fd = iniciar_servidor(puerto);
    if(kernel_memory_fd == -1){
        log_error(logger, "No se pudo iniciar el servidor");
        exit(EXIT_FAILURE);
    }

    while(true){
        int cliente_fd = esperar_cliente(kernel_memory_fd);
        log_info(logger, "Me llego un cliente, %d\n", cliente_fd);
    }
    
    return 0;
}
