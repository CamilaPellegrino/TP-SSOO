#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_
#include "base_sch.h"
#include <time.h>

// planificacion
void* planificador_corto_plazo();
void* planificador_largo_plazo();
void manejar_proceso_exit(t_pcb* proceso);
t_cpu* proxima_cpu();
t_pcb* planificar_CMN();
t_pcb* planificar_RR_y_FIFO();
t_pcb* proximo_proceso();
void* hilo_timeout(void* arg);
void* hilo_fin_quantum(void* arg);
#endif /* PLANIFICADOR_H_*/