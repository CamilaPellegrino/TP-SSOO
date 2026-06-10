#ifndef MANEJAR_SEGMENTOS_H_
#define MANEJAR_SEGMENTOS_H_

#include "base_km.h"

t_hueco* ubicacion_de_proximo_segmento(uint32_t tamanio); // devuelve la ubicacion, segun worst_fig o best_fit
void agregar_segmento_a_proceso(t_segmento* segmento, t_proceso* proceso); // pone el segmento en la tabla de segmentos del proceso y en al  tabla de segmentos global
bool achicar_hueco(t_hueco* hueco, uint32_t seg_tam); // achica el hueco en el tamanio indicado, devuelve true si pudo, false si no
t_segmento* crear_segmento(t_proceso* proceso, uint32_t id_segmento, uint32_t tamanio);
bool segmento_del_proceso(t_segmento* segmento, t_proceso* proceso);

bool eliminar_segmento(t_segmento* segmento, t_proceso* proceso);
uint32_t direccion_final(t_hueco* h);
void fusionar_huecos_contiguos();
void compactar_memoria();
t_hueco* crear_hueco(int32_t base, uint32_t tamanio);
#endif /* MANEJAR_SEGMENTOS_H_ */