#include <utils/hello.h>
#include <utils/utils.h>

void* atender_cliente(void *arg);
void atender_scheduler(int sch_fd);
void atender_stick(int sch_fd);
void atender_swap(int swap_fd);
void atender_cpu(int cpu_fd);
t_log *logger;

int main(int argc, char* argv[]) { //KERNEL MEMORY
    // ejemplo para ejecutar: ./bin/kernel_memory ./kernel_memory.config
    if(argc < 2){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/kernel_memory ./kernel_memory.config");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];
    char* puerto;

    t_config* config;
    
    logger = iniciar_logger("kernel_memory.log", "ProcesoKernelMemory", LOG_LEVEL_INFO );
    config = iniciar_config(ruta_config);
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }

    // iniciar servidor
    puerto = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    int kernel_memory_fd = iniciar_servidor(puerto);
    if(kernel_memory_fd == -1){
        log_error(logger, "No se pudo iniciar el servidor");
        exit(EXIT_FAILURE);
    }
    
    // esperar clientes
    while(true){
        log_info(logger, "esperar cliente");
        int *cliente_fd = esperar_cliente(kernel_memory_fd);
        
        log_info(logger, "Me llego un cliente, %d", *cliente_fd);
        handshake_servidor(*cliente_fd, logger);

        //Creacion del hilo para atender la conexion entrante
        pthread_t thread;
        pthread_create(&thread, NULL, atender_cliente, cliente_fd);
        pthread_detach(thread);
    
    }
}

void* atender_cliente(void *arg){
    int *cliente_fd_ptr = (int *) arg;
    int cliente_fd = *cliente_fd_ptr;
    free(cliente_fd_ptr);
    op_code cod_op = recibir_operacion(cliente_fd);
    switch(cod_op){
        case SCH_KM__CONEXION: {
            char *msg = recibir_mensaje(cliente_fd);

            log_info(logger, "Me llego el scheduler, mensaje recibido: %s", msg);

            atender_scheduler(cliente_fd);
            free(msg);
            break;
        }case STICK_KM__CONEXION: {
            // recibo el paquete que me mando stick en una t_list
            t_list *lista_paquete = recibir_paquete(cliente_fd);
            // leo cada elemento de la t_list en el orden en que stick los agrego al paquete
            int *tamanio = list_get(lista_paquete, 0);
            log_info(logger, "Me llego un stick de tamanio: %d", *tamanio);
            
            // poner al stick en la lista de sticks
            // ...

            //avisar a las cpus conectadas que llego un stick para que se conecten
            // ...

            // atender stick
            atender_stick(cliente_fd);

            list_destroy_and_destroy_elements(lista_paquete, free);
            break;
        }case SWAP_KM__CONEXION:{
            log_info(logger, "SWAP");
            atender_swap(cliente_fd);
            break;
        }case CPU_KM__CONEXION:{
            t_list *lista_paquete = recibir_paquete(cliente_fd);
            int *cpu_id = list_get(lista_paquete, 0);
            log_info(logger, "Me llego cpu de id: %d", *cpu_id);

            // agregar CPU a la lista de cpus
            //mandarle todos los sticks que se conectarno hasta ahora

            atender_cpu(cliente_fd);
            list_destroy_and_destroy_elements(lista_paquete, free);
            break;
        }default: 
            log_warning(logger, "Warning: Operacion desconocida en atender_cliente, cod_op = %d", cod_op);
    }
    return NULL;
}

void atender_cpu(int cpu_fd){
    while(1){
        log_info(logger, "****Atendiendo CPU");
        op_code cod_op = recibir_operacion(cpu_fd);
        if(cod_op == -1){
            log_warning(logger, "se desconecto CPU");
            break;
        }
    }
    close(cpu_fd);
    log_info(logger, "cerrando hilo de CPU");
}

void atender_swap(int swap_fd){
    while(1){
        log_info(logger, "****Atendiendo al SWAP");
        op_code cod_op = recibir_operacion(swap_fd);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto SWAP, terminando todos los modulos: BSOD");
            break;
        }
        
    }
    log_info(logger, "cerrando hilo de swap");
}
void atender_scheduler(int sch_fd){
    log_info(logger, "****Atendiendo al SCHEDULER");
    while(1){
        op_code cod_op = recibir_operacion(sch_fd);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto scheduler");
            break;
        }
    }
    log_info(logger, "cerrando hilo de sch");
    return;
}

void atender_stick(int stick_fd){
    log_info(logger, "****Atendiendo al STIICK");

    while(1){
        op_code cod_op = recibir_operacion(stick_fd);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto stick");
            break;
        }
    };
    log_info(logger, "cerrando hilo de stick");
}