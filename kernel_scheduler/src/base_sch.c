#include "base_sch.h"
#include "maquina_estados.h"
t_planificacion algoritmo_str_a_enum(char *algoritmo_str);

// inicializar  
void inicializar_variables_globales(t_config* config){
    inicializar_parametros_de_config(config);


    // inicializar listas de eventos
    lista_evt_sleep    = list_create();
    lista_evt_stdin    = list_create();
    lista_evt_stdout   = list_create();

    // inicializar listas de otras cosas 
    lista_cpus         = list_create();
    lista_mutex        = list_create();

    // inicializar semaforos
    sem_init(&s_intentar_planificar, 0, 0);
    sem_init(&s_nuevo_proceso_new, 0, 0);
    sem_init(&s_evt_sleep, 0, 0);
    sem_init(&s_evt_stdin, 0, 0);
    sem_init(&s_evt_stdout, 0, 0);

    // inicializar mutexes
    pthread_mutex_init(&m_lista_evt_sleep, NULL);
    pthread_mutex_init(&m_lista_evt_stdin, NULL);
    pthread_mutex_init(&m_lista_evt_stdout, NULL);

    pthread_mutex_init(&m_lista_mutex, NULL);
    pthread_mutex_init(&m_lista_cpus, NULL);
    pthread_mutex_init(&m_estado_global, NULL);
    inicializar_maquina_estados();

    //otras cosas
    pthread_mutex_lock(&m_proximo_pid);
    proximo_pid = 0;
    pthread_mutex_unlock(&m_proximo_pid);
    pthread_mutex_lock(&m_estado_global);
    estado_global = PLANIF_ACTIVA;
    pthread_mutex_unlock(&m_estado_global);
}   

void inicializar_parametros_de_config(t_config* config){
    // leer de config
    log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));
    logger = iniciar_logger("kernel_scheduler.log", "ProcesoKernelScheduler", log_level);
    queue_preemption = queue_preemption_from_string(config_get_string_value(config, "QUEUE_PREEMPTION"));
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
        nuevo_pcb->status = OK;
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

