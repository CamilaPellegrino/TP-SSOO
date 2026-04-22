#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char* argv[]){
    // ejemplo para ejecutar: ./bin/cpu "./cpu.config" 0
    if(argc < 3){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/cpu ./cpu.config 0");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];
    int id_cpu = atoi(argv[2]);


    saludar("cpu");
    
    t_log* logger;
    t_config* config;
    char *ip;
    char *puerto_kernel_scheduler;
    char *puerto_kernel_memory;
    char *puerto_memory_stick;

    // crear logger
    logger = iniciar_logger("cpu.log", "ProcesoCPU", LOG_LEVEL_INFO);

    // iniciar config
    config = iniciar_config(ruta_config);
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }

    ip = config_get_string_value(config, "IP");
    
    puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    puerto_memory_stick = config_get_string_value(config, "PUERTO_MEMORY_STICK");
    puerto_kernel_scheduler = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");
    log_info(logger, "ip: %s, puerto_kernel_memory: %s, puerto_memory_stick: %s", ip, puerto_kernel_memory, puerto_memory_stick);

    // conectar a kernel scheduler
    int conexion_kernel_scheduler = crear_conexion(ip, puerto_kernel_scheduler);
    exit_si_error_conexion(conexion_kernel_scheduler, logger, "kernel scheduler");
    handshake_cliente(conexion_kernel_scheduler,logger);
    
    // conectar a kernel memory
    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    handshake_cliente(conexion_kernel_memory,logger);
    
    //mando informacion propia al km
    t_paquete *paquete = crear_paquete(CPU_KM__CONEXION);
    agregar_a_paquete(paquete, &id_cpu, sizeof(id_cpu));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);


    // conectar a memory stick
    int conexion_memory_stick = crear_conexion(ip, puerto_memory_stick);
    exit_si_error_conexion(conexion_memory_stick, logger, "memory stick");
    handshake_cliente(conexion_memory_stick,logger);
    
    // queda escuchando mensajes que envie el scheduler
    while (1)
    {
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
        if(cod_op == -1){
            log_info(logger, "se desconecto scheduler"); 
            break; 
        }
        switch(cod_op){
            case SCH_CPU__PID:{
                log_info(logger, "me llego un PID");
                break;
            }
            default:
                log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
                break;
        }
    }
    
    // liberar logger, config y conexiones
    log_destroy(logger);
    config_destroy(config);
    close(conexion_kernel_scheduler);
    close(conexion_kernel_memory);
    close(conexion_memory_stick);
    return 0;
}