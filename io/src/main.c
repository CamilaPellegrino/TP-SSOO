#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char* argv[]) {
    if(argc < 2){ // ejemplo para ejecutar: ./bin/io "./io.config"
        printf("Se esperaban mas parametros. Ejemplo: ./bin/io \"./io.config\"");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];
    t_log * logger;
    t_config* config;
    char* ip; 
    char * puerto_kernel_scheduler;

    //Setup inicial
    logger = iniciar_logger("./logs/io.log", "ProcesoIO", LOG_LEVEL_INFO );
    config = iniciar_config(ruta_config);
    
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }

    //conexion a kernel scheduler
    ip = config_get_string_value (config, "IP");
    puerto_kernel_scheduler = config_get_string_value (config, "PUERTO_KERNEL_SCHEDULER");
    
    int conexion_kernel_scheduler = crear_conexion(ip, puerto_kernel_scheduler);

    if(conexion_kernel_scheduler == -1){
        log_error(logger, "No se pudo conectar a kernel_scheduler");
        exit(EXIT_FAILURE);
    }
    
    return 0;
}
