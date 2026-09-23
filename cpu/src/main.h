#ifndef CPU_H_
#define CPU_H_

#include <utils/hello.h>
#include <utils/utils.h>
#include <semaphore.h>
#include "ciclo_instruccion.h"
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

void procesar_evento(op_code cod_op);
void transicion_desde_wait_sys(op_code cod_op);
void transicion_desde_exec(op_code cod_op);
void transicion_desde_wait_sys_y_prox_desalojo(op_code cod_op);
void transicion_desde_libre(op_code cod_op);
void transicion_desde_wait_mem_alloc(op_code cod_op);
void transicion_desde_wait_mem_alloc_y_prox_desalojo(op_code cod_op);
void enviar_confirmacion();


void* atender_stick(void* arg);
void conectarse_a_stick(char* ip, char* puerto, uint32_t tamanio);
void recibir_sticks_de_km();

void* ejecutar();


void esperar_a_poder_ejecutar(t_pcb* pcb); 


void pedir_contexto(int pid, t_pcb* pcb);



#endif /* CPU_H_ */