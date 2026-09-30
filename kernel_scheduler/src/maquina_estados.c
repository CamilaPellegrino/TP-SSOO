#include "maquina_estados.h"

typedef struct {
	t_list* sublista;
	pthread_mutex_t mutex;
} t_sublista_ready;

typedef struct {
	t_list* sublista;
	pthread_mutex_t mutex;
	char* nombre;
	t_tipo_estado tipo;
} t_lista_estado;

static t_lista_estado* estado_new;
static t_lista_estado* estado_ready;
static t_lista_estado* estado_blocked;
static t_lista_estado* estado_exec;
static t_lista_estado* estado_susp_blocked;
static t_lista_estado* estado_susp_ready;
static t_lista_estado* estado_exit;
static pthread_mutex_t m_transicionar;
static pthread_mutex_t m_procesos_en_ready;
static int procesos_en_ready;

static void agregar_proceso_a_lista(t_list* lista, t_pcb* proceso, t_tipo_estado nuevo_estado, pthread_mutex_t* mutex);
static bool eliminar_proceso_de_lista(t_list* lista, t_pcb* proceso, char* nombre_lista, pthread_mutex_t* mutex);
static void agregar_a_ready(t_pcb* proceso);
static bool eliminar_de_ready(t_pcb* proceso);
static t_sublista_ready* sublista_ready_de_prioridad(int prioridad);

static t_lista_estado* inicializar_lista_estado(char* nombre, t_tipo_estado tipo){
	t_lista_estado* estado = malloc(sizeof(*estado));
	estado->nombre = nombre;
	estado->sublista = list_create();
	estado->tipo = tipo;
	pthread_mutex_init(&estado->mutex, NULL);
	return estado;
}

void inicializar_maquina_estados(void){
	pthread_mutex_init(&m_transicionar, NULL);
	pthread_mutex_init(&m_procesos_en_ready, NULL);
	procesos_en_ready = 0;

	estado_ready = malloc(sizeof(*estado_ready));
	estado_ready->nombre = "lista_ready";
	estado_ready->sublista = list_create();
	estado_ready->tipo = LISTO;
	if(algoritmo == CMN){
		for(int i = 0; i < list_size(queues_algorithms); i++){
			t_sublista_ready* sublista = malloc(sizeof(*sublista));
			sublista->sublista = list_create();
			pthread_mutex_init(&sublista->mutex, NULL);
			list_add(estado_ready->sublista, sublista);
		}
	}
	pthread_mutex_init(&estado_ready->mutex, NULL);
	estado_new = inicializar_lista_estado("NEW", NUEVO);
	estado_exec = inicializar_lista_estado("EXEC", EJECUTANDO);
	estado_blocked = inicializar_lista_estado("BLOCKED", BLOQUEADO);
	estado_susp_blocked = inicializar_lista_estado("SUSP_BLOCKED", SUSP_BLOQUEADO);
	estado_susp_ready = inicializar_lista_estado("SUSP_READY", SUSP_LISTO);
	estado_exit = inicializar_lista_estado("EXIT", FINALIZADO);
}

void bloquear_maquina_estados(void){
	pthread_mutex_lock(&m_transicionar);
}

void desbloquear_maquina_estados(void){
	pthread_mutex_unlock(&m_transicionar);
}

int cantidad_procesos_ready(void){
	pthread_mutex_lock(&m_procesos_en_ready);
	int cantidad = procesos_en_ready;
	pthread_mutex_unlock(&m_procesos_en_ready);
	return cantidad;
}

static t_list* lista_de_tipo_estado(t_tipo_estado tipo){
	switch(tipo){
		case NUEVO: return estado_new->sublista;
		case LISTO: return estado_ready->sublista;
		case BLOQUEADO: return estado_blocked->sublista;
		case SUSP_BLOQUEADO: return estado_susp_blocked->sublista;
		case SUSP_LISTO: return estado_susp_ready->sublista;
		case FINALIZADO: return estado_exit->sublista;
		case EJECUTANDO: return estado_exec->sublista;
		default: return NULL;
	}
}

t_pcb* get_proceso_de_pid_thread_safe(int pid){
	const t_tipo_estado estados[] = {NUEVO, LISTO, BLOQUEADO, SUSP_BLOQUEADO, SUSP_LISTO, FINALIZADO};
	for(size_t e = 0; e < sizeof(estados) / sizeof(estados[0]); e++){
		t_list* lista = lista_de_tipo_estado(estados[e]);
		pthread_mutex_lock(&m_transicionar);
		for(int i = 0; i < list_size(lista); i++){
			t_pcb* proceso = list_get(lista, i);
			if(proceso->pid == pid){
				pthread_mutex_unlock(&m_transicionar);
				return proceso;
			}
		}
		pthread_mutex_unlock(&m_transicionar);
	}
	return NULL;
}

