#ifndef CONEXION_MEMORIA_H_
#define CONEXION_MEMORIA_H_

#include "base_km.h"
#include "manejar_segmentos.h"
#include <commons/bitarray.h>
typedef struct{
    t_list* segmentos_suspendidos;
    t_list* lista_instrucciones;
    t_pcb* pcb;
}t_proceso_suspendido;
typedef struct{
    int id_segmento;
    t_list* bloques;
    int tamanio;
}t_segmento_suspendido;
 
typedef struct{
    int num_bloque;
    void* contenido; 
    int tamanio;
}t_data_escritura_bloque;

void* atender_stick(void* arg);
void liberar_data_escritura_bloque(void* d);
void atender_swap(int swap_fd);
t_list* pedidos_a_sticks_para_acceder_a(int base, int tamanio);
void atender_suspension(t_evt* evt);
void atender_desuspension(t_evt* evt);
void desuspender_proceso(int pid);
void atender_sch_escritura(t_evt* evt);
void atender_sch_lectura(t_evt* evt);
void atender_sch_mover(t_evt* evt);
void ejecutar_pedidos_escritura_en_swap(t_list* data);
void* ejecutar_pedidos_lectura_en_swap(t_list* data);
t_list* bloques_necesarios_para_escribir_en_swap(void* contenido, int tamanio);
int obtener_primer_bloque_libre();
int cant_bloques_libres();
bool bloques_libres_suficientes(int n);

t_data_escritura_bloque* iniciar_data_escritura_bloque(int num_bloque, void* contenido, int tamanio);
t_proceso_suspendido* iniciar_proceso_suspendido(t_proceso*);

t_segmento_suspendido* iniciar_segmento_suspendido(int id_segmento, t_list*bloques, int tamanio);

bool proceso_suspendido_thread_safe(int pid);
t_proceso_suspendido* proceso_suspendido_de_pid_thread_safe(int pid);
#endif /* CONEXION_MEMORIA_H_ */