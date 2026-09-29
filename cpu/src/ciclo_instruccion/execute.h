#ifndef EXECUTE_H_
#define EXECUTE_H_

#include "base_cpu.h"

bool execute(t_instruccion_decodificada *instruccion, t_pcb *pcb);
void ejecutar_exit(t_instruccion_decodificada *instr, t_pcb *pcb);
void ejecutar_mov_in(t_instruccion_decodificada *instr, t_pcb *pcb);
void ejecutar_mov_out(t_instruccion_decodificada *instr, t_pcb *pcb);

#endif /* EXECUTE_H_ */
