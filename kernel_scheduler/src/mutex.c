#include "mutex.h"
void insertar_por_prioridad(t_list* l, t_pcb* p);
void heredar_prioridad(t_pcb* d, t_pcb* w, t_list* visitados);
void recalcular_prioridad(t_pcb* proc);
bool ya_visitado(t_list* l, t_pcb* p);
t_pcb* prox_duenio_mutex(t_list* espera);

t_mutex* m_create(char* nombre){
    static int proximo_mutex_id = 0;
    if(get_mutex(nombre)){
        return NULL;
    }
    t_mutex* m = malloc(sizeof(t_mutex));
    m->nombre = strdup(nombre);
    m->mutex_id = proximo_mutex_id++;
    m->duenio = NULL;
    m->procesos_en_espera = list_create();
    pthread_mutex_init(&m->lock, NULL);
    pthread_mutex_lock(&m_lista_mutex);
    list_add(lista_mutex, m);
    pthread_mutex_unlock(&m_lista_mutex);

    return m;
}

void m_signal(t_mutex* mutex, t_pcb* proceso){
    pthread_mutex_lock(&mutex->lock);

    if(mutex->duenio != proceso){
        log_debug(logger, "hizo signal un proc que no tenia asignado el mutex");
        pthread_mutex_unlock(&mutex->lock);
        return;
    }

    t_pcb* siguiente = prox_duenio_mutex(mutex->procesos_en_espera);
    if(siguiente == NULL){
        log_debug(logger, "Nadie esperando mutex %s", mutex->nombre);
        mutex->duenio = NULL;
        pthread_mutex_unlock(&mutex->lock);
        return;
    }
    t_evt* evt = get_evt_de_proceso(siguiente);
    if(evt != NULL){
        finalizar_evento(evt);
        liberar_evt(evt, free);
    }
    mutex->duenio = siguiente;
    siguiente->mutex_esperado = NULL;
    desbloquear_proceso(siguiente);

    recalcular_prioridad(proceso);

    log_info(logger, "## (<%d>) Toma el Mutex <%s>", siguiente->pid, mutex->nombre);

    pthread_mutex_unlock(&mutex->lock);
}

bool m_wait(t_mutex* mutex, t_pcb* proceso){
    pthread_mutex_lock(&mutex->lock);
    if(mutex->duenio == NULL){
        mutex->duenio = proceso;
        log_info(logger, "## (<%d>) Toma el Mutex <%s>", proceso->pid, mutex->nombre);
        pthread_mutex_unlock(&mutex->lock);
        return true;
    }

    log_info(logger, "## (<%d>) Bloqueado por Mutex <%s>", proceso->pid, mutex->nombre);
    proceso->mutex_esperado = mutex;
    
    // insertar_por_prioridad(mutex->procesos_en_espera, proceso);
    list_add(mutex->procesos_en_espera, proceso);
    t_list* visitados = list_create();
    heredar_prioridad(mutex->duenio, proceso, visitados);
    list_destroy(visitados);

    pthread_mutex_unlock(&mutex->lock); 
    
    return false;
}

t_mutex* get_mutex(char* nombre){
    pthread_mutex_lock(&m_lista_mutex);

    t_mutex* encontrado = NULL;

    for(int i = 0; i < list_size(lista_mutex); i++){
        t_mutex* mutex = list_get(lista_mutex, i);

        if(strcmp(mutex->nombre, nombre) == 0){
            encontrado = mutex;
            break;
        }
    }
    pthread_mutex_unlock(&m_lista_mutex);

    return encontrado;
}

t_pcb* prox_duenio_mutex(t_list* espera) {
    int indice_mejor = -1;
    t_pcb* mejor = NULL;

    for (int i = 0; i < list_size(espera); i++) {
        t_pcb* pcb = list_get(espera, i);

        // if (get_estado(pcb) != BLOQUEADO){
        //     continue;
        // }
        if (mejor == NULL || get_prioridad_actual_thread_safe(pcb) > get_prioridad_actual_thread_safe(mejor)) {
            mejor = pcb;
            indice_mejor = i;
        }
    }
    if(indice_mejor == -1){
        return NULL;
    }
    return list_remove(espera, indice_mejor);
}

void insertar_por_prioridad(t_list* l, t_pcb* p){
    if(algoritmo != CMN){
        log_debug(logger, "Insertado <%d>, tam: %d", p->pid, list_size(l));
        list_add(l, p);
        return;
    }
    for(int i = 0; i<list_size(l); i++){
        t_pcb* x = list_get(l, i);
        if(x->prioridad_actual < p->prioridad_actual){
            log_debug(logger, "Insertado <%d>, tam: %d", p->pid, list_size(l));
            list_add_in_index(l, i, p);
            return;
        }
    }
    list_add(l, p);
}

void heredar_prioridad(t_pcb* d, t_pcb* w, t_list* visitados){
    if(ya_visitado(visitados, d)){
        return;
    }

    list_add(visitados, d);

    if(w->prioridad_actual >= d->prioridad_actual){
        return;
    }
    cambiar_prioridad(d, w->prioridad_actual);

    if(d->mutex_esperado != NULL && d->mutex_esperado->duenio != NULL){
        t_mutex* m = d->mutex_esperado;
        heredar_prioridad(m->duenio, d, visitados);
    }
}

void recalcular_prioridad(t_pcb* proc){
    int prox_prior = proc->prioridad_base;

    for(int i = 0; i<list_size(lista_mutex); i++){
        t_mutex* m = list_get(lista_mutex, i);
        log_debug(logger, "heredar_prioridad: Analizando mutex: %s", m->nombre);
        if(m->duenio == proc){
            for(int j = 0; j<list_size(m->procesos_en_espera); j++){
                t_pcb* x = list_get(m->procesos_en_espera, j);
                if(x->prioridad_actual < prox_prior){
                    prox_prior = x->prioridad_actual;
                }
            }
        }
    }
    if(proc->prioridad_actual != prox_prior){
        cambiar_prioridad(proc, prox_prior);
    }
}

bool ya_visitado(t_list* l, t_pcb* p){
    for(int i = 0; i<list_size(l); i++){
        t_pcb* x = list_get(l, i);
        if(x->pid == p->pid){
            return true;
        }
    }
    return false;
}