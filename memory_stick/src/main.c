#include <utils/hello.h>
#include <utils/utils.h>
#include "base_stick.h"
void atender_pedido_escritura(int);
void atender_pedido_lectura(int);
void* atender_pedidos(void* arg);
void* atender_cliente(void *arg);

t_log * logger;

int conexion_kernel_memory;

int main(int argc, char* argv[]) {
    // ejemplo para ejecutar: ./bin/memory_stick "./memory_stick.config" 32
    if(argc < 3){
        printf("Se esperaban mas parametros. Ejemplo: ./bin/memory_stick ./memory_stick.config 32");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];
    int tamanio = atoi(argv[2]);

    logger = iniciar_logger("memory_stick.log", "ProcesoMemorySticks", LOG_LEVEL_INFO);
    
    // inciar config
    t_config* config = iniciar_config(ruta_config);

    // conectarse a kernel memory
    char *ip = config_get_string_value(config, "IP");
    char *puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    log_info(logger, "## Conectado a Kernel Memory");
    handshake_cliente(conexion_kernel_memory, logger);

    t_cliente* cliente_km = iniciar_cliente(conexion_kernel_memory, CLIENTE_KERNEL_MEMORY);
    pthread_t hilo = crear_hilo_o_exit(atender_pedidos, cliente_km, "atender_pedidos", logger);
    pthread_detach(hilo);

    //iniciar como servidor
    char *puerto = config_get_string_value (config, "PUERTO_MEMORY_STICK"); //31551
    int memory_stick_fd = iniciar_servidor_o_exit(puerto, logger);
    
    // mandar info propia al kernel memory
    t_paquete *paquete = crear_paquete(STICK_KM__CONEXION);
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    agregar_string_a_paquete(paquete, puerto);
    agregar_string_a_paquete(paquete, ip);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);

    // esperar clientes
    while(true){
        int *cliente_fd = esperar_cliente(memory_stick_fd);
        log_info(logger, "Me llego un cliente, %d", *cliente_fd);
        handshake_servidor(*cliente_fd, logger);
        //creamos el hilo para atender multiples CPUs
        pthread_t thread = crear_hilo_o_exit(atender_cliente, cliente_fd, "atender_cliente", logger);
        pthread_detach(thread);
    }
    return 0;
}

void* atender_pedidos(void* arg){
    log_info(logger, "atendiendo pedidos de cliente");
    t_cliente* cliente = (t_cliente*) arg;
    int cliente_fd = cliente->fd;
    while(1){
        op_code cod_op = recibir_operacion(cliente_fd);
        if(cod_op == -1){
            if(cliente->tipo == CLIENTE_KERNEL_MEMORY){
                log_error(logger, "Error: se desconecto Kernel Memory");
                free(cliente);
                exit(EXIT_FAILURE);
            }
            log_warning(logger, "se desconecto CPU");
            free(cliente);
            break; // salgo del hilo
        }
        switch(cod_op){
            case X_STICK__ESCRITURA:{
                atender_pedido_escritura(cliente_fd);
                break;
            }case X_STICK__LECTURA:{
                atender_pedido_lectura(cliente_fd);
                break;
            }default:{
                log_warning(logger, "Operacion desconocida, cod_op: %d", cod_op);
            }
        }
    }
    return NULL;
}

void atender_pedido_escritura(int cliente_fd){
    // log_warning(logger, "Warning: Operacion de escritura no implementada en stick");
    enviar_operacion(cliente_fd, STICK_X__OK);
}

void atender_pedido_lectura(int cliente_fd){
    // log_warning(logger, "Warning: Operacion de lectura no implementada en stick");
    t_paquete* paquete = crear_paquete(STICK_X__OK);
    char* txt = "HOLA";
    agregar_string_a_paquete(paquete, txt);
    enviar_paquete_y_liberarlo(paquete, cliente_fd);
}

void* atender_cliente(void *arg){
    log_debug(logger, "atendiendo");
    int *cliente_fd_ptr = (int *) arg;
    int cliente_fd = *cliente_fd_ptr;
    free(cliente_fd_ptr);
    op_code cod_op = recibir_operacion(cliente_fd);
    switch(cod_op){
        case CPU_STICK__CONEXION: {
            log_info(logger, "Me llego el CPU");
            t_cliente* cliente_cpu = iniciar_cliente(cliente_fd, CLIENTE_CPU);
            atender_pedidos(cliente_cpu);
            break;
        }
        default:
            log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
    }
    return NULL;
}

