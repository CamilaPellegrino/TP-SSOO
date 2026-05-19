#include "planificador.h"

void* planificador_corto_plazo(){
    while(1){
        log_debug(logger, "esperando cosas en corto plazo");
        sem_wait(&s_nueva_cpu_libre);
        log_debug(logger, "planificador_corto_plazo: cpu libre encontrada");
        t_cpu* prox_cpu = proxima_cpu();
        if(prox_cpu == NULL){
            log_debug(logger, "planificador_corto_plazo: no hay cpu, volviendo a wait");
            continue;
        }
        
        sem_wait(&s_nuevo_proceso_ready);
        log_debug(logger, "planificador_corto_plazo: ejecutando");
        
        t_pcb* prox_proceso = proximo_proceso();
        if(prox_proceso == NULL){
            log_debug(logger, "planificador_corto_plazo: no encontre proceso, volviendo a wait. Probablemente hubo un sem_post de mas");
            continue;
        }
        int pid = prox_proceso->pid;

        // marcar que la cpu esta ocupada 
        prox_cpu->proceso = prox_proceso;

        // le asigno a prox_cpu el proceso prox_proceso mandandole el pid
        int cpu_fd = prox_cpu->fd;
        t_paquete* paquete_asignar_proceso = crear_paquete(SCH_CPU__PID);         
        agregar_a_paquete(paquete_asignar_proceso, &pid, sizeof(pid));         
        enviar_paquete(paquete_asignar_proceso, cpu_fd);         
        log_debug(logger, "planificador_corto_plazo: Envie pid %d a cpu de fd %d", pid, cpu_fd); 
	    log_debug(logger, "ENVIO OP=%d SIZE=%d", paquete_asignar_proceso->codigo_operacion, paquete_asignar_proceso->buffer->size);

        ready_a_exec(prox_proceso);
    }
    return NULL;
}

void * planificador_largo_plazo(){
    while(1){
        sem_wait(&s_nuevo_proceso_new);
        log_info(logger, "planificador_largo_plazo: ejecutando");
        if(list_is_empty(lista_new)){
            log_warning(logger, "planificador_largo_plazo: no habia nada en new (raro que esto pase)");
            continue;
        }
        t_pcb* proceso_new = list_get(lista_new, 0);
        new_a_ready(proceso_new);
        log_debug(logger, "cosas en new ahora: %d, cosas en ready: %d", list_size(lista_new), list_size(lista_ready));
    }
}

void manejar_proceso_exit(t_pcb* proceso){
    eliminar_de_exec(proceso);
    loguear_tamanio_listas_de_estado();
    // t_paquete* paquete_fin_proc = crear_paquete(SCH_KM__EXIT);
    // agregar_a_paquete(paquete_fin_proc, &proceso->pid, sizeof(proceso->pid));
    // enviar_paquete_y_liberarlo(paquete_fin_proc, conexion_kernel_memory); // TODO: enviar el paquete a km y que km avise cuando libero todo 
}

t_cpu* proxima_cpu()
{ // no saca la cpu de la lista, solamente devuelve el puntero ala cpu
    t_cpu* ret = NULL;
    pthread_mutex_lock(&m_lista_cpus);
    for(int i=0; i<list_size(lista_cpus); i++){
        t_cpu* cpu = list_get(lista_cpus, i);
        if(cpu->proceso == NULL)
            ret = cpu;
    }
    pthread_mutex_unlock(&m_lista_cpus);

    return ret;
}

// devuelve el pcb dep proximo proceso a ejecutar si el algoritmo es CMN
t_pcb* planificar_CMN(){
    pthread_mutex_lock(&m_lista_ready);
    for(int i = 0; i < list_size(lista_ready); i++){
        t_list* actual = list_get(lista_ready, i);
        
        if(!list_is_empty(actual)){
            t_pcb* proceso = list_get(actual, 0);
            pthread_mutex_unlock(&m_lista_ready);
            return proceso;
        }
    }
    pthread_mutex_unlock(&m_lista_ready);
    return NULL;
}

// para FIFO y RR
t_pcb* planificar_RR_y_FIFO(){
    pthread_mutex_lock(&m_lista_ready);
    if(list_is_empty(lista_ready)){
        pthread_mutex_unlock(&m_lista_ready);
        return NULL;
    }
    t_pcb* proceso = list_get(lista_ready, 0);
    pthread_mutex_unlock(&m_lista_ready);
    return proceso;
}

t_pcb* proximo_proceso(){ // no elimina el proceso de la lista, solamente devuelve el puntero al proxim oque hay que ejecutar
    switch(algoritmo){
        case CMN: {
            return planificar_CMN();
        }
        case FIFO:
        case RR: {
            return planificar_RR_y_FIFO();
        }default: 
            log_warning(logger, "Algoritmo desconocido, no se puede planificar");
            exit(EXIT_FAILURE);
            return NULL;
    }
}

void* hilo_timeout(void* arg){
    t_evt* evt = (t_evt*)arg;
    t_pcb* proceso = evt->proceso;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    sumar_milisegundos(&ts, suspension_timeout);
    int rc = 0;

    pthread_mutex_lock(&evt->mutex);
    while(!evt->syscall_finalizada && rc == 0){
        rc = pthread_cond_timedwait(&evt->cond, &evt->mutex, &ts);
    }

    // timeout vencido o syscall finalizada
    log_debug(logger, "timeout vencido o syscall finalizada");
    if(!evt->syscall_finalizada && proceso->estado == BLOQUEADO){
        log_debug(logger, "syscall no finalizo a tiempo, suspendiendo");
        blocked_a_susp_blocked(proceso);
    }
    log_debug(logger, "hilo_timeout: finalizando");
    
    pthread_mutex_unlock(&evt->mutex);
    return NULL;
}