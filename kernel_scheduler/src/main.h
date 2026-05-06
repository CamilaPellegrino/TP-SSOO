#ifndef SCHEDULER_H_
#define SCHEDULER_H_
#include <utils/hello.h>
#include <stdbool.h>
#include "base_sch.h"
#include "planificador.h"
#include <pthread.h>
#include <semaphore.h>

// variables globales 

t_list* lista_cpus;
t_list* lista_io;
t_list* lista_new;
t_list* lista_ready;         // lista procesos en ready
t_list* lista_exec;          // lista de procesos en exec
t_list* lista_blocked;       // procesos en blocked
t_list* lista_susp_blocked;
t_list* lista_susp_ready;
t_log * logger;
t_planificacion algoritmo;

// tipos de datos
/*
 * Declaracion de funcioens
*/

// atender_x
void* atender_cliente(void *arg);
void* atender_cpu(t_cpu* cpu);
void* atender_io(t_io* io);
void enviar_paquete_a_todas_las_cpus(t_paquete* paquete);
void* atender_km(void *conexion_kernel_memory);

// otras
t_planificacion algoritmo_str_a_enum(char *algoritmo_str);

#endif  /* SCHEDULER_H_ */ 
