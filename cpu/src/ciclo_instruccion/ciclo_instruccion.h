#ifndef CICLO_INSTRUCCION_H_
#define CICLO_INSTRUCCION_H_

#include "base_cpu.h"

void ciclo_instruccion(t_pcb *pcb);
char* fetch(t_pcb *pcb);

#endif /* CICLO_INSTRUCCION_H_ */