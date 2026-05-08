#include "planificador.h"

void* planificador_corto_plazo(){
    while(1){
        sem_wait(&s_planificar_corto);
        log_debug(logger, "Planificador de corto plazo hizo sem_wait");
        t_cpu* prox_cpu = proxima_cpu();
        if(prox_cpu == NULL){
            log_debug(logger, "no encontre cpu, volviendo a wait");
            continue;
        }
        t_pcb* prox_proceso = proximo_proceso();
        
        if(prox_proceso == NULL){
            log_debug(logger, "no encontre proceso, volviendo a wait");
            continue;
        }
        int pid = prox_proceso->pid;

        // marcar que la cpu esta ocupada 
        prox_cpu->proceso = prox_proceso;

        // le asigno a prox_cpu el proceso prox_proceso mandandole el pid
        int cpu_fd = prox_cpu->fd;
        log_debug(logger, "Enviando cosas a cpu de fd: %d", cpu_fd);
        t_paquete* paquete_asignar_proceso = crear_paquete(SCH_CPU__PID);
        agregar_a_paquete(paquete_asignar_proceso, &pid, sizeof(pid));
        enviar_paquete(paquete_asignar_proceso, cpu_fd);
        log_debug(logger, "voy a enviar pid %d a fd %d", pid, cpu_fd);
        ready_a_exec(prox_proceso);
    }
    return NULL;
}


void * planificador_largo_plazo(){
    while(1){
        sem_wait(&s_planificar_largo);
        log_info(logger, "Planificador de largo plazo hizo sem_wait");
        if(list_is_empty(lista_new)){
            log_debug(logger, "no habia nada en new (raro que esto pase, si pasa es que sobra un sem_post en algun lado");
            continue;
        }
        t_pcb* proceso_new = list_remove(lista_new, 0);
        new_a_ready(proceso_new);
        log_debug(logger, "cosas en new ahora: %d, cosas en ready: %d", list_size(lista_new), list_size(lista_ready));
        sem_post(&s_planificar_corto);
    }
}

t_cpu* proxima_cpu()
{ // no saca la cpu de la lista, solamente devuelve el puntero ala cpu
    for(int i=0; i<list_size(lista_cpus); i++){
        t_cpu* cpu = list_get(lista_cpus, i);
        if(cpu->proceso == NULL)
            return cpu;
    }
    return NULL;
}


// devuelve el pcb dep proximo proceso a ejecutar si el algoritmo es CMN
t_pcb* planificar_CMN(){
    for(int i=0; i<list_size(lista_ready); i++){
        t_list *actual = list_get(lista_ready, i);
        if(!list_is_empty(actual)){
            return list_get(actual, 0); // Devuelve el 1er elemento de la cola de mayor nivel que no este vacia
        }
    }
    return NULL;
}

// para FIFO y RR
t_pcb* planificar_RR_y_FIFO(){
    if(list_is_empty(lista_ready)){
        return NULL;
    }
    return list_get(lista_ready, 0);
}

t_pcb* proximo_proceso()
{ // no elimina el proceso de la lista, solamente devuelve el puntero al proxim oque hay que ejecutar
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