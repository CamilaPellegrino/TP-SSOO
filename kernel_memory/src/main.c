#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char* argv[]) { //KERNEL MEMORY
    // ejemplo para ejecutar: ./bin/kernel_memory "./kernel_memory.config"
    if(argc < 2){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/kernel_memory \"./kernel_memory.config\"");
        exit(EXIT_FAILURE);
    }    
    char *ruta_config = argv[1];
    char* puerto;
    t_log* logger;
    t_config* config;
    
    logger = iniciar_logger("kernel_memory.log", "ProcesoKernelMemory", LOG_LEVEL_INFO );
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
    
    // esperar clientes
    while(true){
        int cliente_fd = esperar_cliente(kernel_memory_fd);
        log_info(logger, "Me llego un cliente, %d\n", cliente_fd);
        handshake_servidor(cliente_fd, logger);
        op_code cod_op = recibir_operacion(cliente_fd);
        
        switch(cod_op){
            case SCHE_CONEXION: 
                char *msg = recibir_mensaje(cliente_fd);
                log_info(logger, "Me llego el scheduler, mensaje recibido: %s", msg);
                break;
            default: 
                log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
        }
        return 0;
    }
}
