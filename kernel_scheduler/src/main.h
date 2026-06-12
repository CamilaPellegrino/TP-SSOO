#ifndef SCHEDULER_H_
#define SCHEDULER_H_
#include <utils/hello.h>
#include <stdbool.h>
#include "base_sch.h"
#include "planificador.h"
#include "atender_io.h"
#include "mutex.h"
#include "test.h"

// variables globales 
t_lista_estado* estado_new;
t_lista_estado* estado_ready;
t_lista_estado* estado_blocked;
t_lista_estado* estado_exec;
t_lista_estado* estado_susp_blocked;
t_lista_estado* estado_susp_ready;
t_lista_estado* estado_exit;

t_list* lista_cpus;
t_list* lista_io;

t_log * logger;
t_planificacion algoritmo;
t_list* queues_algorithms;  
int proximo_pid;
int suspension_timeout;
int quantum;
int procesos_en_ready;
t_estado_sch estado_global;

// listas de cosas de syscalls
t_list* lista_evt_sleep;
t_list* lista_evt_stdin;
t_list* lista_evt_stdout;
t_list* lista_mutex;

// semaforos
sem_t s_nuevo_proceso_new;
sem_t s_intentar_planificar;
sem_t s_planificar_corto;
sem_t s_evt_sleep;
sem_t s_evt_stdin;
sem_t s_evt_stdout;

// mutexs
pthread_mutex_t m_lista_evt_sleep;
pthread_mutex_t m_lista_evt_stdin;
pthread_mutex_t m_lista_evt_stdout;
pthread_mutex_t m_lista_cpus;
pthread_mutex_t m_lista_mutex;
pthread_mutex_t m_transicionar;
pthread_mutex_t m_proximo_pid;
pthread_mutex_t m_procesos_en_ready;
pthread_mutex_t m_estado_global;
// sockets
int conexion_kernel_memory;

/*
 * Declaracion de funciones
*/
// atender_x
void* atender_cliente(void *arg);
void* atender_cpu(t_cpu* cpu);
void atender_cpu_syscall_sleep(int tiempo_sleep, t_cpu* cpu);
void atender_cpu_syscall_stdin(int tamanio, int dir_logica, t_cpu* cpu);
void atender_cpu_syscall_stdout(int tamanio, int dir_logica, int stick, t_cpu* cpu);
void atender_cpu_syscall_mutex_create(char* nombre_mutex, t_cpu* cpu);
void atender_cpu_syscall_mutex_lock(char* nombre_mutex, t_cpu* cpu);
void atender_cpu_syscall_mutex_unlock(char* nombre_mutex, t_cpu* cpu);
void atender_cpu_syscall_mem_alloc(int id_segmento, int tamanio, t_cpu* cpu);
void atender_cpu_syscall_mem_free(int id_segmento,t_cpu* cpu);
void atender_cpu_ejecucion_detenida(t_cpu* cpu);
void* atender_km(void*);

#endif  /* SCHEDULER_H_ */ 
