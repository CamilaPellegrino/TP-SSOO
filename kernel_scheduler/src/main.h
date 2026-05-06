#ifndef SCHEDULER_H_
#define SCHEDULER_H_
#include <utils/hello.h>
#include <utils/utils.h>
#include <stdbool.h>
#include "base_sch.h"
#include <pthread.h>
#include <semaphore.h>


// tipos de datos
/*
 * Declaracion de funcioens
*/
void* atender_cliente(void *arg);
void* atender_cpu(t_cpu* cpu);
void* atender_io(t_io* io);
void enviar_paquete_a_todas_las_cpus(t_paquete* paquete);
void* km_notif(void *conexion_kernel_memory);

// planificacion
bool planificador_corto_plazo();
void * planificador_largo_plazo();
t_cpu* proxima_cpu();
t_pcb* planificar_CMN();
t_pcb* planificar_RR_y_FIFO();
t_pcb* proximo_proceso();

// mover entre listas de procesos
void blocked_a_susp_blocked(t_pcb* pid);
void susp_blocked_a_susp_ready(t_pcb* pid);
void susp_ready_a_ready(t_pcb* pid);
void ready_a_exec(t_pcb* pid);
void exec_a_blocked(t_pcb* pid);
void exec_a_ready(t_pcb* pid);
void new_a_ready(t_pcb* pid);
t_list* sublista_ready_de_prioridad(int prioridad);

// funciones para inicializar el algoritmo de planificacion
t_planificacion algoritmo_str_a_enum(char *algoritmo_str);
void obtener_algoritmo_planificacion(char *algoritmo_str);

// funciones genericas
void pasarA(t_list* ,t_list*, t_pcb*, t_tipo_estado); // mover un pid de una lista a otra

// variables globales

t_log * logger;
t_list* lista_cpus;
t_list* lista_io;

t_list* lista_new;
t_list* lista_ready;     // lista procesos en ready
t_list* lista_exec;      // lista de procesos en exec
t_list* lista_blocked;   // procesos en blocked
t_list* lista_susp_blocked;
t_list* lista_susp_ready;

t_planificacion algoritmo;


#endif  /* SCHEDULER_H_ */ 