t_pcb* proceso_new_siguiente(void){
	pthread_mutex_lock(&estado_new->mutex);
	t_pcb* proceso = list_is_empty(estado_new->sublista) ? NULL : list_get(estado_new->sublista, 0);
	pthread_mutex_unlock(&estado_new->mutex);
	return proceso;
}

t_pcb* desencolar_ready_cmn(void){
	pthread_mutex_lock(&estado_ready->mutex);
	for(int i = 0; i < list_size(estado_ready->sublista); i++){
		t_sublista_ready* sublista = list_get(estado_ready->sublista, i);
		pthread_mutex_lock(&sublista->mutex);
		if(!list_is_empty(sublista->sublista)){
			t_pcb* proceso = list_remove(sublista->sublista, 0);
			pthread_mutex_lock(&m_procesos_en_ready);
			procesos_en_ready--;
			pthread_mutex_unlock(&m_procesos_en_ready);
			pthread_mutex_unlock(&sublista->mutex);
			pthread_mutex_unlock(&estado_ready->mutex);
			return proceso;
		}
		pthread_mutex_unlock(&sublista->mutex);
	}
	pthread_mutex_unlock(&estado_ready->mutex);
	return NULL;
}

t_pcb* desencolar_ready_fifo_rr(void){
	pthread_mutex_lock(&estado_ready->mutex);
	if(list_is_empty(estado_ready->sublista)){
		pthread_mutex_unlock(&estado_ready->mutex);
		return NULL;
	}
	t_pcb* proceso = list_remove(estado_ready->sublista, 0);
	pthread_mutex_unlock(&estado_ready->mutex);
	pthread_mutex_lock(&m_procesos_en_ready);
	procesos_en_ready--;
	pthread_mutex_unlock(&m_procesos_en_ready);
	return proceso;
}

static void generica_transicion_de_estados(t_pcb* proceso, t_lista_estado* lista_origen, t_lista_estado* lista_destino){
	pthread_mutex_lock(&m_transicionar);
	pthread_mutex_lock(&proceso->mutex);

	bool eliminado = eliminar_proceso_de_lista(lista_origen->sublista, proceso, lista_origen->nombre, &lista_origen->mutex);
	if(eliminado){
		agregar_proceso_a_lista(lista_destino->sublista, proceso, lista_destino->tipo, &lista_destino->mutex);
		log_obligatorio_cambio_de_estado(proceso->pid, lista_origen->nombre, lista_destino->nombre);
	}
	pthread_mutex_unlock(&proceso->mutex);
	pthread_mutex_unlock(&m_transicionar);
}

void blocked_a_susp_blocked(t_pcb* proceso){
	generica_transicion_de_estados(proceso, estado_blocked, estado_susp_blocked);
}

