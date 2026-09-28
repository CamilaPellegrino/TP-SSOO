#ifndef MAQUINA_ESTADOS_CPU_H_
#define MAQUINA_ESTADOS_CPU_H_
#include "base_cpu.h"
#include <utils/hello.h>
#include <utils/utils.h>
#include <semaphore.h>

void atender_scheduler(void);
void procesar_evento(op_code cod_op);
void transicion_desde_exec(op_code cod_op);
void transicion_desde_wait_sys(op_code cod_op);
void transicion_desde_wait_sys_y_prox_desalojo(op_code cod_op);
void transicion_desde_libre(op_code cod_op);
void transicion_desde_wait_mem_alloc(op_code cod_op);
void transicion_desde_wait_mem_alloc_y_prox_desalojo(op_code cod_op);
void enviar_confirmacion(void);
void procesar_desalojo_pendiente(t_pcb* pcb);
void esperar_a_poder_ejecutar(t_pcb* pcb);
void conectarse_a_stick(char* ip, char* puerto, uint32_t tamanio);






#endif /* MAQUINA_ESTADOS_CPU_H_ */