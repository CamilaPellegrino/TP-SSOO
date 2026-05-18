#include <utils/hello.h>
#include <utils/utils.h>

int main(int argc, char* argv[]) {
    // ejemplo para ejecutar: ./bin/io "./io.config 0"
    if(argc < 3){ 
        
        printf("Se esperaban mas parametros. Ejemplo: ./bin/io ./io.config 0");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];
    int tipo_io = atoi(argv[2]);
    t_log * logger;
    t_config* config;
    char* ip; 
    char * puerto_kernel_scheduler;

    //Setup inicial
    logger = iniciar_logger("io.log", "ProcesoIO", LOG_LEVEL_INFO );
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
        uint32_t pid = *(uint32_t*) list_get(paquete_datos,0);

        log_info(logger , " ## PID: %u - Inicio de IO" , pid);

        switch (tipo_io){
            case SLEEP: {
                uint32_t tiempo = *(uint32_t*)list_get(paquete_datos , 1);
                log_info(logger, " ## PID: %u - Haciendo sleep por %u milesegundos" , pid, tiempo);
                usleep(tiempo*1000);
            break;
            }

            case STDIN: {
                uint32_t tam = *(uint32_t*)list_get(paquete_datos , 1);
                log_info(logger, " ## PID: %u - Ingrese %u caracteres: " , pid ,tam );

                char* leido  = readline(">");
                char* buffer = calloc(1,tam);

                int len_a_copiar = (strlen(leido) > tam ) ? tam : strlen(leido);
                memcpy(buffer, leido , len_a_copiar);

                if(len_a_copiar < tam ){
                    memset(buffer + len_a_copiar, '\0' , tam - len_a_copiar);

                }
                
                t_paquete* p_res = crear_paquete(IO_SCH__CONEXION);
                agregar_a_paquete(p_res, buffer, tam);
                enviar_paquete_y_liberarlo(p_res ,conexion_kernel_scheduler);

                free(leido);
                free(buffer);


            break;
            }

            case STDOUT: {
                char* texto = (char*)list_get(paquete_datos , 1);
                log_info(logger, " ## PID: %u -  %s " , pid , texto );
                printf("%s\n" , texto);
            break;
            }
        }

        log_info(logger, "## PID: %u - Fin de IO" ,pid);

        //Notificar al Kernel que terminamos
        enviar_operacion(conexion_kernel_scheduler , IO_SCH__CONEXION);

        list_destroy_and_destroy_elements(paquete_datos , free);
    }

    // liberar logger, config y conexiones
    log_destroy(logger);
    config_destroy(config);
    close(conexion_kernel_scheduler);
    
    return 0;
}
