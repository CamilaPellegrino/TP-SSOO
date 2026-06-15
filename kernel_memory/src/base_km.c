#include "base_km.h"
// inicializar cosas
void inicializar_variables_globales(t_config* config){
	puerto = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    scripts_basepath = config_get_string_value(config, "SCRIPTS_BASEPATH");

    t_log_level log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));
    logger = iniciar_logger("kernel_memory.log", "ProcesoKernelMemory", log_level);

    lista_sticks           = list_create();
    lista_cpus             = list_create();
    lista_segmentos_global = list_create();
    lista_huecos           = list_create();
    lista_procesos         = list_create();
    lista_eventos_stick    = list_create();

    algoritmo_fit = BEST_FIT;
    tamanio_total_mem = 0;
    tamanio_total_libre = 0;
    
    // semaforos
    sem_init(&s_lista_eventos_stick, 0, 0);
    sem_init(&s_fin_mover, 0, 0);

    // mutex
    pthread_mutex_init(&m_lista_segmentos_global, NULL);
    pthread_mutex_init(&m_lista_procesos, NULL);
    pthread_mutex_init(&m_lista_huecos, NULL);
    pthread_mutex_init(&m_lista_eventos_stick, NULL);
    pthread_mutex_init(&m_manejar_memoria, NULL);
}

t_proceso* iniciar_proceso(t_pcb* pcb, t_list* instrucciones){
	if(instrucciones == NULL || pcb == NULL){ return NULL; }
    t_proceso* proceso = malloc(sizeof(t_proceso));
	if(proceso == NULL){ return NULL; }
    proceso->pcb = pcb;
    proceso->instrucciones = instrucciones;
    proceso->lista_segmentos = list_create();
    return proceso;
}

t_stick* iniciar_stick(char* ip, char* puerto, int tamanio, int cliente_fd){
	t_stick* nuevo_stick = malloc(sizeof(t_stick));
	if (nuevo_stick != NULL) {
		nuevo_stick->ip = strdup(ip);
		nuevo_stick->puerto = strdup(puerto);
		nuevo_stick->fd = cliente_fd;
		nuevo_stick->tamanio = tamanio;
        // nuevo_stick->lista_evt = list_create();
        // pthread_mutex_init(&nuevo_stick->m_lista_evt, NULL);
        // sem_init(&nuevo_stick->s_list_evt, 0, 0);
	}
	return nuevo_stick;
}

void destruir_stick(t_stick* stick){
	free(stick);
}

