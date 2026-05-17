#ifndef MUTEX_H_
#define MUTEX_H_

#include "base_sch.h"

// tipo de semaforo mutex que uso para simular semaforos de los procesos

t_mutex* m_create(char* nombre);
void m_signal(t_mutex* mutex);
bool m_wait(t_mutex* mutex, t_pcb* proceso);
t_mutex* get_mutex(char* nombre);
#endif /* MUTEX_H_ */