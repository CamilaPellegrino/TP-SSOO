#include "test.h"

void testear(){
    printf("procesos en ready: %d", list_size(lista_ready));
    t_pcb* proc1 = iniciar_pcb(0, 0, LISTO);
    list_add(lista_ready, proc1);
    t_cpu* cpu = iniciar_cpu(1,0);
    list_add(lista_cpus, cpu);
    
    t_pcb* prox_proc = proximo_proceso();
    t_cpu* prox_cpu = proxima_cpu();
    
    printf("procs en ready: %d, cpus: %d\n", list_size(lista_ready), list_size(lista_cpus));
    printf("pid de prox_proc: %d\n", prox_proc->pid);
    printf("id de prox_cpu: %d\n", prox_cpu->id);
    exit(EXIT_SUCCESS);
}