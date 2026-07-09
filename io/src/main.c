#include <utils/hello.h>
#include <utils/utils.h>
#include <readline/readline.h>
t_tipo_io tipo_str_a_enum(char* tipo_str);

int main(int argc, char* argv[]) {
    // ejemplo para ejecutar: ./bin/io "./io.config SLEEP"
    if(argc < 3){ 
        
        printf("Se esperaban mas parametros. Ejemplo: ./bin/io ./io.config SLEEP");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];
    char * puerto_kernel_scheduler;

    //Setup inicial
    char* tipo_str = argv[2];
    char log_path[64];
    
    t_tipo_io tipo_io = tipo_str_a_enum(tipo_str);
    

    
    if(tipo_io == INVALIDO){
        printf("Error: Tipo de IO desconocida. Opciones validas: SLEEP, STDIN, STDOUT");
        exit(EXIT_FAILURE);
    }
    snprintf(log_path, sizeof(log_path), "io_%s.log", tipo_str);
    
    t_log * logger = iniciar_logger(log_path, "ProcesoIO", LOG_LEVEL_INFO );t_config* config = iniciar_config(ruta_config);
    
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }

    //conexion a kernel scheduler
    char* ip = config_get_string_value (config, "IP");
    puerto_kernel_scheduler = config_get_string_value (config, "PUERTO_KERNEL_SCHEDULER");
    
    int conexion_kernel_scheduler = crear_conexion(ip, puerto_kernel_scheduler);

    if(conexion_kernel_scheduler == -1){
        log_error(logger, "No se pudo conectar a kernel_scheduler");
        exit(EXIT_FAILURE);
    }

    //Conexion scheduler
    log_info(logger ," ## Conectado a Kerneñ Scheduler");

    // handshake
    handshake_cliente(conexion_kernel_scheduler, logger);


    // mando info propia a sch (el tipo de io)
    t_paquete *paquete = crear_paquete(IO_SCH__CONEXION);
    agregar_a_paquete(paquete, &tipo_io, sizeof(tipo_io));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
    
    
    while(1){
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
        if(cod_op == -1){
            log_info(logger, "Se desconecto scheduler"); 
            break; 
        }


        t_list* paquete_datos = recibir_paquete(conexion_kernel_scheduler);
        int  pid = *(int*) list_get(paquete_datos,0);

        log_info(logger , " ## PID: %u - Inicio de IO" , pid);

        switch (tipo_io){
            case SLEEP: {
                int tiempo = *(int*)list_get(paquete_datos , 1);
                log_info(logger, " ## PID: %u - Haciendo sleep por %u milesegundos" , pid, tiempo);
                usleep(tiempo*1000);
                enviar_operacion(conexion_kernel_scheduler, IO_SCH__OK);
            break;
            }
            case STDIN: {
                int tam = *(int*)list_get(paquete_datos , 1);
                log_info(logger, " ## PID: %u - Ingrese %u caracteres: " , pid ,tam );

                char* leido  = readline(">");
                char* buffer = calloc(1,tam);

                int len_a_copiar = (strlen(leido) > tam ) ? tam : strlen(leido);
                memcpy(buffer, leido , len_a_copiar);

                imprimir_bytes(buffer, tam);
                
                t_paquete* p_res = crear_paquete(IO_SCH__OK);
                agregar_a_paquete(p_res, buffer, tam);
                enviar_paquete_y_liberarlo(p_res ,conexion_kernel_scheduler);

                free(leido);
                free(buffer);


            break;
            }

            case STDOUT: {
                char* texto = (char*)list_get(paquete_datos , 1);
                log_info(logger, " ## PID: %u - %s" , pid , texto );
                printf("%s\n" , texto);
                enviar_operacion(conexion_kernel_scheduler, IO_SCH__OK);
            break;
            }
            default:
                break;
        }

        log_info(logger, "## PID: %u - Fin de IO" ,pid);

        list_destroy_and_destroy_elements(paquete_datos , free);
    }

    // liberar logger, config y conexiones
    log_destroy(logger);
    config_destroy(config);
    close(conexion_kernel_scheduler);
    
    return 0;
}

t_tipo_io tipo_str_a_enum(char* tipo_str){
    if(strcmp(tipo_str, "SLEEP") == 0){
        return SLEEP;
    }
    if(strcmp(tipo_str, "STDIN") == 0){
        return STDIN;
    }
    if(strcmp(tipo_str, "STDOUT") == 0){
        return STDOUT;
    }
    return INVALIDO;
}
