#include <utils/hello.h>
#include <utils/utils.h>
#include "base_swap.h"

char *puerto_kernel_memory;
t_log * logger;
//declaro funciones 
void atender_pedido_escritura();
void atender_pedido_lectura();
void inicializar_variables_globales(t_config* config);

int main(int argc, char* argv[]) {
    if(argc < 2){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/swap ./swap.config");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];
    t_config* config = iniciar_config(ruta_config);

    //crear conexion con kernel memory
    char* ip = config_get_string_value (config, "IP");
    
    inicializar_variables_globales(config);

    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory); 
    handshake_cliente(conexion_kernel_memory, logger);

    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");

    enviar_operacion(conexion_kernel_memory, SWAP_KM__CONEXION);

    while(1){
        op_code cod_op = recibir_operacion(conexion_kernel_memory);
        if(cod_op == -1){
            log_error(logger, "Desconexion de Kernel Memory");
            exit(EXIT_FAILURE);
        }
        switch(cod_op){
            case KM_SWAP__ESCRITURA:{
                atender_pedido_escritura();
                break;
            }case KM_SWAP__LECTURA:{
                atender_pedido_lectura();
                break;
            }default:{
                log_warning(logger, "Operacion desconocida, cod_op: %d", cod_op);
            }
        }
    }

    // liberar logger, config y conexiones
    log_destroy(logger);
    config_destroy(config);
    close(conexion_kernel_memory);

    return 0;
}


void atender_pedido_escritura(){
    log_warning(logger, "Warning: Operacion de escritura no implementada en swap");
}

void atender_pedido_lectura(){
    log_warning(logger, "Warning: Operacion de lectura no implementada en swap");
}

void inicializar_variables_globales(t_config* config){
    puerto_kernel_memory = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    t_log_level log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL")); 
    logger = iniciar_logger("swap.log", "ProcesoSWAP", log_level);
}