#ifndef CONEXION_MEMORIA_H_
#define CONEXION_MEMORIA_H_

#include "base_km.h"

void* atender_stick(void* arg);

void atender_swap(int swap_fd);
t_list* pedidos_a_sticks_para_acceder_a(int base, int tamanio);

void atender_sch_escritura(t_evt* evt);
void atender_sch_lectura(t_evt* evt);
void atender_sch_mover(t_evt* evt);
#endif /* CONEXION_MEMORIA_H_ */