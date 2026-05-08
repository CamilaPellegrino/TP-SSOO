#include "main.h"

int main(int argc, char* argv[]) {
    if(argc < 3){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/kernel_scheduler ./kernel_scheduler.config archivo.txt");
        exit(EXIT_FAILURE);
    }

    saludar("kernel_scheduler");
    
    char *ruta_config = argv[1];
    char *instrucciones_pid_0 = argv[2];

    t_config *config = iniciar_config(ruta_config); 

    // leer de config
    char* ip = config_get_string_value (config, "IP");
    char* puerto_kernel_memory = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    char* puerto_kernel_scheduler = config_get_string_value (config, "PUERTO_KERNEL_SCHEDULER");
    char* algoritmo_str = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
    t_log_level log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));
    // char* queues_algorithms = config_get_array_value(config, "QUEUES_ALGORITHMS");
    // int rr_quantum = config_get_int_value(config, "RR_QUANTUM");

    // crear logger
    logger = iniciar_logger("kernel_scheduler.log", "ProcesoKernelScheduler", log_level);

    // inicializar variables globales
    inicializar_variables_globales();
    algoritmo = obtener_algoritmo_planificacion(algoritmo_str);

    // testear(); // descomentar esto si solo queres testear y defini el test en test.c

    // conectar a kernel memory
    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    log_info(logger, "## Conectado a Kernel Memory");
    
    // handshake a kernel memory
    handshake_cliente(conexion_kernel_memory, logger);

    // enviar mensaje a kernel memory
    enviar_mensaje("Hola, soy sche", conexion_kernel_memory, SCH_KM__CONEXION);
    
    pthread_t thread_km = crear_hilo_o_exit(atender_km, &conexion_kernel_memory, "thread_km");
    pthread_detach(thread_km);
    // iniciar servidor
    int kernel_scheduler_fd = iniciar_servidor_o_exit(puerto_kernel_scheduler); 

    // crear hilos de planificacion

    pthread_t hilo_largo_plazo = crear_hilo_o_exit(planificador_largo_plazo, NULL, "hilo_largo_plazo");
    pthread_detach(hilo_largo_plazo);
    pthread_t hilo_corto_plazo = crear_hilo_o_exit(planificador_corto_plazo, NULL, "hilo_corto_plazo");
    pthread_detach(hilo_corto_plazo);

    // pthread_create(&hilo_mediano_plazo, NULL, planificador_mediano_plazo, NULL);

    log_debug(logger, "hilos de planificacion creados\n");

    // agregar el proceso de pid 0
    t_pcb* pcb_pid_0 = nuevo_proc(0, 0, instrucciones_pid_0); //TODO: mandar la ruta de las instrucciones junto al PID al kernel memory para que las guarde
    
    // esperar clientes
    while(true){
        int *cliente_fd = esperar_cliente(kernel_scheduler_fd);
        log_info(logger, "Me llego un cliente, %d", *cliente_fd);

        handshake_servidor(*cliente_fd, logger);
        
        // crear del hilo que atiende al cliente que se acaba de conectar
        pthread_t thread;
        pthread_create(&thread, NULL, atender_cliente, cliente_fd);
        pthread_detach(thread);
    }

    return 0;
}


void* atender_cliente(void *arg){
    int *cliente_fd_ptr = (int *) arg;
    int cliente_fd = *cliente_fd_ptr;
    free(cliente_fd_ptr);
    op_code cod_op = recibir_operacion(cliente_fd);
    switch(cod_op){
        case CPU_SCH__CONEXION: {
            t_list* lista_paquete = recibir_paquete(cliente_fd);
            int* id_cpu = list_get(lista_paquete, 0);
            t_cpu* cpu = iniciar_cpu(*id_cpu, cliente_fd); 
            list_add(lista_cpus, cpu); // la agrego a la lista
            atender_cpu(cpu);
            break;
            
        }case IO_SCH__CONEXION: { 
            t_list *lista_paquete = recibir_paquete(cliente_fd);   // recibo un paquete con el tipo de io
            t_tipo_io *tipo_io = list_get(lista_paquete, 0);
            log_info(logger, "Me llego io de tipo: %d", *tipo_io);
            t_io* io = iniciar_io(*tipo_io, cliente_fd);

            list_add(lista_io, io);

            atender_io(io);
            break;
        }default: 
            log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
    }
    return NULL;
}


void* atender_cpu(t_cpu* cpu){
    int cpu_fd = cpu->fd;
    int cpu_id = cpu->id;
    log_info(logger, "## CPU <%d> Conectada", cpu_id);
    while(1){
        op_code cod_op = recibir_operacion(cpu_fd);
        if(cod_op == -1){
            log_warning(logger, "Se desconecto cpu de id:%d", cpu_id);
            break;
        }
    }
    return NULL;
}

void* atender_io(t_io* io){
    int io_fd = io->fd;
    t_tipo_io tipo = io->tipo;

    log_info(logger, "** Atendiendo al io de tipo %d", tipo);

    while(1){
        op_code cod_op = recibir_operacion(io_fd);
        if(cod_op == -1){
            log_warning(logger, "Desconexion de io de tipo %d", tipo);
        }
    }
    return NULL;
}

void* atender_km(void *conexion_kernel_memory_v){
    int *conexion_kernel_memory = (int*) conexion_kernel_memory_v; 
    while(1){
        op_code cod_op = recibir_operacion(*conexion_kernel_memory);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto kernel memory");
            break;
        }

        switch(cod_op){
            case KM_SCH__NUEVO_STICK: {
                log_info(logger, "Conexion de modulo stick");
                t_list* lista_paquete = recibir_paquete(*conexion_kernel_memory);
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
            }case KM_SCH__BSOD: {
                log_error(logger, "BSOD, cerrando todo");
                exit(EXIT_FAILURE);
            
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

// cuando intentar planificar al corto?

// nuevo proceso en la lista de ready: (susp/ready->ready) un proceso xq hay otra mem stick
// nueva cpu en lista de cpus: conexion de cpu

/*
sem_init(&semA, 0, 1); 
sem_wait(&semA);
sem_post(&semR);
sem_t semPC;

join o detach?
*/

