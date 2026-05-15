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
    lista_susp_blocked = list_create();
    lista_susp_ready = list_create();
    inicializar_lista_ready();

    lista_evt_sleep = list_create();
    lista_mutex = list_create();

    // inicializar semaforos
    sem_init(&s_nueva_cpu_libre, 0, 0);
    sem_init(&s_nuevo_proceso_ready, 0, 0);
    sem_init(&s_nuevo_proceso_new, 0, 0);
    sem_init(&s_evt_sleep, 0, 0);

    // inicializar mutexes
    pthread_mutex_init(&m_lista_evt_sleep, NULL);
    pthread_mutex_init(&m_lista_evt_sleep, NULL);
    pthread_mutex_init(&m_lista_new, NULL);
    pthread_mutex_init(&m_lista_ready, NULL);
    pthread_mutex_init(&m_lista_exec, NULL);
    pthread_mutex_init(&m_lista_blocked, NULL);
    pthread_mutex_init(&m_lista_susp_blocked, NULL);
    pthread_mutex_init(&m_lista_susp_ready, NULL);
    pthread_mutex_init(&m_lista_mutex, NULL);
    
    //otras cosas
    proximo_pid = 0;
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

t_evt* iniciar_evt_sleep(int tiempo_sleep, t_pcb* proceso){
    t_evt* evt = malloc(sizeof(t_evt));
    evt->proceso = proceso;
    evt->syscall_finalizada = false;
    pthread_cond_init(&evt->cond, NULL);
    pthread_mutex_init(&evt->mutex, NULL);
    t_evt_sleep* evt_sleep = malloc(sizeof(t_evt_sleep));
    evt_sleep->tiempo_sleep = tiempo_sleep;
    evt->data_evt = evt_sleep;
    return evt;
}

// mover entre estados
void blocked_a_susp_blocked(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_blocked);
    bool eliminado = eliminar_proceso_de_lista(lista_blocked, proceso, "lista_blocked");
    pthread_mutex_unlock(&m_lista_blocked);
    if(eliminado){
        pthread_mutex_lock(&m_lista_susp_blocked);
        agregar_proceso_a_lista(lista_susp_blocked, proceso, SUSP_BLOQUEADO);
        pthread_mutex_unlock(&m_lista_susp_blocked);
    }
}

void susp_blocked_a_susp_ready(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_susp_blocked);
    bool eliminado = eliminar_proceso_de_lista(lista_susp_blocked, proceso, "lista_susp_blocked");
    pthread_mutex_unlock(&m_lista_susp_blocked);

    if(eliminado){
        pthread_mutex_lock(&m_lista_susp_ready);
        agregar_proceso_a_lista(lista_susp_ready, proceso, SUSP_LISTO);
        pthread_mutex_unlock(&m_lista_susp_ready);
    }
}

void susp_ready_a_ready(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_susp_ready);
    bool eliminado = eliminar_proceso_de_lista(lista_susp_ready, proceso, "lista_susp_ready");
    pthread_mutex_unlock(&m_lista_susp_ready);

    if(eliminado){
        agregar_a_ready(proceso);
    }
}

void ready_a_exec(t_pcb* proceso){
    bool eliminado = eliminar_de_ready(proceso);
    if(eliminado){

        pthread_mutex_lock(&m_lista_exec);
        agregar_proceso_a_lista(lista_exec, proceso, EJECUTANDO);
        pthread_mutex_unlock(&m_lista_exec);
    }
}

void ready_a_blocked(t_pcb* proceso){
    bool eliminado = eliminar_de_ready(proceso);
    if(eliminado){
        pthread_mutex_lock(&m_lista_blocked);
        agregar_proceso_a_lista(lista_blocked, proceso, BLOQUEADO);
        pthread_mutex_unlock(&m_lista_blocked);
    }
}

void blocked_a_ready(t_pcb* proceso){

    pthread_mutex_lock(&m_lista_blocked);
    bool eliminado = eliminar_proceso_de_lista(lista_blocked, proceso, "lista_blocked");
    pthread_mutex_unlock(&m_lista_blocked);

    if(eliminado){
        agregar_a_ready(proceso);
    }
}

void exec_a_blocked(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_exec);
    bool eliminado = eliminar_proceso_de_lista(lista_exec, proceso, "lista_exec");
    pthread_mutex_unlock(&m_lista_exec);

    if(eliminado){
        pthread_mutex_lock(&m_lista_blocked);
        agregar_proceso_a_lista(lista_blocked, proceso, BLOQUEADO);
        pthread_mutex_unlock(&m_lista_blocked);
    }
}

