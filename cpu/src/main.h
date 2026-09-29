#ifndef CPU_H_
#define CPU_H_

#include <utils/hello.h>
#include <utils/utils.h>
#include <semaphore.h>
#include "ciclo_instruccion/ciclo_instruccion.h"
#include "maquina_estados_cpu.h"
#include "base_cpu.h"

// variables globales
t_log* logger;
t_list* lista_sticks;  // lista global para guardar los memory sticks a los que me conecte

bool enviar_contexto_y_desalojar;
pthread_mutex_t m_enviar_contexto_y_desalojar;

bool v_pedido_de_desalojo;
pthread_cond_t cond_ejecutar;

bool v_pedir_segmentos;
pthread_mutex_t m_pedir_segmentos;

int pid_pendiente; 
pthread_mutex_t m_pid_pendiente;

t_estado_cpu estado_cpu;
pthread_mutex_t m_estado_cpu;

// otros
int conexion_kernel_scheduler;
int conexion_kernel_memory;

int seg_max_size_global;
int id_cpu;


// Declaracion de funciones

void recibir_sticks_de_km();

void* ejecutar();


void pedir_contexto(int pid, t_pcb* pcb);



#endif /* CPU_H_ */