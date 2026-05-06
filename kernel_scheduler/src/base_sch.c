#include "base_sch.h"

// inicializar cosas 

void inicializar_variables_globales(){
	lista_io = list_create();
    lista_cpus = list_create();
    lista_new = list_create();
    lista_ready = list_create();
    lista_exec = list_create();
    lista_blocked = list_create();
}

t_planificacion algoritmo_str_a_enum(char *algoritmo_str){
    if(strcmp(algoritmo_str, "FIFO") == 0){
        return FIFO;
    }
    if(strcmp(algoritmo_str, "RR") == 0){
        return RR;
    }
    if(strcmp(algoritmo_str, "CMN") == 0){
        return CMN;
    }
    return -1;
}

t_cpu* iniciar_cpu(int id, int fd){
	t_cpu* nuevo_cpu = malloc(sizeof(t_cpu));
	if (nuevo_cpu != NULL) {
		nuevo_cpu->id = id;
		nuevo_cpu->fd = fd;
	}
	return nuevo_cpu;
}

t_io* iniciar_io(t_tipo_io tipo, int io_fd){
	t_io* nueva_io = malloc(sizeof(t_io));
	if(nueva_io != NULL){
		nueva_io->tipo = tipo;
		nueva_io->fd = io_fd;
	}
	return nueva_io;
}

t_pcb* iniciar_pcb(int pid, int prioridad, t_tipo_estado estado){
	t_pcb* nuevo_pcb = malloc(sizeof(t_pcb));
	nuevo_pcb->estado = NUEVO;
	nuevo_pcb->pid = pid;
	nuevo_pcb->prioridad = prioridad;
	nuevo_pcb->estado = estado;
	return nuevo_pcb;
}

// mover entre estados
void blocked_a_susp_blocked(t_pcb* proceso){
    pasarA(lista_blocked, lista_susp_blocked, proceso, SUSP_BLOQUEADO);
}

void susp_blocked_a_susp_ready(t_pcb* proceso){
    pasarA(lista_susp_blocked, lista_susp_ready, proceso, SUSP_LISTO);
}

void susp_ready_a_ready(t_pcb* proceso){
    if(algoritmo == CMN){
        list_remove_element(lista_susp_ready, proceso);
        // agregar_a_ready_CMN(proceso);
    }else{
        pasarA(lista_susp_ready, lista_ready, proceso, LISTO);
    }
}

void ready_a_exec(t_pcb* proceso){
    pasarA(lista_ready, lista_exec, proceso, EJECUTANDO);
}

void exec_a_blocked(t_pcb* proceso){
    pasarA(lista_exec, lista_blocked, proceso, BLOQUEADO);
}

void exec_a_ready(t_pcb* proceso){
    if(algoritmo == CMN){
        list_remove_element(lista_exec, proceso);
        // agregar_a_ready_CMN(proceso);
    }else{
        pasarA(lista_exec, lista_ready, proceso, LISTO);
    }
}

void new_a_ready(t_pcb* proceso){
    if(algoritmo == CMN){
        list_remove_element(lista_new, proceso);
        //agregar_a_ready_CMN(proceso);
    }else{
        pasarA(lista_new, lista_ready, proceso, LISTO);
    }
}

t_list* sublista_ready_de_prioridad(int prioridad){
    // si es CMN lista_ready tiene sublistas, deuelve la sublista de la prioridad pedida
    return list_get(lista_ready, prioridad);
}

// genericas
void pasarA(t_list* lsrc,t_list*ldest, t_pcb* proceso, t_tipo_estado estado){
    list_remove_element(lsrc, proceso);
    list_add(ldest, proceso);
    proceso->estado = estado;
}
