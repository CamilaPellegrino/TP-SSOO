#include <utils/hello.h>
#include <utils/utils.h>
#include "base_swap.h"

int main(int argc, char* argv[]) {
    // ejemplo para ejecutar: ./bin/swap "./swap.config"
    if(argc < 2){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/swap ./swap.config");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];
    t_log * logger;
    t_config* config;
    char* ip; 
    char *puerto_kernel_memory;

    //Setup inicial
    logger = iniciar_logger("swap.log", "ProcesoSWAP", LOG_LEVEL_INFO );
    config = iniciar_config(ruta_config);
    
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }

    //crear conexion con kernel memory
    ip = config_get_string_value (config, "IP");
    puerto_kernel_memory = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    
    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory); 
    handshake_cliente(conexion_kernel_memory, logger);

    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
   

    // enviar
    enviar_operacion(conexion_kernel_memory, SWAP_KM__CONEXION);

    

    // liberar logger, config y conexiones
    log_destroy(logger);
    config_destroy(config);
    close(conexion_kernel_memory);

    return 0;
}
