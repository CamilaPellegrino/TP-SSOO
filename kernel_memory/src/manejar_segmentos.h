#ifndef MANEJAR_SEGMENTOS_H_
#define MANEJAR_SEGMENTOS_H_

#include "base_km.h"

t_segmento* crear_segmento_thread_safe(t_proceso* proceso, uint32_t id_segmento, uint32_t tamanio);
bool eliminar_segmento_thread_safe(t_segmento* segmento, t_proceso* proceso);
bool hay_espacio_total_thread_safe(uint32_t tamanio);
bool compactar_memoria();
void agregar_espacio_mem_thread_safe(int bytes);
bool existe_segmento_de_id_de_proc_thread_safe(int id, t_proceso* p);

void destruir_proceso(t_proceso* p);
void agregar_segmentos_al_paquete(t_list* segmentos, t_paquete* p);
int calc_dir_fisica(int pid, int id_segmento, int offset);
#endif /* MANEJAR_SEGMENTOS_H_ */