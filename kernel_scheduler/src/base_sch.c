#include "base_sch.h"
void inicializar_lista_ready();
void log_obligatorio_cambio_de_estado(int pid, char* estado_anterior, char* estado_actual);


// inicializar cosas 
void inicializar_variables_globales(t_config* config){
    inicializar_parametros_de_config(config);


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

    pthread_mutex_init(&m_lista_mutex, NULL);
    pthread_mutex_init(&m_lista_cpus, NULL);
    pthread_mutex_init(&m_transicionar, NULL);
    pthread_mutex_init(&m_estado_global, NULL);

    inicializar_lista_ready();
    estado_new = inicializar_lista_estado("NEW", NULL, true, NUEVO);
    estado_exec = inicializar_lista_estado("EXEC", NULL, true, EJECUTANDO);
    estado_blocked = inicializar_lista_estado("BLOCKED", NULL, true, BLOQUEADO);
    estado_susp_blocked = inicializar_lista_estado("SUSP_BLOCKED", NULL, true, SUSP_BLOQUEADO);
    estado_susp_ready = inicializar_lista_estado("SUSP_READY", NULL, true, SUSP_LISTO);
    estado_exit = inicializar_lista_estado("EXIT", NULL, true, FINALIZADO);

    //otras cosas
    pthread_mutex_lock(&m_proximo_pid);
    proximo_pid = 0;
    pthread_mutex_unlock(&m_proximo_pid);
    pthread_mutex_lock(&m_procesos_en_ready);
    procesos_en_ready = 0;
    pthread_mutex_unlock(&m_procesos_en_ready);
    pthread_mutex_lock(&m_estado_global);
    estado_global = PLANIF_ACTIVA;
    pthread_mutex_unlock(&m_estado_global);
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
    estado_ready = malloc(sizeof(t_lista_estado));
    estado_ready->nombre = "lista_ready";
    estado_ready->sublista = list_create();
    estado_ready->tipo = LISTO;
    if(algoritmo == CMN){
        for(int i = 0; i<list_size(queues_algorithms); i++){
            t_list* lista = list_create();
            t_sublista_ready* s = malloc(sizeof(t_sublista_ready));
            s->sublista = lista;
            pthread_mutex_init(&s->mutex, NULL);
            list_add(estado_ready->sublista, s);
        }
    }
    pthread_mutex_init(&estado_ready->mutex, NULL);
}

