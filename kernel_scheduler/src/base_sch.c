#include "base_sch.h"
void inicializar_lista_ready();
void log_obligatorio_cambio_de_estado(int pid, char* estado_anterior, char* estado_actual);

int procesos_en_ready;
pthread_mutex_t m_procesos_en_ready;
// inicializar cosas 
void inicializar_variables_globales(t_config* config){
    inicializar_parametros_de_config(config);

    // inicializar listas de estados de procesos
    lista_new          = list_create();
    lista_exec         = list_create();
    lista_blocked      = list_create();
    lista_susp_blocked = list_create();
    lista_susp_ready   = list_create();
    lista_exit         = list_create();
    inicializar_lista_ready();

    // inicializar listas de eventos
    lista_evt_sleep    = list_create();
    lista_evt_stdin    = list_create();
    lista_evt_stdout   = list_create();

    // inicializar listas de otras cosas 
    lista_cpus         = list_create();
	lista_io           = list_create();
    lista_mutex        = list_create();

    // inicializar semaforos
    sem_init(&s_intentar_planificar, 0, 0);
    sem_init(&s_nuevo_proceso_new, 0, 0);
    sem_init(&s_evt_sleep, 0, 0);
    sem_init(&s_evt_stdin, 0, 0);
    sem_init(&s_evt_stdout, 0, 0);

    // inicializar mutexes
    pthread_mutex_init(&m_procesos_en_ready, NULL);

    pthread_mutex_init(&m_lista_evt_sleep, NULL);
    pthread_mutex_init(&m_lista_evt_stdin, NULL);
    pthread_mutex_init(&m_lista_evt_stdout, NULL);

    pthread_mutex_init(&m_lista_new, NULL);
    pthread_mutex_init(&m_lista_ready, NULL);
    pthread_mutex_init(&m_lista_exec, NULL);
    pthread_mutex_init(&m_lista_blocked, NULL);
    pthread_mutex_init(&m_lista_susp_blocked, NULL);
    pthread_mutex_init(&m_lista_susp_ready, NULL);
    pthread_mutex_init(&m_lista_exit, NULL);
    pthread_mutex_init(&m_lista_mutex, NULL);
    pthread_mutex_init(&m_lista_cpus, NULL);
    
    //otras cosas
    pthread_mutex_lock(&m_proximo_pid);
    proximo_pid = 0;
    pthread_mutex_unlock(&m_proximo_pid);
    pthread_mutex_lock(&m_procesos_en_ready);
    procesos_en_ready = 0;
    pthread_mutex_unlock(&m_procesos_en_ready);

}   

void inicializar_parametros_de_config(t_config* config){
    // leer de config
    t_log_level log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));
    logger = iniciar_logger("kernel_scheduler.log", "ProcesoKernelScheduler", log_level);

    suspension_timeout = config_get_int_value(config, "SUSPENSION_TIMEOUT");
    algoritmo = obtener_algoritmo_planificacion(config_get_string_value(config, "PLANIFICATION_ALGORITHM"));
    char** queues_str = config_get_array_value(config, "QUEUES_ALGORITHMS");
    queues_algorithms = queues_algorithms_a_t_list(queues_str);
    string_array_destroy(queues_str); 
    quantum = config_get_int_value(config, "RR_QUANTUM"); 
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
        nuevo_cpu->desalojando = false;
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

t_pcb* iniciar_pcb(int pid, int ppid, int prioridad, t_tipo_estado estado){
	t_pcb* nuevo_pcb = malloc(sizeof(t_pcb));
	nuevo_pcb->estado = NUEVO;
	nuevo_pcb->pid = pid;
	nuevo_pcb->ppid = ppid;
	nuevo_pcb->prioridad = prioridad;
	nuevo_pcb->estado = estado;   
    nuevo_pcb->data_cond.cond_val = false;
    pthread_mutex_init(&nuevo_pcb->data_cond.mutex_cond, NULL);
    pthread_cond_init(&nuevo_pcb->data_cond.cond, NULL);
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
t_evt* iniciar_evt_std_in_out(int tamanio, int dir_logica, t_pcb* proceso){
    t_evt* evt = malloc(sizeof(t_evt));
    evt->proceso = proceso;
    evt->syscall_finalizada = false;
    pthread_cond_init(&evt->cond, NULL);
    pthread_mutex_init(&evt->mutex, NULL);
    t_evt_std_in_out* evt_std_in_out = malloc(sizeof(*evt_std_in_out));
    evt_std_in_out->tamanio = tamanio;
    evt_std_in_out->dir_logica = dir_logica;
    evt->data_evt = evt_std_in_out;
    return evt;
}
// funciones par destroy
void destroy_pcb(t_pcb* pcb){ free(pcb); }

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
    log_obligatorio_cambio_de_estado(proceso->pid, "BLOCKED", "SUSP_BLOCKED");

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
    log_obligatorio_cambio_de_estado(proceso->pid, "SUSP_BLOCKED", "SUSP_READY");

}

