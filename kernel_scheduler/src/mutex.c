#include "mutex.h"


t_mutex* m_create(char* nombre){
    static int proximo_mutex_id = 0;
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
    pthread_mutex_lock(&mutex->lock);
    mutex->valor--;
    if(mutex->valor < 0){
        list_add(mutex->procesos_en_espera, proceso);
        pthread_mutex_unlock(&mutex->lock);
        return false; // bloqueado
    }
    pthread_mutex_unlock(&mutex->lock);
    return true; // continúa ejecutando
}