t_cpu* iniciar_cpu(int id, int fd){
	t_cpu* nuevo_cpu = malloc(sizeof(t_stick));
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

t_evt* iniciar_evt_read(int pid, int base, int tamanio){
    t_evt* evt = malloc(sizeof(t_evt));

    evt->tipo = SCH_LECTURA;
    evt->pid = pid;

    t_data_read* data = malloc(sizeof(t_data_read));
    data->base = base;
    data->tamanio = tamanio;

    evt->data = data;

    pthread_mutex_init(&evt->mutex, NULL);

    return evt;
}

t_evt* iniciar_evt_write(int pid, int base, int tamanio, void* bytes){
    t_evt* evt = malloc(sizeof(t_evt));

    evt->tipo = SCH_ESCRITURA;
    evt->pid = pid;

    t_data_write* data = malloc(sizeof(t_data_write));
    data->base = base;
    data->bytes = bytes;
    data->tamanio = tamanio;

    evt->data = data;

    pthread_mutex_init(&evt->mutex, NULL);

    return evt;
}


t_evt* iniciar_evt_mover(int pid, int base_leer, int tamanio, int base_escribir){
    t_evt* evt = malloc(sizeof(t_evt));
    evt->pid = pid;
    evt->tipo = SCH_MOVER;

    t_data_mover* data = malloc(sizeof(t_data_mover));
    data->base_leer = base_leer;
    data->tamanio = tamanio;
    data->base_escribir = base_escribir;
    
    evt->data = data;
    
    pthread_mutex_init(&evt->mutex, NULL);

    return evt;
}
// ...
t_proceso* proceso_de_pid(int pid){
    for(int i = 0; i < list_size(lista_procesos); i++){
        t_proceso* proceso = list_get(lista_procesos, i);
        if(proceso->pcb->pid == pid){
            return proceso;
        }
    }
    return NULL;
}

t_segmento* segmento_de_id(int id_segmento){
    pthread_mutex_lock(&m_lista_segmentos_global);

    for(int i = 0; i < list_size(lista_segmentos_global); i++){
        t_segmento* s = list_get(lista_segmentos_global, i);

        if(s->id_segmento == id_segmento){
            pthread_mutex_unlock(&m_lista_segmentos_global);
            return s;
        }
    }

    pthread_mutex_unlock(&m_lista_segmentos_global);
    return NULL;
}

t_stick* stick_por_id(int nro_stick){
    if(nro_stick >= list_size(lista_sticks)){
        return NULL;
    }
    return (t_stick*)list_get(lista_sticks, nro_stick);
}
bool guardar_nuevo_proceso(int pid, int ppid, char* ruta){
    t_pcb* pcb = calloc(1, sizeof(t_pcb));
    if (pcb == NULL) {
        return NULL;
    }
    pcb->pid = pid;
    pcb->ppid = ppid;
    t_list* instrucciones = instrucciones_de_ruta(ruta);
    t_proceso* proceso = iniciar_proceso(pcb, instrucciones);
    if(proceso == NULL){
        return false;
    }
    list_add(lista_procesos, proceso);
    log_info(logger, "Nuevo proceso de PID <%d> guardado", pcb->pid);
    return true;
}

void agregar_evt_a_stick(t_evt* evt){
    pthread_mutex_lock(&m_lista_eventos_stick);
    list_add(lista_eventos_stick, evt);
    pthread_mutex_unlock(&m_lista_eventos_stick);
    sem_post(&s_lista_eventos_stick);
}

void agregar_stick_a_paquete(t_paquete* paquete, t_stick* stick){
    agregar_a_paquete(paquete, &(stick->tamanio), sizeof(int));
    agregar_string_a_paquete(paquete, stick->puerto);
    agregar_string_a_paquete(paquete, stick->ip);
}

void agregar_stick(t_stick* stick){
    list_add(lista_sticks, stick);
    tamanio_total_mem += stick->tamanio;
    tamanio_total_libre += stick->tamanio;
}

void enviar_nuevo_stick_a_scheduler(t_stick* nuevo_stick, int sch_fd){
    t_paquete* paquete = crear_paquete(KM_SCH__NUEVO_STICK);
    agregar_string_a_paquete(paquete, nuevo_stick->ip);
    agregar_string_a_paquete(paquete, nuevo_stick->puerto);
    agregar_a_paquete(paquete, &(nuevo_stick->tamanio), sizeof(int));
    enviar_paquete_y_liberarlo(paquete, sch_fd);
    log_info(logger, "Enviando stick con IP %s al SCHED por el FD %d",nuevo_stick->ip, sch_fd);
}

void enviar_sticks_a_cpu(int cpu_fd){
    t_paquete* paquete = crear_paquete(KM_CPU__STICKS);
    int cant_sticks = list_size(lista_sticks);
    agregar_a_paquete(paquete, &cant_sticks, sizeof(int));
    for(int i = 0; i < cant_sticks; i++){
        t_stick* stick = list_get(lista_sticks, i);
        agregar_stick_a_paquete(paquete, stick);
    }
    enviar_paquete_y_liberarlo(paquete, cpu_fd);
}

void recibir_pcb_actualizado(t_list* valores){
    int i = 0;
    // pcb
    int* pid = list_get(valores, i++);
    t_proceso* proc = proceso_de_pid(*pid);
    t_pcb* pcb = proc->pcb;

    if(proc == NULL){
        log_error(logger, "recibir_pcb_actualizado, Error: No se encontro proceso de pid %d", *pid);
        list_destroy_and_destroy_elements(valores, free);
        return;
    }
    memcpy(&pcb->pid, pid, sizeof(pcb->pid));
    memcpy(&pcb->ppid, list_get(valores, i++), sizeof(pcb->ppid));
    memcpy(&pcb->prioridad, list_get(valores, i++), sizeof(pcb->prioridad));
    // registros
    memcpy(&pcb->pc, list_get(valores, i++), sizeof(pcb->pc));

    memcpy(&pcb->ax, list_get(valores, i++), sizeof(pcb->ax));
    memcpy(&pcb->bx, list_get(valores, i++), sizeof(pcb->bx));
    memcpy(&pcb->cx, list_get(valores, i++), sizeof(pcb->cx));
    memcpy(&pcb->dx, list_get(valores, i++), sizeof(pcb->dx));

    memcpy(&pcb->eax, list_get(valores, i++), sizeof(pcb->eax));
    memcpy(&pcb->ebx, list_get(valores, i++), sizeof(pcb->ebx));
    memcpy(&pcb->ecx, list_get(valores, i++), sizeof(pcb->ecx));
    memcpy(&pcb->edx, list_get(valores, i++), sizeof(pcb->edx));

    memcpy(&pcb->si, list_get(valores, i++), sizeof(pcb->si));
    memcpy(&pcb->di, list_get(valores, i++), sizeof(pcb->di));

    list_destroy_and_destroy_elements(valores, free);
    return;
}

void agregar_pcb_al_paquete(t_pcb* pcb, t_paquete* p){
    // datos del pcb
    agregar_a_paquete(p, &pcb->pid, sizeof(pcb->pid));
    agregar_a_paquete(p, &pcb->ppid, sizeof(pcb->ppid));
    agregar_a_paquete(p, &pcb->prioridad, sizeof(pcb->prioridad));

    // registros
    agregar_a_paquete(p, &pcb->pc, sizeof(pcb->pc));

    agregar_a_paquete(p, &pcb->ax, sizeof(pcb->ax));
    agregar_a_paquete(p, &pcb->bx, sizeof(pcb->bx));
    agregar_a_paquete(p, &pcb->cx, sizeof(pcb->cx));
    agregar_a_paquete(p, &pcb->dx, sizeof(pcb->dx));

    agregar_a_paquete(p, &pcb->eax, sizeof(pcb->eax));
    agregar_a_paquete(p, &pcb->ebx, sizeof(pcb->ebx));
    agregar_a_paquete(p, &pcb->ecx, sizeof(pcb->ecx));
    agregar_a_paquete(p, &pcb->edx, sizeof(pcb->edx));

    agregar_a_paquete(p, &pcb->si, sizeof(pcb->si));
    agregar_a_paquete(p, &pcb->di, sizeof(pcb->di));

}

void agregar_segmentos_al_paquete(t_list* segmentos, t_paquete* p){
    int cantidad = list_size(segmentos);
    agregar_a_paquete(p, &cantidad, sizeof(cantidad));

    for (int i = 0; i < cantidad; i++) {
        t_segmento* seg = list_get(segmentos, i);
        agregar_a_paquete(p, seg, sizeof(t_segmento));
    }
}
// Manejo de rutas
t_list* instrucciones_de_ruta(char* nombre_archivo){
    char* ruta = ruta_completa(scripts_basepath, nombre_archivo);
    FILE* archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        return NULL;
    }
    t_list* lineas = list_create();
    char* linea = NULL;
    size_t len = 0;
    ssize_t leidos;

    while ((leidos = getline(&linea, &len, archivo)) != -1) {
        if (leidos > 0 && linea[leidos - 1] == '\n') {
            linea[leidos - 1] = '\0';
        }
        list_add(lineas, strdup(linea));
    }
    free(ruta);
    free(linea);
    fclose(archivo);
    return lineas;
}

