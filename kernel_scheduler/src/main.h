#ifndef SCHEDULER_H_
#define SCHEDULER_H_
#include <utils/hello.h>
#include <utils/utils.h>
#include <stdbool.h>

// tipos de datos

typedef enum{
    CMN,
    FIFO,
    RR
} t_planificacion;


/*
 * Declaracion de funcioens
*/
void* atender_cliente(void *arg);
void atender_cpu(t_cpu* cpu);
void atender_io(t_io* io);
void enviar_paquete_a_todas_las_cpus(t_paquete* paquete);
void* km_notif(int *conexion_kernel_memory);

// planificacion
bool intentar_planificar();
t_cpu* proxima_cpu();
int planificar_CMN();
int planificar_RR_y_FIFO();

// mover entre listas de procesos
void blocked_a_susp_blocked(int pid);
void pasoDeSuspBlocked_a_Susp_ready(int pid);
void susp_ready_a_ready(int pid);
void ready_a_exec(int pid);
void exec_a_blocked(int pid);
void exec_a_ready(int pid);
void new_a_ready(proceso_id);


// funciones genericas
void pasarA(t_list* lsrc,t_list*ldest, int pid); // mover un pid de una lista a otra

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
