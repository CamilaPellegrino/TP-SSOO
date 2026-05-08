#ifndef BASE_SCH_H_
#define BASE_SCH_H_

#include <utils/utils.h>
#include <semaphore.h>

typedef struct{
    int pid;
    int prioridad;
	t_tipo_estado estado;
} t_pcb;
typedef struct
{
	int fd;
	int id;
	t_pcb* proceso; // proceso que esta ejecutando (NULL si no esta ejecutando nada)
} t_cpu;

typedef struct
{
	int fd;
	t_tipo_io tipo;
} t_io;

typedef enum{
    CMN,
    FIFO,
    RR
} t_planificacion;


// variables globales 

extern t_list* lista_cpus;
extern t_list* lista_io;
extern t_list* lista_new;
extern t_list* lista_ready;        // lista procesos en ready
extern t_list* lista_exec;         // lista de procesos en exec
extern t_list* lista_blocked;      // procesos en blocked
extern t_list* lista_susp_blocked;
extern t_list* lista_susp_ready;
extern t_log* logger;
extern t_planificacion algoritmo;  // CMN, FIFO o RR
extern t_list* queues_algorithms;   // algoritmo que usa cada cola cuando es CMN

// semaforos

extern sem_t s_planificar_corto;
extern sem_t s_planificar_largo;


// funciones para inicializar cosas
void inicializar_variables_globales();
t_list* queues_algorithms_a_t_list(char** queues_algorithms_str);
t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);
t_pcb* iniciar_pcb(int pid, int prioridad, t_tipo_estado estado);

// mover entre listas de procesos
void blocked_a_susp_blocked(t_pcb* pid);
void susp_blocked_a_susp_ready(t_pcb* pid);
void susp_ready_a_ready(t_pcb* pid);
void ready_a_exec(t_pcb* pid);
void exec_a_blocked(t_pcb* pid);
void exec_a_ready(t_pcb* pid);
void new_a_ready(t_pcb* pid);
t_pcb* nuevo_proc(int pid, int prioridad, char* instrucciones);
t_list* sublista_ready_de_prioridad(int prioridad);
void agregar_a_ready_CMN(t_pcb* proceso);

// funciones genericas
void pasarA(t_list* ,t_list*, t_pcb*, t_tipo_estado); // mover un pid de una lista a otra
int iniciar_servidor_o_exit(char* puerto);
t_planificacion obtener_algoritmo_planificacion(char *algoritmo_str);
pthread_t crear_hilo_o_exit(void* (*funcion)(void*), void* arg, char* nombre_hilo);
#endif