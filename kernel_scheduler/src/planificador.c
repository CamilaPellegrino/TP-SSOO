#include "planificador.h"
void asignar_proceso(t_pcb* proceso, t_cpu* cpu);
t_cpu* cpu_a_desalojar_por_prioridad(t_pcb* p);
t_cpu* proxima_cpu_libre();

void* planificador_corto_plazo(){
    while(1){
        log_debug(logger, "esperando");
        sem_wait(&s_intentar_planificar);
        pthread_mutex_lock(&m_transicionar);
        log_debug(logger, "planificador_corto_plazo: despertado");
        t_pcb* proceso = proximo_proceso();
        if(proceso == NULL){
            log_debug(logger, "planificador_corto_plazo: no hay procesos en ready");
            pthread_mutex_unlock(&m_transicionar);
            continue;
        }
        pthread_mutex_lock(&m_lista_cpus);
        t_cpu* cpu = proxima_cpu_libre();
        if(cpu != NULL){
            pthread_mutex_unlock(&m_lista_cpus);
            log_debug(logger, "planificador_corto_plazo: asignando cpu %d a proceso %d", cpu->id, proceso->pid);
            asignar_proceso(proceso, cpu);
            pthread_mutex_unlock(&m_transicionar);
            continue;
        }
        pthread_mutex_unlock(&m_lista_cpus);
        if(algoritmo == CMN){
            t_cpu* cpu_desalojable = cpu_a_desalojar_por_prioridad(proceso);
            if(cpu_desalojable != NULL){
                pthread_mutex_lock(&m_lista_cpus);
                pthread_mutex_lock(&cpu_desalojable->mutex);
                if(!cpu_desalojable->desalojando && cpu_desalojable->proceso != proceso){
                    cpu_desalojable->desalojando = true;
                    enviar_operacion(cpu_desalojable->fd, SCH_CPU__DETENER_EJECUCION);
                    log_info(logger, "## desalojando cpu %d", cpu_desalojable->id);
                    agregar_a_ready_al_frente(proceso);
                    pthread_mutex_unlock(&cpu_desalojable->mutex);
                    pthread_mutex_unlock(&m_lista_cpus);
                    pthread_mutex_unlock(&m_transicionar);
                    continue;
                }
                pthread_mutex_unlock(&cpu_desalojable->mutex);
                pthread_mutex_unlock(&m_lista_cpus);
            }
        }
        agregar_a_ready_al_frente(proceso);
        pthread_mutex_unlock(&m_transicionar);
        continue;
    }
    return NULL;
}

t_cpu* proxima_cpu_libre(){
    for(int i = 0; i< list_size(lista_cpus); i++){
        t_cpu* c = list_get(lista_cpus, i);
        if(c->proceso == NULL){
            return c;
        }
    }
    return NULL;
}

t_cpu* cpu_a_desalojar_por_prioridad(t_pcb* p){
    t_cpu* peor_cpu = NULL;
    pthread_mutex_lock(&m_lista_cpus);
    for(int i = 0; i < list_size(lista_cpus); i++){
        t_cpu* c = list_get(lista_cpus, i);
        log_debug(logger, "analizando cpu de id %d", c->id);
        if(c->proceso == NULL){
            peor_cpu = NULL;
            break;
        }
        if(c->desalojando){
            continue;
        }
        if(c->proceso->prioridad_actual > p->prioridad_actual){
            if(peor_cpu == NULL || c->proceso->prioridad_actual > peor_cpu->proceso->prioridad_actual){
                peor_cpu = c;
            }
        }
    }
    pthread_mutex_unlock(&m_lista_cpus);
    log_debug(logger, "cpu_a_desalojar_por_prioridad finalizando");
    return peor_cpu;
}


void asignar_proceso(t_pcb* proceso, t_cpu* cpu){
    // marcar que la cpu esta ocupada 
    cpu->proceso = proceso;
    int pid = proceso->pid;

    // le asigno a prox_cpu el proceso prox_proceso mandandole el pid
    int cpu_fd = cpu->fd;
    t_paquete* paquete_asignar_proceso = crear_paquete(SCH_CPU__PID);         
    agregar_a_paquete(paquete_asignar_proceso, &pid, sizeof(pid));         
    enviar_paquete_y_liberarlo(paquete_asignar_proceso, cpu_fd);         
    log_debug(logger, "planificador_corto_plazo: Envie pid %d a cpu de fd %d", pid, cpu_fd); 

    agregar_proceso_a_lista(estado_exec->sublista, proceso, EJECUTANDO, &estado_exec->mutex);
    log_obligatorio_cambio_de_estado(proceso->pid, "READY", "EXEC");
    if(algoritmo_de_proceso(proceso) == RR){
        log_debug(logger, "creando hilo_fin_quantum para proc %d", pid);
        crear_hilo_o_exit(hilo_fin_quantum, cpu, "hilo_fin_quantum", logger);
    }
}

void* planificador_largo_plazo(){
    while(1){
        sem_wait(&s_nuevo_proceso_new);
        log_info(logger, "planificador_largo_plazo: ejecutando");
        pthread_mutex_lock(&estado_new->mutex);
        if(list_is_empty(estado_new->sublista)){
        pthread_mutex_unlock(&estado_new->mutex);
            log_warning(logger, "planificador_largo_plazo: no habia nada en new (raro que esto pase)");
            continue;
        }
        pthread_mutex_unlock(&estado_new->mutex);
        t_pcb* proceso_new = list_get(estado_new->sublista, 0);
        new_a_ready(proceso_new);
    }
}

