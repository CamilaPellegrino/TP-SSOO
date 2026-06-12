#ifndef MANEJAR_SEGMENTOS_H_
#define MANEJAR_SEGMENTOS_H_

#include "base_km.h"

t_hueco* ubicacion_de_proximo_segmento(uint32_t tamanio); 
void agregar_segmento_a_proceso(t_segmento* segmento, t_proceso* proceso); 
bool achicar_hueco(t_hueco* hueco, uint32_t seg_tam);
t_segmento* crear_segmento(t_proceso* proceso, uint32_t id_segmento, uint32_t tamanio);
bool segmento_del_proceso(t_segmento* segmento, t_proceso* proceso);

bool eliminar_segmento(t_segmento* segmento, t_proceso* proceso);
uint32_t direccion_final(t_hueco* h);
void fusionar_huecos_contiguos();
bool compactar_memoria();
t_hueco* crear_hueco(int32_t base, uint32_t tamanio);
void agregar_espacio_mem(int bytes);
bool hay_espacio_total(uint32_t tamanio);
#endif /* MANEJAR_SEGMENTOS_H_ */