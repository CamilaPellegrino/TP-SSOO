#include "main.h"
#include <utils/hello.h>
#include <utils/utils.h>
#include "base_sch.h"

void* atender_cliente(void *arg);
void* atender_cpu(t_cpu* cpu);
void* atender_io(t_io* io);
void enviar_paquete_a_todas_las_cpus(t_paquete* paquete);
void* km_notif(void *conexion_kernel_memory);

t_log *logger;
t_list* lista_cpus;
t_list* lista_io;

t_list* lista_ready;     // lista procesos en ready
t_list* lista_blocked;   // procesos en blocked
t_list* lista_susp_blocked;
t_list* lista_susp_ready;

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

    ip = config_get_string_value (config, "IP");
    puerto_kernel_memory = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    
    obtener_algoritmo_planificacion(config_get_string_value(config, "PLANIFICATION_ALGORITHM"));

    // conectar a kernel memory

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

    // incializar listas globales 
    lista_io = list_create();
    lista_cpus = list_create();
    lista_ready = list_create();
    lista_blocked = list_create();

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

void* km_notif(void *conexion_kernel_memory_v){
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

// cuando intentar planificar?

// nuevo proceso en la lista de ready: (susp/ready->ready) un proceso xq hay otra mem stick
// nueva cpu en lista de cpus: conexion de cpu

// planificacion
bool intentar_planificar(){
    t_pcb_sch* proceso = proximo_proceso();
    
    if(proceso == NULL){
        return false;
    }
    int pid = proceso->pid;
    t_cpu* prox_cpu = proxima_cpu();
    if(prox_cpu == NULL){
        return false;
    }
    
    ready_a_exec(proceso);

    // marcar que la cpu esta ocupada ?
    // ...

    // le asigno a prox_cpu el proceso prox_proceso
    int cpu_fd = prox_cpu->fd;
    t_paquete* paquete_asignar_proceso = crear_paquete(SCH_CPU__PID);
    agregar_a_paquete(paquete_asignar_proceso, &pid, sizeof(pid));
    enviar_paquete_y_liberarlo(paquete_asignar_proceso, cpu_fd);

    return true;
}

t_cpu* proxima_cpu(){
    if(list_is_empty(lista_cpus)){
        return NULL;
    }
    return list_get(lista_cpus, 0);
}
t_pcb_sch* proximo_proceso(){
    switch(algoritmo){
        case CMN: {
            return planificar_CMN();
        }
        case FIFO:
        case RR: {
            return planificar_RR_y_FIFO();
        }default: 
            log_warning(logger, "Algoritmo desconocido, no se puede planificar");
            // terminar programa ?
            return NULL;
    }
}

// devuelve el pcb dep proximo proceso a ejecutar si el algoritmo es CMN
t_pcb_sch* planificar_CMN(){
    for(int i=0; i<list_size(lista_ready); i++){
        t_list *actual = list_get(lista_ready, i);
        if(!list_is_empty(actual)){
            return list_get(actual, 0); // Devuelve el 1er elemento de la cola de mayor nivel que no este vacia
        }
    }
    return NULL;
}

// para FIFO y RR
t_pcb_sch* planificar_RR_y_FIFO(){
    if(list_is_empty(lista_ready)){
        return NULL;
    }
    return list_get(lista_ready, 0);
}

// mover proceso entre estados

void blocked_a_susp_blocked(t_pcb_sch* proceso){
    pasarA(lista_blocked, lista_susp_blocked, proceso);
}

void susp_blocked_a_susp_ready(t_pcb_sch* proceso){
    pasarA(lista_susp_blocked, lista_susp_ready, proceso);
}

void susp_ready_a_ready(t_pcb_sch* proceso){
    if(algoritmo == CMN){
        list_remove_element(lista_susp_ready, proceso);
        // agregar_a_ready_CMN(proceso);
    }else{
        pasarA(lista_susp_ready, lista_ready, proceso);
    }
}

void ready_a_exec(t_pcb_sch* proceso){
    pasarA(lista_ready, lista_exec, proceso);
}
void exec_a_blocked(t_pcb_sch* proceso){
    pasarA(lista_exec, lista_blocked, proceso);
}
void exec_a_ready(t_pcb_sch* proceso){
    if(algoritmo == CMN){
        list_remove_element(lista_exec, proceso);
        // agregar_a_ready_CMN(proceso);
    }else{
        pasarA(lista_exec, lista_ready, proceso);
    }
}

void new_a_ready(t_pcb_sch* proceso){
    if(algoritmo == CMN){
        //agregar_a_ready_CMN(proceso);
    }else{
        pasarA(lista_exec, lista_ready, proceso);
    }
}

void pasarA(t_list* lsrc,t_list*ldest, t_pcb_sch* proceso){
    list_remove_element(lsrc, proceso);
    list_add(ldest, proceso);
}

t_list* sublista_ready_de_prioridad(int prioridad){
    // si es CMN lista_ready tiene sublistas, deuelve la sublista de la prioridad pedida
    return list_get(lista_ready, prioridad);
}

void * planificador_largoPlazo(){
    
    //intentar_planificar();

    return NULL;
}

void obtener_algoritmo_planificacion(char *algoritmo_str){
    algoritmo = algoritmo_str_a_enum(algoritmo_str);
    if(algoritmo == -1){
        printf("%s no es un algoritmo invalido, proba con FIFO, RR o CMN", algoritmo_str);
        exit(EXIT_FAILURE);
    }
}

t_planificacion algoritmo_str_a_enum(char *algoritmo_str){
    if(strcmp(algoritmo_str, "FIFO") == 0){
        return FIFO;
    }
    if(strcmp(algoritmo_str, "RR") == 0){
        return RR;
    }
    if(strcmp(algoritmo_str, "CMN") == 0){
        return CMN;
    }
    return -1;
}