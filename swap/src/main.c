#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char* argv[]) {
    if(argc < 2){ // ejemplo para ejecutar: ./bin/swap "./swap.config"
        printf("Se esperaban mas parametros");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];
    t_log * logger;
    t_config* config;
    char* ip; 
    char *puerto_kernel_memory;

    //Setup inicial
    logger = iniciar_logger("./logs/swap.log", "ProcesoSWAP", LOG_LEVEL_INFO );
    config = iniciar_config(ruta_config);
    
    //crear conexion con kernel memory
    ip = config_get_string_value (config, "IP");
    puerto_kernel_memory = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    
    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory); 
    
    return 0;
}