void susp_ready_a_ready(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_susp_ready);
    bool eliminado = eliminar_proceso_de_lista(lista_susp_ready, proceso, "lista_susp_ready");
    pthread_mutex_unlock(&m_lista_susp_ready);

    if(eliminado){
        agregar_a_ready(proceso);
    }
    log_obligatorio_cambio_de_estado(proceso->pid, "SUSP_READY", "READY");

}

void ready_a_exec(t_pcb* proceso){
    bool eliminado = eliminar_de_ready(proceso);
    if(eliminado){

        pthread_mutex_lock(&m_lista_exec);
        agregar_proceso_a_lista(lista_exec, proceso, EJECUTANDO);
        pthread_mutex_unlock(&m_lista_exec);
    }
    log_obligatorio_cambio_de_estado(proceso->pid, "READY", "EXEC");
}

void blocked_a_ready(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_blocked);
    bool eliminado = eliminar_proceso_de_lista(lista_blocked, proceso, "lista_blocked");
    pthread_mutex_unlock(&m_lista_blocked);

    if(eliminado){
        agregar_a_ready(proceso);
    }
    log_obligatorio_cambio_de_estado(proceso->pid, "BLOCKED", "READY");
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
    log_obligatorio_cambio_de_estado(proceso->pid, "EXEC", "BLOCKED");
}

void exec_a_blocked_cond_signal(t_pcb* proceso){
    if(algoritmo_de_proceso(proceso) != RR){
        exec_a_blocked(proceso);
        log_debug(logger, "El proceso %d no usa RR, no hago lo de cond", proceso->pid);
        return;
    }
    pthread_mutex_lock(&proceso->data_cond.mutex_cond);
    exec_a_blocked(proceso);
    proceso->data_cond.cond_val = true;
    pthread_cond_signal(&proceso->data_cond.cond);
    pthread_mutex_unlock(&proceso->data_cond.mutex_cond);
}

void exec_a_ready(t_pcb* proceso){

    pthread_mutex_lock(&m_lista_exec);
    bool eliminado = eliminar_proceso_de_lista(lista_exec, proceso, "lista_exec");
    pthread_mutex_unlock(&m_lista_exec);

    if(eliminado){
        agregar_a_ready(proceso);
    }
    log_obligatorio_cambio_de_estado(proceso->pid, "EXEC", "READY");
}

void exec_a_ready_cond_signal(t_pcb* proceso){
    pthread_mutex_lock(&proceso->data_cond.mutex_cond);
    proceso->data_cond.cond_val = true;
    pthread_cond_signal(&proceso->data_cond.cond);
    pthread_mutex_unlock(&proceso->data_cond.mutex_cond);
    exec_a_ready(proceso);
}

void new_a_ready(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_new);
    bool eliminado = eliminar_proceso_de_lista(lista_new, proceso, "lista_new");
    pthread_mutex_unlock(&m_lista_new);

    if(eliminado){
        agregar_a_ready(proceso);
    }
    log_obligatorio_cambio_de_estado(proceso->pid, "NEW", "READY");
}

void exec_a_exit(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_exec);
    bool eliminado = eliminar_proceso_de_lista(lista_exec, proceso, "lista_exec");
    pthread_mutex_unlock(&m_lista_exec);

    if(eliminado){
        pthread_mutex_lock(&m_lista_exit);
        agregar_proceso_a_lista(lista_exit, proceso, FINALIZADO);
        pthread_mutex_unlock(&m_lista_exit);
    }
    log_obligatorio_cambio_de_estado(proceso->pid, "EXEC", "EXIT");
}

void proceso_a_new(t_pcb* proceso){
    pthread_mutex_lock(&m_lista_new);
    agregar_proceso_a_lista(lista_new, proceso, NUEVO);
    pthread_mutex_unlock(&m_lista_new);
    log_info(logger, "## (<%d>) Se crea el proceso - Estado: NEW", proceso->pid);
}

