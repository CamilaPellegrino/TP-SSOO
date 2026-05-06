#include "planificador.h"

bool planificador_corto_plazo(){
    t_pcb* proceso = proximo_proceso();
    
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


void * planificador_largo_plazo(){
    while(1){
        t_pcb* proceso_new = list_get(lista_new, 0);
        new_a_ready(proceso_new);
        return NULL;
    }
}

t_cpu* proxima_cpu(){
    for(int i=0; i<list_size(lista_cpus); i++){
        t_cpu* cpu = list_get(lista_cpus, i);
        if(cpu->proceso != NULL)
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

t_pcb* proximo_proceso(){
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