void manejar_proceso_exit(t_pcb* proceso){
    exec_a_exit(proceso);
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
        if(cpu->proceso == NULL){
            ret = cpu;
            break;
        }
    }
    pthread_mutex_unlock(&m_lista_cpus);

    return ret;
}

// Problema: Las funciones de proximo_proceso tienen que devolver el prox_proc y sacarlo de la lista para no tener un TOCTOU


t_pcb* proximo_proceso(){ // no elimina el proceso de la lista, solamente devuelve el puntero al proxim oque hay que ejecutar
    if(estado_ready->sublista == NULL){
        return NULL;
    }
    t_pcb* pcb = NULL;
    switch(algoritmo){
        case CMN: {
            pcb = planificar_CMN();
            break;
        }
        case FIFO:
        case RR: {
            pcb = planificar_RR_y_FIFO();
            break;
        }default: 
            log_warning(logger, "Algoritmo desconocido, no se puede planificar");
            exit(EXIT_FAILURE);
    }
    return pcb;
}

t_pcb* planificar_CMN(){
    pthread_mutex_lock(&estado_ready->mutex);
    int size = list_size(estado_ready->sublista);
    for(int i = 0; i < size; i++){
        t_sublista_ready* actual = list_get(estado_ready->sublista, i);
        pthread_mutex_lock(&actual->mutex);
        if(!list_is_empty(actual->sublista)){
            t_pcb* proceso = list_remove(actual->sublista, 0);
            pthread_mutex_lock(&m_procesos_en_ready);
            procesos_en_ready--;
            pthread_mutex_unlock(&m_procesos_en_ready);        
            pthread_mutex_unlock(&actual->mutex);
            pthread_mutex_unlock(&estado_ready->mutex);
            return proceso;
        }
        pthread_mutex_unlock(&actual->mutex);
    }
    pthread_mutex_unlock(&estado_ready->mutex);

    return NULL;
}

void agregar_a_ready_al_frente(t_pcb* proceso){
    if(proceso == NULL){
        return;
    }
    switch(algoritmo){
        case FIFO:
        case RR: {
            pthread_mutex_lock(&estado_ready->mutex);
            list_add_in_index(estado_ready->sublista, 0, proceso);
            pthread_mutex_unlock(&estado_ready->mutex);
            break;
        }
        case CMN: {
            t_sublista_ready* sublista = sublista_ready_de_prioridad(proceso->prioridad_actual);
            pthread_mutex_lock(&estado_ready->mutex);
            pthread_mutex_lock(&sublista->mutex);

            list_add_in_index(sublista->sublista, 0, proceso);

            pthread_mutex_lock(&m_procesos_en_ready);
            procesos_en_ready++;
            pthread_mutex_unlock(&m_procesos_en_ready);

            pthread_mutex_unlock(&sublista->mutex);
            pthread_mutex_unlock(&estado_ready->mutex);
            break;
        }
        default:
            log_error(logger, "Algoritmo desconocido");
            exit(EXIT_FAILURE);
    }
}

t_pcb* planificar_RR_y_FIFO(){
    pthread_mutex_lock(&estado_ready->mutex);

    if(list_is_empty(estado_ready->sublista)){
        pthread_mutex_unlock(&estado_ready->mutex);
        return NULL;
    }
    t_pcb* proceso = list_remove(estado_ready->sublista, 0);

    pthread_mutex_unlock(&estado_ready->mutex);
    pthread_mutex_lock(&m_procesos_en_ready);
    procesos_en_ready--;
    pthread_mutex_unlock(&m_procesos_en_ready);
    return proceso;
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

    pthread_mutex_lock(&proceso->mutex);
    bool suspender = !evt->syscall_finalizada && proceso->estado == BLOQUEADO;
    pthread_mutex_unlock(&proceso->mutex);

    if(suspender){
        log_debug(logger, "syscall no finalizo a tiempo, suspendiendo");
        blocked_a_susp_blocked(proceso);
    }
    log_debug(logger, "hilo_timeout: finalizando");
    
    pthread_mutex_unlock(&evt->mutex);
    return NULL;
}

void* hilo_fin_quantum(void* arg){
    t_cpu* cpu = (t_cpu*) arg;
    if(cpu == NULL || cpu->proceso == NULL){ return NULL; }
    t_pcb* proceso = cpu->proceso;
    log_debug(logger, "iniciando hilo_fin_quantum para pid <%d>", proceso->pid);
    if(proceso == NULL){
        log_debug(logger, "hilo_fin_quantum: cpu libre, terminando este hilo");
        return NULL;
    }
    t_data_cond* data_cond = &proceso->data_cond;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    sumar_milisegundos(&ts, quantum);
    int rc = 0;

    pthread_mutex_lock(&data_cond->mutex_cond);
    while(!data_cond->cond_val && rc == 0){
        rc = pthread_cond_timedwait(&data_cond->cond, &data_cond->mutex_cond, &ts);
    }

    // timeout vencido o syscall finalizada
    log_debug(logger, "quantum vencido o proceso bloqueado antes del quantum");
    if(!data_cond->cond_val && proceso->estado == EJECUTANDO){
        log_debug(logger, "## (<%d>) - Desalojado por fin de quantum", proceso->pid);
        // exec_a_ready(proceso);
        // liberar_cpu(cpu); 
        enviar_operacion(cpu->fd, SCH_CPU__DETENER_EJECUCION);
    }
    data_cond->cond_val = false;
    pthread_mutex_unlock(&data_cond->mutex_cond);
    log_debug(logger, "hilo_fin_quantum: finalizando");

    return NULL;
}