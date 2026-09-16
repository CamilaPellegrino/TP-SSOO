#ifndef CICLO_INSTRUCCION_H_
#define CICLO_INSTRUCCION_H_

#include "base_cpu.h"
void* ejecutar();

void ciclo_instruccion(t_pcb *pcb);
char* fetch(t_pcb *pcb);
void decode(char *instruccion, t_pcb* pcb,  t_instruccion_decodificada * instruccion_decodificada);
bool execute(t_instruccion_decodificada *instruccion, t_pcb *pcb);

// Instrucciones soportadas
void ejecutar_jnz(t_instruccion_decodificada *instruccion, t_pcb *pcb);
void ejecutar_sub(t_instruccion_decodificada *instruccion, t_pcb *pcb);
void ejecutar_sum(t_instruccion_decodificada *instruccion, t_pcb *pcb);
void ejecutar_set(t_instruccion_decodificada *instruccion, t_pcb *pcb);

void ejecutar_sleep(t_instruccion_decodificada* instr);
void ejecutar_stdin(t_instruccion_decodificada* instr, t_pcb*);
void ejecutar_stdout(t_instruccion_decodificada* instr, t_pcb*);
void ejecutar_m_create(t_instruccion_decodificada* instr);

void ejecutar_m_lock(t_instruccion_decodificada* instr);
void ejecutar_m_unlock(t_instruccion_decodificada* instr);
void ejecutar_exit(t_instruccion_decodificada* instr, t_pcb* pcb);
void ejecutar_init_proc(t_instruccion_decodificada* instr);

void ejecutar_copy_mem(t_instruccion_decodificada* instruccion, t_pcb* pcb);
void ejecutar_mov_in(t_instruccion_decodificada* instr, t_pcb* pcb);
void ejecutar_mov_out(t_instruccion_decodificada* instr, t_pcb* pcb);
void ejecutar_mem_alloc(t_instruccion_decodificada* instr);

void ejecutar_mem_free(t_instruccion_decodificada* instr);

// Otros
void manejar_seg_fault(t_pcb* pcb);


#endif /* CICLO_INSTRUCCION_H_ */