#include "test.h"

void testear_listas();

void testear(){
    log_debug(logger, "Testeando cosas :)");
    testear_listas();

    exit(EXIT_SUCCESS);
}

void testear_listas(){
    log_debug(logger, "tamanio de lista_ready: %d", list_size(lista_ready));
    t_pcb* proc1 = iniciar_pcb(0, 0, LISTO);
    agregar_a_ready_CMN(proc1);
    t_cpu* cpu = iniciar_cpu(1,0);
    list_add(lista_cpus, cpu);
    
    t_pcb* prox_proc = proximo_proceso();
    t_cpu* prox_cpu = proxima_cpu();
    
    log_debug(logger, "Tamanio de ready: %d, cpus: %d\n", list_size(lista_ready), list_size(lista_cpus));
    log_debug(logger, "pid de prox_proc: %d\n", prox_proc->pid);
    log_debug(logger, "id de prox_cpu: %d\n", prox_cpu->id);

}