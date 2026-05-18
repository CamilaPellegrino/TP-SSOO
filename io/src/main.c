#include <utils/hello.h>
#include <utils/utils.h>
void ejecucion_tipo_sleep();
void ejecucion_tipo_stdin();
void ejecucion_tipo_stdout();
t_tipo_io tipo_io_str_a_enum(char* tipo_io);


t_log * logger;
int conexion_kernel_scheduler;
int main(int argc, char* argv[]) {
    // ejemplo para ejecutar: ./bin/io "./io.config 0"
    if(argc < 3){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/io ./io.config SLEEP");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];
    char* tipo_io_str = argv[2];
    int tipo_io = tipo_io_str_a_enum(tipo_io_str);
    t_config* config;

    //Setup inicial
    logger = iniciar_logger("io.log", "ProcesoIO", LOG_LEVEL_DEBUG);
    config = iniciar_config(ruta_config);
    
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }

    //conexion a kernel scheduler
    char* ip = config_get_string_value (config, "IP");
    char* puerto_kernel_scheduler = config_get_string_value (config, "PUERTO_KERNEL_SCHEDULER");
    
    conexion_kernel_scheduler = crear_conexion(ip, puerto_kernel_scheduler);

    if(conexion_kernel_scheduler == -1){
        log_error(logger, "No se pudo conectar a kernel_scheduler");
        exit(EXIT_FAILURE);
    }
    // handshake
    handshake_cliente(conexion_kernel_scheduler, logger);

    // mando info propia a sch (el tipo de io)
    t_paquete *paquete = crear_paquete(IO_SCH__CONEXION);
    agregar_a_paquete(paquete, &tipo_io, sizeof(tipo_io));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);

    switch(tipo_io){
        case SLEEP:
            ejecucion_tipo_sleep();
            break;
        case STDIN:
            ejecucion_tipo_stdin();
            break;
        case STDOUT:
            ejecucion_tipo_stdout();
    }

    // liberar logger, config y conexiones
    log_destroy(logger);
    config_destroy(config);
    close(conexion_kernel_scheduler);
    
    return 0;
}


void ejecucion_tipo_sleep(){
    while(1){
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
        if(cod_op == -1){
            log_info(logger, "se desconecto scheduler"); 
            break; 
        }
        switch(cod_op){
            case SCH_IO__SOLICITUD:
                // scheduler me pidio algo. Como soy modulo de tipo sleep, se que me pidio que haga sleep
                t_list* paquete_solic = recibir_paquete(conexion_kernel_scheduler);
                if(paquete_solic == NULL){
                    log_error(logger, "error recibiendo paquete en io tipo sleep");
                    enviar_operacion(conexion_kernel_scheduler, IO_SCH__ERROR);
                    continue;
                }
                int tiempo_sleep = *(int*)list_get(paquete_solic, 0);
                list_destroy_and_destroy_elements(paquete_solic, free);
                usleep(tiempo_sleep * 1000);
                log_debug(logger, "termine op SLEEP, OK");
                enviar_operacion(conexion_kernel_scheduler, IO_SCH__OK);
                break;
            default:
                log_warning(logger, "Operacion invalida en ejecucion_tipo_sleep, cod_op: %d", cod_op);
        }
    }
}
void ejecucion_tipo_stdin(){
    while(1){
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
        if(cod_op == -1){
            log_info(logger, "se desconecto scheduler"); 
            break; 
        }
    }
}
void ejecucion_tipo_stdout(){
    while(1){
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
        if(cod_op == -1){
            log_info(logger, "se desconecto scheduler"); 
            break; 
        }
    }
}

t_tipo_io tipo_io_str_a_enum(char* tipo_io){

    if(strcmp(tipo_io, "SLEEP") == 0){
        return SLEEP;
    }

    if(strcmp(tipo_io, "STDIN") == 0){
        return STDIN;
    }

    if(strcmp(tipo_io, "STDOUT") == 0){
        return STDOUT;
    }
    return -1;
}