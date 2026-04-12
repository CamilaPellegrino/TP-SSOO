#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char* argv[]){
    if(argc < 2){ // ejemplo para ejecutar: ./bin/cpu "./cpu.config"
        printf("Se esperaban mas parametros");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];

    saludar("cpu");
    t_log* logger;
    t_config* config;
    char *ip;
    char *puerto_kernel_scheduler;
    char *puerto_kernel_memory;
    char *puerto_memory_stick;

    // crear logger
    logger = iniciar_logger("./logs/cpu.log", "ProcesoCPU", LOG_LEVEL_INFO);
    config = iniciar_config(ruta_config);

    ip = config_get_string_value(config, "IP");
    
    puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    puerto_memory_stick = config_get_string_value(config, "PUERTO_MEMORY_STICK");
    puerto_kernel_scheduler = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");
    log_info(logger, "ip: %s, puerto_kernel_memory: %s, puerto_memory_stick: %s", ip, puerto_kernel_memory, puerto_memory_stick);

    // conectar a kernel scheduler
    int conexion_kernel_scheduler = crear_conexion(ip, puerto_kernel_scheduler);
    
    if(conexion_kernel_scheduler == -1){
        log_error(logger, "No se pudo conectar a kernel scheduler");
        exit(EXIT_FAILURE);
    }

    // conectar a kernel memory
    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "No se pudo conectar a kernel_memory");

    // conectar a memory stick
    int conexion_memory_stick = crear_conexion(ip, puerto_memory_stick);
    if(conexion_memory_stick == -1){
        log_error(logger, "No se pudo conectar a memory stick");
        exit(EXIT_FAILURE);
    }
    return 0;
}