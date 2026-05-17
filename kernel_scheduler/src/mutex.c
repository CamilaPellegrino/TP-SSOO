#include "mutex.h"


t_mutex* m_create(char* nombre){
    static int proximo_mutex_id = 0;
    if(get_mutex(nombre)){
        return NULL; // significa que ya existe un mutex con este nombre, hay que informar el error a CPU
    }
    t_mutex* m = malloc(sizeof(t_mutex));
    m->nombre = nombre;
    m->mutex_id = proximo_mutex_id++;
    m->valor = 1;
    m->procesos_en_espera = list_create();
    pthread_mutex_init(&m->lock, NULL);
    return m;
}

void m_signal(t_mutex* mutex){
    pthread_mutex_lock(&mutex->lock);
    mutex->valor++;
    if(mutex->valor <= 0){
        t_pcb* proceso = list_remove(mutex->procesos_en_espera, 0);
        desbloquear_proceso(proceso);
    }
    pthread_mutex_unlock(&mutex->lock);
}

bool m_wait(t_mutex* mutex, t_pcb* proceso){
    log_debug(logger, "m_wait: iniciando");
    pthread_mutex_lock(&mutex->lock);
    mutex->valor--;
    if(mutex->valor < 0){
        log_debug(logger, "m_wait: mutex no disponible, encolando proceso");
        list_add(mutex->procesos_en_espera, proceso);
        pthread_mutex_unlock(&mutex->lock);
        return false; // bloqueado
    }
    pthread_mutex_unlock(&mutex->lock);
    return true; // continua ejecutando
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