char* ruta_completa(char* base, char* nombre_archivo){
	size_t len1 = strlen(base);
    size_t len2 = strlen(nombre_archivo);

    char* resultado = malloc(len1 + len2 + 2);

    if (resultado == NULL) {
        return NULL;
    }

    sprintf(resultado, "%s/%s", base, nombre_archivo);
    return resultado;
}
// liberar

void liberar_evt(t_evt* evt){
    free(evt->data);
    free(evt);
}
// Otros


void imprimir_bytes(void* data, int tamanio){
    uint8_t* bytes = (uint8_t*) data;
        printf("Bytes: ");
        for(int j = 0; j < tamanio; j++) {
            printf("%02X ", bytes[j]);
        }

    printf("\n");
}

void imprimir_estado_mem(){
    imprimir_segmentos();
    imprimir_huecos();
}

void imprimir_huecos(){
    printf("HUECOS:\n");
    for(int i = 0; i < list_size(lista_huecos); i++){
        t_hueco* h = list_get(lista_huecos, i);
        if(h == NULL){
        }
        printf("Hueco %d -> base: %d | tam: %u | ult_dir: %d\n", i, h->base, h->tamanio, h->base + h->tamanio - 1);
    }
}

void imprimir_segmentos(){
    printf("SEGMENTOS:\n");
    pthread_mutex_lock(&m_lista_segmentos_global);
    for(int i = 0; i < list_size(lista_segmentos_global); i++){
        t_segmento* s = list_get(lista_segmentos_global, i);
        printf(
            "Segmento %d -> base: %d | tam: %u | ult_dir: %d\n",
            s->id_segmento,
            s->base,
            s->tamanio,
            s->base + s->tamanio - 1
        );
    }
    pthread_mutex_unlock(&m_lista_segmentos_global);
}