t_lista_estado* inicializar_lista_estado(char* nombre, t_list* lista, bool init_m, t_tipo_estado tipo){
    t_lista_estado* e = malloc(sizeof(t_lista_estado));
    e->nombre = nombre;
    e->sublista = lista == NULL ? list_create() : lista;
    e->tipo = tipo;
    if(!init_m){
        return e;
    }
    pthread_mutex_init(&e->mutex, NULL);
    return e;
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
        pthread_mutex_init(&nuevo_cpu->mutex, NULL);
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
    log_debug(logger, "Prior:%d", prioridad);
	t_pcb* nuevo_pcb = malloc(sizeof(t_pcb));
    if(nuevo_pcb != NULL){
        nuevo_pcb->estado = NUEVO;
        nuevo_pcb->pid = pid;
        nuevo_pcb->ppid = ppid;
        nuevo_pcb->prioridad_base = prioridad;
        nuevo_pcb->prioridad_actual = prioridad;
        nuevo_pcb->estado = estado;   
        nuevo_pcb->data_cond.cond_val = false;
        nuevo_pcb->mutex_esperado = NULL;
        pthread_mutex_init(&nuevo_pcb->data_cond.mutex_cond, NULL);
        pthread_cond_init(&nuevo_pcb->data_cond.cond, NULL);

        pthread_mutex_init(&nuevo_pcb->mutex, NULL);
        nuevo_pcb->evt_actual = NULL;
    }
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

t_evt* iniciar_evt_std_in(int tamanio, int base, t_pcb* proceso){
    t_evt* evt = malloc(sizeof(t_evt));
    evt->proceso = proceso;
    evt->syscall_finalizada = false;
    pthread_cond_init(&evt->cond, NULL);
    pthread_mutex_init(&evt->mutex, NULL);
    t_evt_std_in* evt_std_in = malloc(sizeof(*evt_std_in));
    evt_std_in->tamanio = tamanio;
    evt_std_in->base = base;
    evt->data_evt = evt_std_in;
    return evt;
}

t_evt* iniciar_evt_std_out(char* datos_leidos, t_pcb* proceso){
    t_evt* evt = malloc(sizeof(t_evt));
    evt->proceso = proceso;
    evt->syscall_finalizada = false;
    pthread_cond_init(&evt->cond, NULL);
    pthread_mutex_init(&evt->mutex, NULL);
    t_evt_std_out* evt_std_in_out = malloc(sizeof(*evt_std_in_out));
    evt_std_in_out->datos_leidos = datos_leidos;
    evt->data_evt = evt_std_in_out;
    return evt;
}

// funciones para modificar
void cambiar_prioridad(t_pcb* proceso, int prioridad){
    if(algoritmo != CMN){
        log_debug(logger, "Algoritmo no es CMN, no aplica el cambio de prioridad");
        return;
    }
    if(list_size(queues_algorithms)>prioridad){
        log_info(logger, "## <%d> Herencia de prioridad: Pasa de %d a %d", proceso->pid, proceso->prioridad_actual, prioridad);
        pthread_mutex_lock(&proceso->mutex);
        proceso->prioridad_actual = prioridad;
        pthread_mutex_unlock(&proceso->mutex);
    }else{
        log_error(logger, "Error: Intento de pasar al proceso <%d> a una prioridad invalida (%d)", proceso->pid, prioridad);
        exit(EXIT_FAILURE);
    }

}

void cambiar_de_evt(t_pcb* proceso, t_evt* evt){
    pthread_mutex_lock(&proceso->mutex);
    proceso->evt_actual = evt;
    pthread_mutex_unlock(&proceso->mutex);
}

void cambiar_estado_global(t_estado_sch e){
    pthread_mutex_lock(&m_estado_global);
    estado_global = e;
    pthread_mutex_unlock(&m_estado_global);
}

// funciones par destroy
void destroy_pcb(t_pcb* pcb){ free(pcb); }

// mover entre estados
void generica_transicion_de_estados(t_pcb* proceso, t_lista_estado* l_remove, t_lista_estado* l_add){
    pthread_mutex_lock(&m_transicionar);
    pthread_mutex_lock(&proceso->mutex);

    bool eliminado = eliminar_proceso_de_lista(l_remove->sublista, proceso, l_remove->nombre, &l_remove->mutex);
    if(eliminado){
        agregar_proceso_a_lista(l_add->sublista, proceso, l_add->tipo, &l_add->mutex);
        log_obligatorio_cambio_de_estado(proceso->pid, l_remove->nombre, l_add->nombre);
    }
    pthread_mutex_unlock(&proceso->mutex);
    pthread_mutex_unlock(&m_transicionar);
}

void blocked_a_susp_blocked(t_pcb* proceso){
    generica_transicion_de_estados(proceso, estado_blocked, estado_susp_blocked);
}

void susp_blocked_a_susp_ready(t_pcb* proceso){
    generica_transicion_de_estados(proceso, estado_susp_blocked, estado_susp_ready);
}

void susp_ready_a_ready(t_pcb* proceso){
    pthread_mutex_lock(&m_transicionar);
    pthread_mutex_lock(&proceso->mutex);
    bool eliminado = eliminar_proceso_de_lista(estado_susp_ready->sublista, proceso, "estado_susp_ready->sublista", &estado_susp_ready->mutex);

    if(eliminado){
        agregar_a_ready(proceso);
        log_obligatorio_cambio_de_estado(proceso->pid, "SUSP_READY", "READY");
    }
    pthread_mutex_unlock(&proceso->mutex);
    pthread_mutex_unlock(&m_transicionar);
}

void ready_a_exec(t_pcb* proceso){
    pthread_mutex_lock(&m_transicionar);
    pthread_mutex_lock(&proceso->mutex);
    bool eliminado = eliminar_de_ready(proceso);
    if(eliminado){
        agregar_proceso_a_lista(estado_exec->sublista, proceso, EJECUTANDO, &estado_exec->mutex);
        log_obligatorio_cambio_de_estado(proceso->pid, "READY", "EXEC");
    }
    pthread_mutex_unlock(&proceso->mutex);
    pthread_mutex_unlock(&m_transicionar);
}

void blocked_a_ready(t_pcb* proceso){
    pthread_mutex_lock(&m_transicionar);
    pthread_mutex_lock(&proceso->mutex);
    bool eliminado = eliminar_proceso_de_lista(estado_blocked->sublista, proceso, "lista_blocked", &estado_blocked->mutex);

    if(eliminado){
        agregar_a_ready(proceso);
        log_obligatorio_cambio_de_estado(proceso->pid, "BLOCKED", "READY");
    }
    pthread_mutex_unlock(&proceso->mutex);
    pthread_mutex_unlock(&m_transicionar);
}

void exec_a_blocked(t_pcb* proceso){
    generica_transicion_de_estados(proceso, estado_exec, estado_blocked);
}

void exec_a_blocked_cond_signal(t_pcb* proceso){
    if(algoritmo_de_proceso(proceso) != RR){
        exec_a_blocked(proceso);
        return;
    }
    pthread_mutex_lock(&proceso->data_cond.mutex_cond);
    
    exec_a_blocked(proceso);
    proceso->data_cond.cond_val = true;
    pthread_cond_signal(&proceso->data_cond.cond);

    pthread_mutex_unlock(&proceso->data_cond.mutex_cond);
}

void exec_a_ready(t_pcb* proceso){
    pthread_mutex_lock(&m_transicionar);
    pthread_mutex_lock(&proceso->mutex);
    bool eliminado = eliminar_proceso_de_lista(estado_exec->sublista, proceso, "lista_exec", &estado_exec->mutex);
    if(eliminado){
        agregar_a_ready(proceso);
        log_obligatorio_cambio_de_estado(proceso->pid, "EXEC", "READY");
    }
    pthread_mutex_unlock(&proceso->mutex);
    pthread_mutex_unlock(&m_transicionar);
}

void exec_a_ready_cond_signal(t_pcb* proceso){
    pthread_mutex_lock(&proceso->data_cond.mutex_cond);

    exec_a_ready(proceso);
    proceso->data_cond.cond_val = true;
    pthread_cond_signal(&proceso->data_cond.cond);

    pthread_mutex_unlock(&proceso->data_cond.mutex_cond);
}

void new_a_ready(t_pcb* proceso){
    pthread_mutex_lock(&m_transicionar);
    pthread_mutex_lock(&proceso->mutex);
    bool eliminado = eliminar_proceso_de_lista(estado_new->sublista, proceso, "lista_new", &estado_new->mutex);

    if(eliminado){
        agregar_a_ready(proceso);
        log_obligatorio_cambio_de_estado(proceso->pid, "NEW", "READY");
    }
    pthread_mutex_unlock(&proceso->mutex);
    pthread_mutex_unlock(&m_transicionar);
}

void exec_a_exit(t_pcb* proceso){
    generica_transicion_de_estados(proceso, estado_exec, estado_exit);
}

void proceso_a_new(t_pcb* proceso){
    pthread_mutex_lock(&m_transicionar);
    pthread_mutex_lock(&proceso->mutex);
    agregar_proceso_a_lista(estado_new->sublista, proceso, NUEVO, &estado_new->mutex);
    log_info(logger, "## (<%d>) Se crea el proceso - Estado: NEW", proceso->pid);
    pthread_mutex_unlock(&proceso->mutex);
    pthread_mutex_unlock(&m_transicionar);
}

void agregar_a_ready_thread_safe(t_pcb* proceso){
    pthread_mutex_lock(&proceso->mutex);
    agregar_a_ready(proceso);
    pthread_mutex_unlock(&proceso->mutex);
}

void eliminar_de_ready_thread_safe(t_pcb* proceso){
    pthread_mutex_lock(&proceso->mutex);
    eliminar_de_ready(proceso);
    pthread_mutex_unlock(&proceso->mutex);
}

void agregar_a_ready(t_pcb* proceso){
    if(algoritmo == CMN){
        agregar_a_ready_CMN(proceso);
    }else{
        agregar_proceso_a_lista(estado_ready->sublista, proceso, LISTO, &estado_ready->mutex);
    }    
    pthread_mutex_lock(&m_procesos_en_ready);
    procesos_en_ready++;
    pthread_mutex_unlock(&m_procesos_en_ready);
    
    intentar_planificar();
    log_debug(logger, "Hice sem_post de s_nuevo_proceso_ready");
}

void agregar_a_ready_CMN(t_pcb* proceso){
    if(proceso == NULL){
        log_error(logger, "proceso NULL en agregar_a_ready_CMN");
        return;
    }
    t_sublista_ready* cola_prioridad = sublista_ready_de_prioridad(proceso->prioridad_actual);
    agregar_proceso_a_lista(cola_prioridad->sublista, proceso, LISTO, &cola_prioridad->mutex);
}

bool eliminar_de_ready(t_pcb* proceso){
    bool eliminado;
    if(algoritmo == CMN){
        eliminado = eliminar_de_ready_CMN(proceso);
    }else{
        eliminado = eliminar_proceso_de_lista(estado_ready->sublista, proceso, "lista_ready", &estado_ready->mutex);
    }
    if(eliminado){
        pthread_mutex_lock(&m_procesos_en_ready);
        procesos_en_ready--;
        pthread_mutex_unlock(&m_procesos_en_ready);
    }

    return eliminado;
}

bool eliminar_de_ready_CMN(t_pcb* proceso){
    t_sublista_ready* cola = sublista_ready_de_prioridad(proceso->prioridad_actual);
    return eliminar_proceso_de_lista(cola->sublista, proceso, "cola de prioridad de ready", &cola->mutex);
}

bool eliminar_proceso_de_lista(t_list* lista, t_pcb* proceso, char* nombre_lista, pthread_mutex_t* m){
    if (proceso == NULL) {
        log_error(logger, "proceso NULL en eliminar_proceso_de_lista");
        return false;
    }
    pthread_mutex_lock(m);
    bool removido = list_remove_element(lista, proceso);
    pthread_mutex_unlock(m);
    if(!removido){
        log_error(logger, "El proceso PID %d no estaba en %s", proceso->pid, nombre_lista);
    }
    return removido;
}

void agregar_proceso_a_lista(t_list* lista, t_pcb* proceso, t_tipo_estado nuevo_estado, pthread_mutex_t* m){
    if (proceso == NULL) {
        log_error(logger, "proceso NULL en agregar_proceso_a_lista");
        return;
    }
    pthread_mutex_lock(m);
    proceso->estado = nuevo_estado;
    list_add(lista, proceso);
    pthread_mutex_unlock(m);
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

t_sublista_ready* sublista_ready_de_prioridad(int prioridad){
    pthread_mutex_lock(&estado_ready->mutex);
    t_sublista_ready* cola_prioridad = list_get(estado_ready->sublista, prioridad);
    pthread_mutex_unlock(&estado_ready->mutex);
    if(cola_prioridad == NULL){
        log_error(logger, "error al obtener la cola de prioridad %d", prioridad);
        exit(EXIT_FAILURE); 
    }
    return cola_prioridad;
}

// para desbloquear procesos
void desbloquear_proceso(t_pcb* proceso){
    log_debug(logger, "desbloquear_proceso: ejecutando");

    pthread_mutex_lock(&proceso->mutex);
    t_tipo_estado estado = proceso->estado;
    pthread_mutex_unlock(&proceso->mutex);
        if(estado == SUSP_BLOQUEADO){
        // si esta en susp_blocked: mover a susp_ready
        susp_blocked_a_susp_ready(proceso);
        
    }else if(estado == BLOQUEADO){
        // si esta en blocked: mover a ready
        blocked_a_ready(proceso);
    }
}

void manejar_status_op(t_pcb* proceso, t_status_op status){
    switch (status){
        case OK:{
            desbloquear_proceso(proceso);
            
            break;
        
        }case ERROR:{
            log_error(logger, "Error: Caso de status error no manejado");
            break;
        }
    }
}

// obtener por clave

t_pcb* proceso_de_lista(int pid, t_list* list){
    for(int i = 0; i < list_size(list); i++){
        t_pcb* proceso = list_get(list, i);
        if(proceso->pid == pid){
            return proceso;
        }
    }
    return NULL;
}

t_tipo_estado get_estado(t_pcb* p){
    pthread_mutex_lock(&p->mutex);
    t_tipo_estado e = p->estado;
    pthread_mutex_unlock(&p->mutex);
    return e;
}

// liberar

void liberar_cpu(t_cpu* cpu){
    log_debug(logger, "liberando cpu de id %d", cpu->id);
    pthread_mutex_lock(&cpu->mutex);
    cpu->desalojando = false;
    cpu->proceso = NULL;
    intentar_planificar();
    pthread_mutex_unlock(&cpu->mutex);
}

void liberar_pcb_de_exit(int pid){
    pthread_mutex_lock(&estado_exit->mutex);
    for(int i = 0; i < list_size(estado_exit->sublista); i++){
        t_pcb* proceso = list_get(estado_exit->sublista, i);
        if(proceso->pid == pid){
            log_debug(logger, "libereando proc encontrado depid = %d", pid);
            list_remove(estado_exit->sublista, i);
            destroy_pcb(proceso);
            pthread_mutex_unlock(&estado_exit->mutex);
            return;
        }
    }
    pthread_mutex_unlock(&estado_exit->mutex);
    log_error(logger, "No se encontro el proceso PID <%d> en lista_exit", pid);
}

// funciones genericas

t_cpu* cpu_de_pid(int pid){
    pthread_mutex_lock(&m_lista_cpus);
    for(int i = 0; i < list_size(lista_cpus); i++){
        t_cpu* cpu = list_get(lista_cpus, i);
        if(cpu == NULL){
            continue;
        }
        pthread_mutex_lock(&cpu->mutex);
        bool es_el_pid = (
            cpu->proceso != NULL &&
            cpu->proceso->pid == pid
        );
        pthread_mutex_unlock(&cpu->mutex);

        if(es_el_pid){
            pthread_mutex_unlock(&m_lista_cpus);
            return cpu;
        }
    }
    pthread_mutex_unlock(&m_lista_cpus);
    return NULL;
}

t_planificacion obtener_algoritmo_planificacion(char *algoritmo_str){
    t_planificacion algo = algoritmo_str_a_enum(algoritmo_str);
    if(algo == -1){
        printf("%s no es un algoritmo invalido, proba con FIFO, RR o CMN", algoritmo_str);
        exit(EXIT_FAILURE);
    }
    return algo;
}

void intentar_planificar(){
    log_debug(logger, "Intentando planificar");
    pthread_mutex_lock(&m_estado_global);
    if(estado_global == PLANIF_ACTIVA){
        log_debug(logger, "planif activa");
        sem_post(&s_intentar_planificar);
    }else{
        log_debug(logger, "Compactando");
    }

    pthread_mutex_unlock(&m_estado_global);
}

void enviar_paquete_a_todas_las_cpus(t_paquete* paquete){
    for(int i = 0; i < list_size(lista_cpus); i++){
        
        t_cpu* cpu = list_get(lista_cpus, i);
        int cpu_fd = cpu->fd;
        log_info(logger,"%d", cpu_fd);
        enviar_paquete(paquete, cpu_fd);
    }
    eliminar_paquete(paquete);
    log_debug(logger, "Paquete enviado a todas las cpus");
}

void pedido_desalojo_a_todas_las_cpus(op_code cod_op){
    pthread_mutex_lock(&m_lista_cpus);
    for(int i = 0; i < list_size(lista_cpus); i++){
        t_cpu* cpu = list_get(lista_cpus, i);
        pthread_mutex_lock(&cpu->mutex);
        int cpu_fd = cpu->fd;
        cpu->desalojando = true;
        enviar_operacion(cpu_fd, cod_op);
        pthread_mutex_unlock(&cpu->mutex);
    }
    pthread_mutex_unlock(&m_lista_cpus);
    log_debug(logger, "Paquete enviado a todas las cpus");
}

bool hay_cpus_ejecutando(){
    pthread_mutex_lock(&m_lista_cpus);
    for(int i = 0; i < list_size(lista_cpus); i++){
        t_cpu* cpu = list_get(lista_cpus, i);
        pthread_mutex_lock(&cpu->mutex);
        if(cpu->proceso != NULL)
            return true;
        pthread_mutex_unlock(&cpu->mutex);
    }
    pthread_mutex_unlock(&m_lista_cpus);
    return false;
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
    // log_debug(logger, "new: %d, ready: %d, exec: %d, blocked: %d, susp_blocked: %d, susp_ready: %d, exit: %d"
    //                 , list_size(estado_new->sublista)
    //                 , procesos_en_ready
    //                 , list_size(estado_exec->sublista)
    //                 , list_size(estado_blocked->sublista)
    //                 , list_size(estado_susp_blocked->sublista)
    //                 , list_size(estado_susp_ready->sublista)
    //                 , list_size(estado_exit->sublista));
    imprimir_estado_procesos();

}

t_planificacion algoritmo_de_proceso(t_pcb* proceso){
    if(algoritmo != CMN){
        return algoritmo;
    }
    // para CMN:
    t_planificacion algo = *(t_planificacion*)list_get(queues_algorithms, proceso->prioridad_actual);
    return algo;
}

void imprimir_estado_procesos()
{
    pthread_mutex_lock(&m_transicionar);

    char* msg = string_new();

    string_append(&msg, "\n========== ESTADO DE PROCESOS ==========\n");

    string_append(&msg, "NEW: ");
    for(int i = 0; i < list_size(estado_new->sublista); i++){
        t_pcb* p = list_get(estado_new->sublista, i);
        string_append_with_format(&msg, "%d ", p->pid);
    }
    string_append(&msg, "\n");

    string_append(&msg, "READY: ");
    if(algoritmo == CMN){
        for(int i = 0; i < list_size(estado_ready->sublista); i++){
            t_sublista_ready* sub = list_get(estado_ready->sublista, i);

            string_append_with_format(&msg, "[P%d: ", i);

            for(int j = 0; j < list_size(sub->sublista); j++){
                t_pcb* p = list_get(sub->sublista, j);
                string_append_with_format(&msg, "%d ", p->pid);
            }

            string_append(&msg, "] ");
        }
    } else {
        for(int i = 0; i < list_size(estado_ready->sublista); i++){
            t_pcb* p = list_get(estado_ready->sublista, i);
            string_append_with_format(&msg, "%d ", p->pid);
        }
    }
    string_append(&msg, "\n");

    string_append(&msg, "EXEC: ");
    for(int i = 0; i < list_size(estado_exec->sublista); i++){
        t_pcb* p = list_get(estado_exec->sublista, i);
        string_append_with_format(&msg, "%d ", p->pid);
    }
    string_append(&msg, "\n");

    string_append(&msg, "BLOCKED: ");
    for(int i = 0; i < list_size(estado_blocked->sublista); i++){
        t_pcb* p = list_get(estado_blocked->sublista, i);
        string_append_with_format(&msg, "%d ", p->pid);
    }
    string_append(&msg, "\n");

    string_append(&msg, "SUSP_BLOCKED: ");
    for(int i = 0; i < list_size(estado_susp_blocked->sublista); i++){
        t_pcb* p = list_get(estado_susp_blocked->sublista, i);
        string_append_with_format(&msg, "%d ", p->pid);
    }
    string_append(&msg, "\n");

    string_append(&msg, "SUSP_READY: ");
    for(int i = 0; i < list_size(estado_susp_ready->sublista); i++){
        t_pcb* p = list_get(estado_susp_ready->sublista, i);
        string_append_with_format(&msg, "%d ", p->pid);
    }
    string_append(&msg, "\n");

    string_append(&msg, "EXIT: ");
    for(int i = 0; i < list_size(estado_exit->sublista); i++){
        t_pcb* p = list_get(estado_exit->sublista, i);
        string_append_with_format(&msg, "%d ", p->pid);
    }
    string_append(&msg, "\n");

    string_append(&msg, "========================================");

    log_info(logger, "%s", msg);

    free(msg);

    pthread_mutex_unlock(&m_transicionar);
}