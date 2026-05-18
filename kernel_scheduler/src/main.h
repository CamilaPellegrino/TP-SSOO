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
t_list* queues_algorithms;  
int proximo_pid;
int suspension_timeout;

// listas de cosas de syscalls
t_list* lista_evt_sleep;
t_list* lista_mutex;

// semaforos
sem_t s_nuevo_proceso_new;
sem_t s_nuevo_proceso_ready;
sem_t s_nueva_cpu_libre;
sem_t s_planificar_corto;
sem_t s_evt_sleep;

// mutexs
pthread_mutex_t m_lista_evt_sleep;   // mutex para la lista de evts tipo sleep
pthread_mutex_t m_lista_evt_sleep;
pthread_mutex_t m_lista_new;
pthread_mutex_t m_lista_ready;
pthread_mutex_t m_lista_exec;
pthread_mutex_t m_lista_blocked;
pthread_mutex_t m_lista_susp_blocked;
pthread_mutex_t m_lista_susp_ready;
pthread_mutex_t m_lista_cpus;
pthread_mutex_t m_lista_mutex;

/*
 * Declaracion de funcioens
*/
// atender_x
void* atender_cliente(void *arg);
void* atender_cpu(t_cpu* cpu);
void atender_cpu_syscall_sleep(int tiempo_sleep, t_cpu* cpu);
void atender_cpu_syscall_mutex_create(char* nombre_mutex, t_cpu* cpu);
void atender_cpu_syscall_mutex_lock(char* nombre_mutex, t_cpu* cpu);
void atender_cpu_syscall_mutex_unlock(char* nombre_mutex, t_cpu* cpu);
void* atender_km(void *conexion_kernel_memory);

// otras
t_planificacion algoritmo_str_a_enum(char *algoritmo_str);

#endif  /* SCHEDULER_H_ */ 