void agregar_a_ready(t_pcb* proceso){
    if(algoritmo == CMN){
        agregar_a_ready_CMN(proceso);
    }else{
        pthread_mutex_lock(&m_lista_ready);
        agregar_proceso_a_lista(lista_ready, proceso, LISTO);
        pthread_mutex_unlock(&m_lista_ready);
    }    
    pthread_mutex_lock(&m_procesos_en_ready);
    procesos_en_ready++;
    pthread_mutex_unlock(&m_procesos_en_ready);
    sem_post(&s_intentar_planificar);
    log_debug(logger, "Hice sem_post de s_nuevo_proceso_ready");
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
    bool eliminado;
    if(algoritmo == CMN){
        eliminado = eliminar_de_ready_CMN(proceso);
    }else{
        pthread_mutex_lock(&m_lista_ready);
        eliminado = eliminar_proceso_de_lista(lista_ready, proceso, "lista_ready");
        pthread_mutex_unlock(&m_lista_ready);
    }
    if(eliminado){
        pthread_mutex_lock(&m_procesos_en_ready);
        procesos_en_ready--;
        pthread_mutex_unlock(&m_procesos_en_ready);
    }

    return eliminado;
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

t_pcb* nuevo_proc(int prioridad, int ppid, char* instrucciones){
    pthread_mutex_lock(&m_proximo_pid);
    int pid = proximo_pid++;
    pthread_mutex_unlock(&m_proximo_pid);
    t_pcb* pcb = iniciar_pcb(pid, ppid, prioridad, NUEVO);
    proceso_a_new(pcb);
    t_paquete* data_init_proc = crear_paquete(SCH_KM__INIT_PROC);
    agregar_a_paquete(data_init_proc, &pcb->pid, sizeof(int));
    agregar_a_paquete(data_init_proc, &pcb->ppid, sizeof(int));
    agregar_string_a_paquete(data_init_proc, instrucciones);
    enviar_paquete_y_liberarlo(data_init_proc, conexion_kernel_memory);

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
    log_debug(logger, "desbloquear_proceso: ejecutando");
        if(proceso->estado == SUSP_BLOQUEADO){
        // si esta en susp_blocked: mover a susp_ready
        susp_blocked_a_susp_ready(proceso);
        
    }else if(proceso->estado == BLOQUEADO){
        // si esta en blocked: mover a ready
        blocked_a_ready(proceso);
    }
}

// liberar

void liberar_cpu(t_cpu* cpu){
    log_debug(logger, "liberando cpu de id %d", cpu->id);
    cpu->desalojando = false;
    cpu->proceso = NULL;
    sem_post(&s_intentar_planificar);
}

void liberar_pcb_de_exit(int pid){
    pthread_mutex_lock(&m_lista_exit);
    for(int i = 0; i < list_size(lista_exit); i++){
        t_pcb* proceso = list_get(lista_exit, i);
        if(proceso->pid == pid){
            log_debug(logger, "libereando proc encontrado depid = %d", pid);
            list_remove(lista_exit, i);
            destroy_pcb(proceso);
            pthread_mutex_unlock(&m_lista_exit);
            return;
        }
    }
    pthread_mutex_unlock(&m_lista_exit);
    log_error(logger, "No se encontro el proceso PID <%d> en lista_exit", pid);
}

// funciones genericas

t_planificacion obtener_algoritmo_planificacion(char *algoritmo_str){
    t_planificacion algo = algoritmo_str_a_enum(algoritmo_str);
    if(algo == -1){
        printf("%s no es un algoritmo invalido, proba con FIFO, RR o CMN", algoritmo_str);
        exit(EXIT_FAILURE);
    }
    return algo;
}

void enviar_paquete_a_todas_las_cpus(t_paquete* paquete){
    log_info(logger,"%d", list_size(lista_cpus));
    for(int i = 0; i < list_size(lista_cpus); i++){
        
        t_cpu* cpu = list_get(lista_cpus, i);
        int cpu_fd = cpu->fd;
        log_info(logger,"%d", cpu_fd);
        enviar_paquete(paquete, cpu_fd);
        log_info(logger, "Enviando info a cpu de stick");
    }
}

void sumar_milisegundos(struct timespec* ts, int milisegundos)
{
    ts->tv_sec += milisegundos / 1000;
    ts->tv_nsec += (milisegundos % 1000) * 1000000;

    if (ts->tv_nsec >= 1000000000)
    {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000;
    }
}

void log_obligatorio_cambio_de_estado(int pid, char* estado_anterior, char* estado_actual){
    log_info(logger, "## (<%d>) Pasa del estado <%s> al estado <%s>", pid, estado_anterior, estado_actual);
}

void loguear_tamanio_listas_de_estado(){
    log_debug(logger, "new: %d, ready: %d, exec: %d, blocked: %d, susp_blocked: %d, susp_ready: %d"
                    , list_size(lista_new)
                    , procesos_en_ready
                    , list_size(lista_exec)
                    , list_size(lista_blocked)
                    , list_size(lista_susp_blocked)
                    , list_size(lista_susp_ready));
}

t_planificacion algoritmo_de_proceso(t_pcb* proceso){
    if(algoritmo != CMN){
        return algoritmo;
    }
    // para CMN:
    t_planificacion algo = *(t_planificacion*)list_get(queues_algorithms, proceso->prioridad);
    return algo;
}