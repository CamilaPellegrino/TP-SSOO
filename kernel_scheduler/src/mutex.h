#ifndef MUTEX_H_
#define MUTEX_H_

#include "base_sch.h"

// tipo de semaforo mutex que uso para simular semaforos de los procesos
typedef struct{
    char* nombre;
    int mutex_id;
    int valor;
    t_list* procesos_en_espera; // procesos que esperan por el mutex, en orden
    pthread_mutex_t lock;
} t_mutex;

t_mutex* m_create(char* nombre);
void m_signal(t_mutex* mutex);
bool m_wait(t_mutex* mutex, t_pcb* proceso);

#endif /* MUTEX_H_ */