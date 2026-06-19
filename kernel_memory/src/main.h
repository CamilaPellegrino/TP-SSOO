#ifndef KERNEL_MEMORY_H_
#define KERNEL_MEMORY_H_

#include <utils/hello.h>
#include <utils/utils.h>
#include "base_km.h"
#include "manejar_segmentos.h"
#include "conexion_memoria.h"
#include "tests.h"

void* atender_cliente(void *arg);
void* atender_cpu(void* cpu);
void atender_cpu_pcontexto(t_cpu* cpu, t_list* data);
void atender_cpu_fetch(t_cpu* cpu, t_list* data);
void atender_cpu_mov_in(t_list* data, int cpu_fd);
void atender_cpu_mov_out(t_list* data, int fd);

void* atender_scheduler(void*);
void atender_sch_read(t_list* data);
void atender_sch_write(t_list* data);
void atender_sch_init_proc(t_list* data);
void atender_sch_mem_alloc(t_list* data);
void atender_sch_mem_free(t_list* data);
// variables globales

t_log *logger;
t_list *lista_sticks;
t_list *lista_cpus;
int sch_fd; 

// listas para cosas de segmentos
t_list *lista_segmentos_global;
t_list *lista_procesos;
t_list *lista_huecos;
t_list *lista_eventos_stick;

// variables para cosas de segmentos
t_fit algoritmo_fit;
int tamanio_total_mem;
int tamanio_total_libre;

// De config
char* scripts_basepath;
char* puerto;

// mutex
pthread_mutex_t m_lista_segmentos_global;
pthread_mutex_t m_lista_procesos;
pthread_mutex_t m_lista_huecos;
pthread_mutex_t m_lista_eventos_stick;
pthread_mutex_t m_manejar_memoria;

sem_t s_lista_eventos_stick;
sem_t s_fin_mover;
#endif /* KERNEL_MEMORY_H_ */