void exec_a_ready(t_pcb* proceso){

    pthread_mutex_lock(&m_lista_exec);
    bool eliminado = eliminar_proceso_de_lista(lista_exec, proceso, "lista_exec");
    pthread_mutex_unlock(&m_lista_exec);

    if(eliminado){
        agregar_a_ready(proceso);
    }
}

void new_a_ready(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_new);
    bool eliminado = eliminar_proceso_de_lista(lista_new, proceso, "lista_new");
    pthread_mutex_unlock(&m_lista_new);

    if(eliminado){
        agregar_a_ready(proceso);
    }
}

void proceso_a_new(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_new);
    agregar_proceso_a_lista(lista_new, proceso, NUEVO);
    pthread_mutex_unlock(&m_lista_new);
}

void agregar_a_ready(t_pcb* proceso){
    if(algoritmo == CMN){
        agregar_a_ready_CMN(proceso);
    }else{
        pthread_mutex_lock(&m_lista_ready);
        agregar_proceso_a_lista(lista_ready, proceso, LISTO);
        pthread_mutex_unlock(&m_lista_ready);
    }
    sem_post(&s_nuevo_proceso_ready);
}

void agregar_a_ready_CMN(t_pcb* proceso){

    if(proceso == NULL){
        log_error(logger, "proceso NULL en agregar_a_ready_CMN");
        return;
    }
    
    pthread_mutex_lock(&m_lista_ready);
    t_list* cola_prioridad = sublista_ready_de_prioridad(proceso->prioridad);
    agregar_proceso_a_lista(cola_prioridad, proceso, LISTO);
    pthread_mutex_unlock(&m_lista_ready);
}

bool eliminar_de_ready(t_pcb* proceso){
    if(algoritmo == CMN){
        return eliminar_de_ready_CMN(proceso);
    }else{
        pthread_mutex_lock(&m_lista_ready);
        bool eliminado = eliminar_proceso_de_lista(lista_ready, proceso, "lista_ready");
        pthread_mutex_unlock(&m_lista_ready);
        return eliminado;
    }
}

bool eliminar_de_ready_CMN(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_ready);
    t_list* cola = list_get(lista_ready, proceso->prioridad);
    bool eliminado = eliminar_proceso_de_lista(cola, proceso, "cola de prioridad de ready");
    pthread_mutex_unlock(&m_lista_ready);

    return eliminado;
}

bool eliminar_proceso_de_lista(t_list* lista, t_pcb* proceso, char* nombre_lista){
    if (proceso == NULL) {
        log_error(logger, "proceso NULL en eliminar_proceso_de_lista");
        return false;
    }
    bool removido = list_remove_element(lista, proceso);
    if(!removido){
        log_error(logger, "El proceso PID %d no estaba en %s", proceso->pid, nombre_lista);
    }
    return removido;
}

void agregar_proceso_a_lista(t_list* lista, t_pcb* proceso, t_tipo_estado nuevo_estado){
    if (proceso == NULL) {
        log_error(logger, "proceso NULL en agregar_proceso_a_lista");
        return;
    }
    list_add(lista, proceso);
    proceso->estado = nuevo_estado;
}

t_pcb* nuevo_proc(int prioridad, char* instrucciones){
    t_pcb* pcb = iniciar_pcb(proximo_pid++, prioridad, NUEVO);
    proceso_a_new(pcb);
    log_debug(logger, "tamanio ready: %d, tamanio new: %d", list_size(lista_ready), list_size(lista_new));
    sem_post(&s_nuevo_proceso_new);
    log_debug(logger, "fin nuevo proc");

    return pcb;
}

t_list* sublista_ready_de_prioridad(int prioridad){
    // si es CMN lista_ready tiene sublistas, deuelve la sublista de la prioridad pedida
    t_list* cola_prioridad = list_get(lista_ready, prioridad);
    if(cola_prioridad == NULL){
        log_error(logger, "error al obtener la cola de prioridad %d", prioridad);
        exit(EXIT_FAILURE); // si no encuentro la cola de prioridad del proceso, exit con error (prioridad fuera de rango)
    }
    return cola_prioridad;
}

// para desbloquear procesos
void desbloquear_proceso(t_pcb* proceso){
    log_debug(logger, "hilo_desbloquear_proceso");
        if(proceso->estado == SUSP_BLOQUEADO){
        // si esta en susp_blocked: mover a susp_ready
        susp_blocked_a_susp_ready(proceso);
        
    }else if(proceso->estado == BLOQUEADO){
        // si esta en blocked: mover a ready
        blocked_a_ready(proceso);
        sem_post(&s_nuevo_proceso_ready);
    }
}

// liberar

void liberar_cpu(t_cpu* cpu){
    log_debug(logger, "liberando cpu de id %d", cpu->id);
    cpu->proceso = NULL;
}

// genericas
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


