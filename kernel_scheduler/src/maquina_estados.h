#ifndef MAQUINA_ESTADOS_H_
#define MAQUINA_ESTADOS_H_

#include "base_sch.h"

void inicializar_maquina_estados(void);
void bloquear_maquina_estados(void);
void desbloquear_maquina_estados(void);
int cantidad_procesos_ready(void);
t_pcb* proceso_new_siguiente(void);
t_pcb* desencolar_ready_cmn(void);
t_pcb* desencolar_ready_fifo_rr(void);
void liberar_pcb_de_exit(int pid);
void intentar_desuspender(void);
void loguear_tamanio_listas_de_estado(void);
void imprimir_estado_procesos(void);
t_pcb* get_proceso_de_pid_thread_safe(int pid);
void blocked_a_susp_blocked(t_pcb* proceso);
void susp_blocked_a_susp_ready(t_pcb* proceso);
void susp_ready_a_ready(t_pcb* proceso);
void blocked_a_ready(t_pcb* proceso);
void exec_a_blocked_cond_signal(t_pcb* proceso);
void exec_a_ready_cond_signal(t_pcb* proceso);
void exec_a_exit(t_pcb* proceso);
void new_a_ready(t_pcb* proceso);
void proceso_a_new(t_pcb* proceso);
void registrar_proceso_exec(t_pcb* proceso);
void agregar_a_ready_al_frente(t_pcb* proceso);
void desbloquear_proceso(t_pcb* proceso);
void cambiar_prioridad(t_pcb* proceso, int prioridad);
t_pcb* proceso_de_estado(int pid, t_tipo_estado estado);

#endif /* MAQUINA_ESTADOS_H_ */
