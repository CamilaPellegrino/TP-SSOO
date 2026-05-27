#ifndef KERNEL_MEMORY_H_
#define KERNEL_MEMORY_H_

#include <utils/hello.h>
#include <utils/utils.h>
#include "base_km.h"
#include "manejar_segmentos.h"
#include "tests.h"

void* atender_cliente(void *arg);
void* atender_scheduler(void*);
void atender_stick(int sch_fd, int *tamanio);
void atender_swap(int swap_fd);
void* atender_cpu(void* cpu);

void enviar_nuevo_stick_a_scheduler(t_stick* nuevo_stick, int sch_fd);
void enviar_sticks_a_cpu(int cpu_fd);

void recibir_pcb_actualizado(t_list* valores);
void agregar_pcb_al_paquete(t_pcb* pcb, t_paquete* p); 
bool guardar_nuevo_proceso(int pid, int ppid, char* ruta_instrucciones);
t_list* instrucciones_de_ruta(char* ruta);
t_proceso* proceso_de_pid(int pid);

t_list* instrucciones_de_ruta(char* nombre_archivo);

// variables globales

t_log *logger;
t_list *lista_sticks;
t_list *lista_cpus;
int sch_fd; 


// listas para cosas de segmentos
t_list *lista_segmentos_global;
t_list *lista_procesos;
t_list *lista_huecos;

// variables para cosas de segmentos
t_fit algoritmo_fit;
int tamanio_total_mem;

// De config
char* scripts_basepath;
char* puerto;

// mutex
pthread_mutex_t m_lista_segmentos_global;
pthread_mutex_t m_lista_procesos;
pthread_mutex_t m_lista_huecos;

#endif /* KERNEL_MEMORY_H_ */