void susp_blocked_a_susp_ready(t_pcb* proceso){
	generica_transicion_de_estados(proceso, estado_susp_blocked, estado_susp_ready);
	intentar_desuspender();
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

static void exec_a_blocked(t_pcb* proceso){
	generica_transicion_de_estados(proceso, estado_exec, estado_blocked);
}

void exec_a_blocked_cond_signal(t_pcb* proceso){
	log_debug(logger, "<%d> Exec a blocked cond signal", get_pid_thread_safe(proceso));
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

static void exec_a_ready(t_pcb* proceso){
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
	log_debug(logger, "<%d> Exec a ready cond signal", get_pid_thread_safe(proceso));
	exec_a_ready(proceso);
	pthread_mutex_lock(&proceso->data_cond.mutex_cond);
	proceso->data_cond.cond_val = true;
	pthread_cond_signal(&proceso->data_cond.cond);
	pthread_mutex_unlock(&proceso->data_cond.mutex_cond);
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
	log_info(logger, "## (%d) Se crea el proceso - Estado: NEW", proceso->pid);
	pthread_mutex_unlock(&proceso->mutex);
	pthread_mutex_unlock(&m_transicionar);
}

static void agregar_a_ready_CMN(t_pcb* proceso){
	if(proceso == NULL){
		log_error(logger, "proceso NULL en agregar_a_ready_CMN");
		return;
	}
	t_sublista_ready* cola_prioridad = sublista_ready_de_prioridad(proceso->prioridad_actual);
	agregar_proceso_a_lista(cola_prioridad->sublista, proceso, LISTO, &cola_prioridad->mutex);
}

static void agregar_a_ready(t_pcb* proceso){
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

static bool eliminar_de_ready_CMN(t_pcb* proceso){
	t_sublista_ready* cola = sublista_ready_de_prioridad(proceso->prioridad_actual);
	return eliminar_proceso_de_lista(cola->sublista, proceso, "cola de prioridad de ready", &cola->mutex);
}

static bool eliminar_de_ready(t_pcb* proceso){
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

static bool eliminar_proceso_de_lista(t_list* lista, t_pcb* proceso, char* nombre_lista, pthread_mutex_t* mutex){
	if(proceso == NULL){
		log_error(logger, "proceso NULL en eliminar_proceso_de_lista");
		return false;
	}
	pthread_mutex_lock(mutex);
	bool removido = list_remove_element(lista, proceso);
	pthread_mutex_unlock(mutex);
	if(!removido){
		log_error(logger, "El proceso PID %d no estaba en %s", proceso->pid, nombre_lista);
	}
	return removido;
}

static void agregar_proceso_a_lista(t_list* lista, t_pcb* proceso, t_tipo_estado nuevo_estado, pthread_mutex_t* mutex){
	if(proceso == NULL){
		log_error(logger, "proceso NULL en agregar_proceso_a_lista");
		return;
	}
	pthread_mutex_lock(mutex);
	proceso->estado = nuevo_estado;
	list_add(lista, proceso);
	pthread_mutex_unlock(mutex);
}

static t_sublista_ready* sublista_ready_de_prioridad(int prioridad){
	pthread_mutex_lock(&estado_ready->mutex);
	t_sublista_ready* cola_prioridad = list_get(estado_ready->sublista, prioridad);
	pthread_mutex_unlock(&estado_ready->mutex);
	if(cola_prioridad == NULL){
		log_error(logger, "error al obtener la cola de prioridad %d", prioridad);
		exit(EXIT_FAILURE);
	}
	return cola_prioridad;
}

void registrar_proceso_exec(t_pcb* proceso){
	agregar_proceso_a_lista(estado_exec->sublista, proceso, EJECUTANDO, &estado_exec->mutex);
	log_obligatorio_cambio_de_estado(proceso->pid, "READY", "EXEC");
}

void agregar_a_ready_al_frente(t_pcb* proceso){
	if(proceso == NULL){
		return;
	}
	switch(algoritmo){
		case FIFO:
		case RR:
			pthread_mutex_lock(&estado_ready->mutex);
			list_add_in_index(estado_ready->sublista, 0, proceso);
			pthread_mutex_unlock(&estado_ready->mutex);
			break;
		case CMN: {
			t_sublista_ready* sublista = sublista_ready_de_prioridad(proceso->prioridad_actual);
			pthread_mutex_lock(&estado_ready->mutex);
			pthread_mutex_lock(&sublista->mutex);
			list_add_in_index(sublista->sublista, 0, proceso);
			pthread_mutex_unlock(&sublista->mutex);
			pthread_mutex_unlock(&estado_ready->mutex);
			break;
		}
		default:
			log_error(logger, "Algoritmo desconocido");
			exit(EXIT_FAILURE);
	}
	pthread_mutex_lock(&m_procesos_en_ready);
	procesos_en_ready++;
	pthread_mutex_unlock(&m_procesos_en_ready);
}

void desbloquear_proceso(t_pcb* proceso){
	log_debug(logger, "desbloquear_proceso: ejecutando");
	pthread_mutex_lock(&proceso->mutex);
	t_tipo_estado estado = proceso->estado;
	pthread_mutex_unlock(&proceso->mutex);
	if(estado == SUSP_BLOQUEADO){
		susp_blocked_a_susp_ready(proceso);
	}else if(estado == BLOQUEADO){
		blocked_a_ready(proceso);
	}
}

static t_lista_estado* lista_de_estado(t_tipo_estado estado){
	switch(estado){
		case NUEVO: return estado_new;
		case LISTO: return estado_ready;
		case BLOQUEADO: return estado_blocked;
		case SUSP_BLOQUEADO: return estado_susp_blocked;
		case SUSP_LISTO: return estado_susp_ready;
		case FINALIZADO: return estado_exit;
		case EJECUTANDO: return estado_exec;
		default: return NULL;
	}
}

t_pcb* proceso_de_estado(int pid, t_tipo_estado estado){
	t_lista_estado* lista_estado = lista_de_estado(estado);
	if(lista_estado == NULL){
		return NULL;
	}
	pthread_mutex_lock(&lista_estado->mutex);
	for(int i = 0; i < list_size(lista_estado->sublista); i++){
		t_pcb* proceso = list_get(lista_estado->sublista, i);
		if(proceso->pid == pid){
			pthread_mutex_unlock(&lista_estado->mutex);
			return proceso;
		}
	}
	pthread_mutex_unlock(&lista_estado->mutex);
	return NULL;
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

static bool mayor_prioridad(void* a, void* b){
	t_pcb* proceso_a = a;
	t_pcb* proceso_b = b;
	return get_prioridad_actual_thread_safe(proceso_a) < get_prioridad_actual_thread_safe(proceso_b);
}

void intentar_desuspender(void){
	pthread_mutex_lock(&m_transicionar);
	pthread_mutex_lock(&estado_susp_ready->mutex);
	log_debug(logger, "intentando desuspender procesos");
	t_list* procesos_ordenados = list_duplicate(estado_susp_ready->sublista);
	list_sort(procesos_ordenados, mayor_prioridad);
	for(int i = 0; i < list_size(procesos_ordenados); i++){
		int pid = get_pid_thread_safe(list_get(procesos_ordenados, i));
		t_paquete* data = crear_paquete(SCH_KM__INTENTAR_DESUSPENDER);
		agregar_a_paquete(data, &pid, sizeof(pid));
		enviar_paquete_y_liberarlo(data, conexion_kernel_memory);
	}
	pthread_mutex_unlock(&estado_susp_ready->mutex);
	pthread_mutex_unlock(&m_transicionar);
	list_destroy(procesos_ordenados);
}

void loguear_tamanio_listas_de_estado(void){
	imprimir_estado_procesos();
}

void imprimir_estado_procesos(void){
	if(log_level != LOG_LEVEL_DEBUG){
		return;
	}
	pthread_mutex_lock(&m_transicionar);
	char* mensaje = string_new();
	string_append(&mensaje, "NEW=[");
	for(int i = 0; i < list_size(estado_new->sublista); i++){
		if(i) string_append(&mensaje, ",");
		t_pcb* proceso = list_get(estado_new->sublista, i);
		string_append_with_format(&mensaje, "%d", proceso->pid);
	}
	string_append(&mensaje, "] READY=[");
	if(algoritmo == CMN){
		bool primero = true;
		for(int i = 0; i < list_size(estado_ready->sublista); i++){
			t_sublista_ready* sublista = list_get(estado_ready->sublista, i);
			for(int j = 0; j < list_size(sublista->sublista); j++){
				if(!primero) string_append(&mensaje, ",");
				t_pcb* proceso = list_get(sublista->sublista, j);
				string_append_with_format(&mensaje, "%d", proceso->pid);
				primero = false;
			}
		}
	}else{
		for(int i = 0; i < list_size(estado_ready->sublista); i++){
			if(i) string_append(&mensaje, ",");
			t_pcb* proceso = list_get(estado_ready->sublista, i);
			string_append_with_format(&mensaje, "%d", proceso->pid);
		}
	}
	string_append(&mensaje, "] ");
	t_lista_estado* estados[] = {estado_exec, estado_blocked, estado_susp_blocked, estado_susp_ready, estado_exit};
	const char* nombres[] = {"EXEC", "BLOCKED", "SUSP_BLOCKED", "SUSP_READY", "EXIT"};
	for(int e = 0; e < 5; e++){
		string_append_with_format(&mensaje, "%s=[", nombres[e]);
		for(int i = 0; i < list_size(estados[e]->sublista); i++){
			if(i) string_append(&mensaje, ",");
			t_pcb* proceso = list_get(estados[e]->sublista, i);
			string_append_with_format(&mensaje, "%d", proceso->pid);
		}
		string_append(&mensaje, "] ");
	}
	log_info(logger, "%s", mensaje);
	free(mensaje);
	pthread_mutex_unlock(&m_transicionar);
}

void cambiar_prioridad(t_pcb* proceso, int prioridad){
	if(algoritmo != CMN){
		log_debug(logger, "Algoritmo no es CMN, no aplica el cambio de prioridad");
		return;
	}
	if(list_size(queues_algorithms) > prioridad){
		log_info(logger, "## %d Cambio de prioridad: %d - %d", proceso->pid, proceso->prioridad_actual, prioridad);
		pthread_mutex_lock(&m_transicionar);
		pthread_mutex_lock(&proceso->mutex);
		proceso->prioridad_actual = prioridad;
		if(proceso->estado == LISTO){
			bool eliminado = eliminar_de_ready(proceso);
			if(eliminado){
				agregar_a_ready(proceso);
			}
		}
		pthread_mutex_unlock(&proceso->mutex);
		pthread_mutex_unlock(&m_transicionar);
	}else{
		log_error(logger, "Error: Intento de pasar al proceso <%d> a una prioridad invalida (%d)", proceso->pid, prioridad);
		exit(EXIT_FAILURE);
	}
}
