#ifndef CONEXION_MEMORIA_H_
#define CONEXION_MEMORIA_H_

#include "base_km.h"
#include <commons/bitarray.h>
typedef struct{
    int pid;
    t_list* segmentos_suspendidos;
}t_proceso_suspendido;

typedef struct{
    int id_segmento;
    int tamanio;
    t_list* bloques;
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
void atender_sch_escritura(t_evt* evt);
void atender_sch_lectura(t_evt* evt);
void atender_sch_mover(t_evt* evt);
void ejecutar_pedidos_escritura_en_swap(t_list* data);
t_list* bloques_necesarios_para_escribir_en_swap(void* contenido, int tamanio);
int obtener_primer_bloque_libre();
int cant_bloques_libres();
bool bloques_libres_suficientes(int n);

t_data_escritura_bloque* iniciar_data_escritura_bloque(int num_bloque, void* contenido, int tamanio);
#endif /* CONEXION_MEMORIA_H_ */