t_evt* iniciar_evt_std_in(int tamanio, t_dir_fisica dir_fisica, t_pcb* proceso){
    t_evt* evt = malloc(sizeof(t_evt));
    evt->proceso = proceso;
    evt->syscall_finalizada = false;
    pthread_cond_init(&evt->cond, NULL);
    pthread_mutex_init(&evt->mutex, NULL);
    t_evt_std_in* evt_std_in = malloc(sizeof(*evt_std_in));
    evt_std_in->tamanio = tamanio;
    evt_std_in->dir_fisica = dir_fisica;
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

t_evt* iniciar_evt_mutex_lock(t_pcb* proceso){
    t_evt* evt = malloc(sizeof(t_evt));
    evt->proceso = proceso;
    evt->syscall_finalizada = false;
    pthread_cond_init(&evt->cond, NULL);
    pthread_mutex_init(&evt->mutex, NULL);
    evt->data_evt = NULL;
    return evt;
}


// funciones para destroy
void destroy_pcb(t_pcb* pcb){ free(pcb); }

void suspender_proceso(t_pcb* proceso){
    int pid = get_pid_thread_safe(proceso);
    t_paquete* p = crear_paquete(SCH_KM__SUSPENDER);
    agregar_a_paquete(p, &pid, sizeof(pid));
    enviar_paquete_y_liberarlo(p, conexion_kernel_memory);
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

// obtener por clave y getters

t_tipo_estado get_estado(t_pcb* p){
    pthread_mutex_lock(&p->mutex);
    t_tipo_estado e = p->estado;
    pthread_mutex_unlock(&p->mutex);
    return e;
}

int get_prioridad_actual_thread_safe(t_pcb* p){
    if(p == NULL){return -1;};
    pthread_mutex_lock(&p->mutex);
    int n = p->prioridad_actual;
    pthread_mutex_unlock(&p->mutex);
    return n;
}

int get_pid_thread_safe(t_pcb* p){
    int ret = -1;
    if(p == NULL){
        return ret;
    }
    pthread_mutex_lock(&p->mutex);
    ret = p->pid;
    pthread_mutex_unlock(&p->mutex);
    return ret;
}

t_pcb* get_proceso_de_cpu_thread_safe(t_cpu*cpu){
    t_pcb* pcb = NULL;
    pthread_mutex_lock(&cpu->mutex);
    pcb = cpu->proceso;
    pthread_mutex_unlock(&cpu->mutex);
    return pcb;
}

int get_pid_de_proceso_de_cpu_thread_safe(t_cpu* cpu){
    t_pcb* p = get_proceso_de_cpu_thread_safe(cpu);
    return get_pid_thread_safe(p);
}

t_cpu* cpu_de_pid(int pid){
    pthread_mutex_lock(&m_lista_cpus);
    for(int i = 0; i < list_size(lista_cpus); i++){
        t_cpu* cpu = list_get(lista_cpus, i);
        if(cpu == NULL){
            continue;
        }
        t_pcb* proc = get_proceso_de_cpu_thread_safe(cpu);
        bool es_el_pid = (proc != NULL && get_pid_thread_safe(proc) == pid);

        if(es_el_pid){
            pthread_mutex_unlock(&m_lista_cpus);
            return cpu;
        }
    }
    pthread_mutex_unlock(&m_lista_cpus);
    return NULL;
}

t_evt* get_evt_de_proceso(t_pcb* proceso){
    t_evt* ret = NULL;
    pthread_mutex_lock(&proceso->mutex);
    ret = proceso->evt_actual;
    pthread_mutex_unlock(&proceso->mutex);
    return ret;
}

t_status_op get_status(t_pcb* proceso){
    pthread_mutex_lock(&proceso->mutex);
    t_status_op s = proceso->status;
    pthread_mutex_unlock(&proceso->mutex);
    return s;
}

// funciones para modificar y setters

void set_status(t_pcb* pcb, t_status_op status){
    if(status != OK){
        pthread_mutex_lock(&pcb->mutex);
        pcb->status = status;
        pthread_mutex_unlock(&pcb->mutex);
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

void set_proceso_thread_safe(t_cpu* cpu, t_pcb* proceso){
    pthread_mutex_lock(&cpu->mutex);
    cpu->proceso = proceso;
    pthread_mutex_unlock(&cpu->mutex);
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

void liberar_evt(t_evt* evt, void (*free_data)(void*)){
        pthread_mutex_destroy(&evt->mutex);
        pthread_cond_destroy(&evt->cond);
        free_data(evt->data_evt);
        free(evt);
}

void liberar_evt_stdout(void* arg){
    t_evt_std_out* data = (t_evt_std_out*)arg;
    free(data->datos_leidos);
    free(data);
}

// funciones genericas

void finalizar_evento(t_evt* evt){
    pthread_mutex_lock(&evt->mutex);

    evt->syscall_finalizada = true;
    pthread_cond_signal(&evt->cond);
    
    pthread_mutex_unlock(&evt->mutex);
    pthread_join(evt->hilo_timeout, NULL); 
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
        log_debug(logger,"%d", cpu_fd);
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
        if(get_proceso_de_cpu_thread_safe(cpu) != NULL){
            pthread_mutex_unlock(&m_lista_cpus);
            return true;
        }
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

t_planificacion algoritmo_de_proceso(t_pcb* proceso){
    if(algoritmo != CMN){
        return algoritmo;
    }
    // para CMN:
    t_planificacion algo = *(t_planificacion*)list_get(queues_algorithms, proceso->prioridad_actual);
    return algo;
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

bool queue_preemption_from_string(char* str){
    return strcmp(str, "TRUE") == 0;
}

// logs

void log_obligatorio_cambio_de_estado(int pid, char* estado_anterior, char* estado_actual){
    log_info(logger, "## (%d) Pasa del estado %s al estado %s", pid, estado_anterior, estado_actual);
}
