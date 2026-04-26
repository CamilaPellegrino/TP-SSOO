#include <utils/hello.h>
#include <utils/utils.h>

void* atender_cliente(void *arg);
void atender_cpu(int cpu_fd);
void atender_io(int io_fd);
void enviar_paquete_a_todas_las_cpus(t_paquete* paquete);
void* km_notif(int *conexion_kernel_memory);
t_log *logger;
t_list* lista_cpus;


int main(int argc, char* argv[]) {
    // ejemplo para ejecutar: ./bin/kernel_scheduler ./kernel_scheduler.config
    if(argc < 2){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/kernel_scheduler ./kernel_scheduler.config");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];
    
    saludar("kernel_scheduler");

    
    t_config *config;
    
    char *ip;
    char *puerto_kernel_memory;
    char *puerto_kernel_scheduler;

    // crear logger
    logger = iniciar_logger("kernel_scheduler.log", "ProcesoKernelScheduler", LOG_LEVEL_INFO );
    
    // crear config
    config = iniciar_config(ruta_config); 
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }
    // conectar a kernel memory
    ip = config_get_string_value (config, "IP");
    puerto_kernel_memory = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");

    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    log_info(logger, "## Conectado a Kernel Memory");

    
    // handshake a kernel memory
    handshake_cliente(conexion_kernel_memory, logger);
    // enviar mensaje a kernel memory
    enviar_mensaje("Hola, soy sche", conexion_kernel_memory, SCH_KM__CONEXION);
    
    //ACA DEBERIAMOS CREAR EL HILO PARA ATENDER A KERNEL MEMORY, PERO COMO NO SE ESPECIFICA NINGUNA OPERACION QUE DEBA REALIZAR, LO DEJAMOS ASI POR AHORA
    pthread_t thread_km;
    pthread_create(&thread_km, NULL, km_notif , &conexion_kernel_memory);
    
    // iniciar servidor
    puerto_kernel_scheduler = config_get_string_value (config, "PUERTO_KERNEL_SCHEDULER");
    int kernel_scheduler_fd = iniciar_servidor(puerto_kernel_scheduler);

    if(kernel_scheduler_fd == -1){
        log_error(logger, "No se pudo iniciar el servidor");
        exit(EXIT_FAILURE);
    }

    // incializar listas 

    lista_cpus = list_create();

    // esperar clientes
    while(true){
        int *cliente_fd = esperar_cliente(kernel_scheduler_fd);
        log_info(logger, "Me llego un cliente, %d", *cliente_fd);

        handshake_servidor(*cliente_fd, logger);
        // creacion del hilo
        pthread_t thread;
        pthread_create(&thread, NULL, atender_cliente, cliente_fd);
        pthread_detach(thread);
    }

    return 0;
}


void* atender_cliente(void *arg){
    int *cliente_fd_ptr = (int *) arg;
    int cliente_fd = *cliente_fd_ptr;
    op_code cod_op = recibir_operacion(cliente_fd);
    switch(cod_op){
        case CPU_SCH__CONEXION: {
            char *msg = recibir_mensaje(cliente_fd);

            log_info(logger, "Me llego el CPU, mensaje recibido: %s", msg);
            t_cpu* cpu = iniciar_cpu(0, cliente_fd); // le hardcodeo el 0 por ahora, despues cpu se lo manda
            list_add(lista_cpus, cpu); // la agrego a la lista
            atender_cpu(cliente_fd);
            break;
            
        }case IO_SCH__CONEXION: {
            log_info(logger, "stick");
            char *msg = recibir_mensaje(cliente_fd);
            log_info(logger, "Me llego stick, msg: %s", msg);
            atender_io(cliente_fd);

            break;
        }default: 
            log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
    }
}


void atender_cpu(int cpu_fd){
    log_info(logger, "## CPU <ID CPU> Conectada");
    while(1){
        op_code cod_op = recibir_operacion(cpu_fd);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto cpu");
            break;
        }
    }
}

void atender_io(int io_fd){
    log_info(logger, "****Atendiendo al io");
    while(1){
        op_code cod_op = recibir_operacion(io_fd);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto io");
        }
    }
}

void* km_notif(int *conexion_kernel_memory){
    while(1){
        op_code cod_op = recibir_operacion(*conexion_kernel_memory);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto kernel memory");
            break;
        }

        switch(cod_op){
            case KM_SCH__NUEVO_STICK: {
                t_list* lista_paquete = recibir_paquete(*conexion_kernel_memory);
                log_info(logger, "KM me dijo que se conecto una nueva stick");
                char* ip_stick = list_get(lista_paquete, 0);
                char* puerto_stick = list_get(lista_paquete, 1);
                int *tamanio_stick = list_get(lista_paquete, 2);
                list_destroy(lista_paquete);
                t_paquete* paquete = crear_paquete(SCH_CPU__NUEVO_STICK);
                agregar_string_a_paquete(paquete, ip_stick);
                agregar_string_a_paquete(paquete, puerto_stick);
                agregar_a_paquete(paquete, tamanio_stick, sizeof(int));
                enviar_paquete_a_todas_las_cpus(paquete);

                break;
            }default: 
                log_warning(logger, "Warning: Operacion desconocida, cod_op = %d",cod_op);
        }
    }
    return NULL;
}

void enviar_paquete_a_todas_las_cpus(t_paquete* paquete){
    log_info(logger,"%d", list_size(lista_cpus));
    for(int i = 0; i < list_size(lista_cpus); i++){
        
        t_cpu* cpu = list_get(lista_cpus, i);
        int cpu_fd = cpu->fd;
        log_info(logger,"%d", cpu_fd);
        enviar_paquete(paquete, cpu_fd);
        log_info(logger, "Enviando info a cpu de stick");
    }
}