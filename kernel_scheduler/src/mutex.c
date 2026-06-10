#include "mutex.h"


t_mutex* m_create(char* nombre){
    static int proximo_mutex_id = 0;
    if(get_mutex(nombre)){
        return NULL; // significa que ya existe un mutex con este nombre, hay que informar el error a CPU
    }
    t_mutex* m = malloc(sizeof(t_mutex));
    m->nombre = nombre;
    m->mutex_id = proximo_mutex_id++;
    m->duenio = NULL;
    m->procesos_en_espera = list_create();
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

void m_signal(t_mutex* mutex, t_pcb* proceso){
    pthread_mutex_lock(&mutex->lock);

    if(mutex->duenio != proceso){
        log_debug(logger, "hizo signal un proc que no tenia asignado el mutex");
        pthread_mutex_unlock(&mutex->lock);
        return;
    }

    if(list_is_empty(mutex->procesos_en_espera)){
        mutex->duenio = NULL;
        pthread_mutex_unlock(&mutex->lock);
        return;
    }
    t_pcb* siguiente = list_remove(mutex->procesos_en_espera, 0);
    mutex->duenio = siguiente;
    desbloquear_proceso(siguiente);

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
    list_add(mutex->procesos_en_espera, proceso);

    log_info(logger, "## (<%d>) Bloqueado por Mutex <%s>", proceso->pid, mutex->nombre);
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