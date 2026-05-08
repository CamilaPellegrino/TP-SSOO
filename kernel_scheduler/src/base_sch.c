#include "base_sch.h"
void inicializar_lista_ready();

// inicializar cosas 
void inicializar_variables_globales(){
    // inicialziar listas
	lista_io = list_create();
    lista_cpus = list_create();
    lista_new = list_create();
    lista_exec = list_create();
    lista_blocked = list_create();
    inicializar_lista_ready();
    // inicializar semaforos
    sem_init(&s_planificar_largo, 0, 0);
    sem_init(&s_planificar_corto, 0, 0);

}

t_list* queues_algorithms_a_t_list(char** queues_algorithms_str){
    t_list* ret = list_create();
    if (queues_algorithms_str == NULL){
        return NULL;
    }        
    for (int i = 0; queues_algorithms_str[i] != NULL; i++)
    {
        char *algoritmo_str = queues_algorithms_str[i];
        t_planificacion* algoritmo = malloc(sizeof(t_planificacion));
        *algoritmo = obtener_algoritmo_planificacion(algoritmo_str);
        list_add(ret, algoritmo);
    }
    return ret;
}

void inicializar_lista_ready(){
    lista_ready = list_create();
    if(algoritmo == CMN){
        for(int i = 0; i<list_size(queues_algorithms); i++){
            // agrego una lista para cada nivel
            t_list* lista = list_create();
            list_add(lista_ready, lista);
        }
    }else{
        lista_ready = list_create();
    }
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
        nuevo_cpu->proceso = NULL;
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
        agregar_a_ready_CMN(proceso);
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
        agregar_a_ready_CMN(proceso);
    }else{
        pasarA(lista_exec, lista_ready, proceso, LISTO);
    }
}

void new_a_ready(t_pcb* proceso){
    if(algoritmo == CMN){
        list_remove_element(lista_new, proceso);
        agregar_a_ready_CMN(proceso);
    }else{
        pasarA(lista_new, lista_ready, proceso, LISTO);
    }
}

void agregar_a_ready_CMN(t_pcb* proceso){
    // asumo que es CMN y agrego el proceso a ready
    if (proceso == NULL) {
        log_error(logger, "proceso NULL en agregar_a_ready_CMN");
        return;
    }
    t_list* cola_prioridad = sublista_ready_de_prioridad(proceso->prioridad);
    if(cola_prioridad == NULL){
        log_error(logger, "error al obtener la cola de prioridad %d", proceso->prioridad);
        exit(EXIT_FAILURE); // si no encuentro la cola de prioridad del proceso, exit con error (prioridad fuera de rango)
    }
    list_add(cola_prioridad, proceso);
    proceso->estado = LISTO;
}

t_pcb* nuevo_proc(int pid, int prioridad, char* instrucciones){
    t_pcb* pcb = iniciar_pcb(pid, prioridad, NUEVO);
    list_add(lista_new, pcb);
    sem_post(&s_planificar_largo);
    return pcb;
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

int iniciar_servidor_o_exit(char* puerto){
    int fd = iniciar_servidor(puerto);

    if(fd == -1){
        log_error(logger, "No se pudo iniciar el servidor");
        exit(EXIT_FAILURE);
    }
    return fd;
}

t_planificacion obtener_algoritmo_planificacion(char *algoritmo_str){
    t_planificacion algo = algoritmo_str_a_enum(algoritmo_str);
    if(algo == -1){
        printf("%s no es un algoritmo invalido, proba con FIFO, RR o CMN", algoritmo_str);
        exit(EXIT_FAILURE);
    }
    return algo;
}

pthread_t crear_hilo_o_exit(void* (*funcion)(void*), void* arg, char* nombre_hilo){
    pthread_t hilo;
    int resultado = pthread_create(&hilo, NULL, funcion, arg);

    if(resultado != 0){
        log_error(logger, "Error al crear el hilo %s. Codigo: %d", nombre_hilo, resultado);
        exit(EXIT_FAILURE);
    }
    return hilo;
}