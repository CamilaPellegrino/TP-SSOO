/*
 * Tests de modulo kernel_scheduler
 * Compilacion y ejecucion: make -C kernel_scheduler test
 * Archivo compilado en kernel_scheduler/bin
 */

#include "../src/main.h"
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static int cantidad_fallos = 0;

static void verificar(bool condicion, const char* descripcion){
	if(condicion){
		fprintf(stderr, "[OK]: %s\n", descripcion);
	}else{
		fprintf(stderr, "[FALLO]: %s\n", descripcion);
		cantidad_fallos++;
	}
}

static void destruir_pcb_sync(t_pcb* proceso){
	pthread_mutex_destroy(&proceso->mutex);
	pthread_mutex_destroy(&proceso->data_cond.mutex_cond);
	pthread_cond_destroy(&proceso->data_cond.cond);
}

static t_pcb* crear_pcb(int pid, int prioridad){
	return iniciar_pcb(pid, 0, prioridad, NUEVO);
}

static const char* nombre_estado_test(t_tipo_estado estado){
	switch(estado){
		case NUEVO: return "NEW";
		case LISTO: return "READY";
		case EJECUTANDO: return "EXEC";
		case BLOQUEADO: return "BLOCKED";
		case SUSP_BLOQUEADO: return "SUSP_BLOCKED";
		case SUSP_LISTO: return "SUSP_READY";
		case FINALIZADO: return "EXIT";
		default: return "DESCONOCIDO";
	}
}

void imprimir_estado_procesos_test(){
	fprintf(stderr, "%s\n", estado_procesos());
}

static void* esperar_evento_finalizado(void* arg){
	t_evt* evento = arg;
	pthread_mutex_lock(&evento->mutex);
	while(!evento->syscall_finalizada){
		pthread_cond_wait(&evento->cond, &evento->mutex);
	}
	pthread_mutex_unlock(&evento->mutex);
	return NULL;
}

static void agregar_a_exec(t_pcb* proceso){
	proceso_a_new(proceso);
	new_a_ready(proceso);
	t_pcb* elegido = desencolar_ready_cmn();
	verificar(elegido == proceso, "READY despacha el proceso esperado");
	if(elegido == proceso){
		registrar_proceso_exec(proceso);
	}
}

// Verifica el parseo de configuracion y algoritmos.
static void test_configuracion_y_algoritmos(void){
	verificar(obtener_algoritmo_planificacion("FIFO") == FIFO, "reconoce FIFO");
	verificar(obtener_algoritmo_planificacion("RR") == RR, "reconoce RR");
	verificar(obtener_algoritmo_planificacion("CMN") == CMN, "reconoce CMN");
	verificar(queue_preemption_from_string("TRUE"), "activa queue preemption con TRUE");
	verificar(!queue_preemption_from_string("FALSE"), "desactiva queue preemption con FALSE");
	t_pcb proceso = {.prioridad_actual = 1};
	verificar(algoritmo_de_proceso(&proceso) == RR, "selecciona el algoritmo configurado para la prioridad");

	char* nombres[] = {"FIFO", "RR", "CMN", NULL};
	t_list* algoritmos = queues_algorithms_a_t_list(nombres);
	verificar(algoritmos != NULL && list_size(algoritmos) == 3, "convierte la lista de algoritmos de colas");
	if(algoritmos != NULL){
		verificar(*(t_planificacion*)list_get(algoritmos, 0) == FIFO &&
		          *(t_planificacion*)list_get(algoritmos, 1) == RR &&
		          *(t_planificacion*)list_get(algoritmos, 2) == CMN,
		          "conserva el orden de algoritmos configurados");
		list_destroy_and_destroy_elements(algoritmos, free);
	}
}

// Verifica la inicializacion del PCB y sus accesores.
static void test_iniciar_pcb_y_accesores(void){
	t_pcb* proceso = crear_pcb(7, 3);
	verificar(proceso != NULL, "crea un PCB");
	if(proceso == NULL){
		return;
	}
	verificar(get_pid_thread_safe(proceso) == 7, "obtiene PID con sincronizacion");
	verificar(proceso->ppid == 0 && proceso->prioridad_base == 3 && proceso->prioridad_actual == 3,
	          "inicializa parentesco y prioridades del PCB");
	verificar(get_estado(proceso) == NUEVO && get_status(proceso) == OK, "inicializa estado y status del PCB");
	verificar(get_prioridad_actual_thread_safe(proceso) == 3, "obtiene prioridad actual con sincronizacion");
	set_status(proceso, SEG_FAULT);
	verificar(get_status(proceso) == SEG_FAULT, "actualiza el status del proceso");
	set_status(proceso, OK);
	verificar(get_status(proceso) == SEG_FAULT, "conserva el status de error al recibir OK");
	cambiar_de_evt(proceso, NULL);
	verificar(get_evt_de_proceso(proceso) == NULL, "lee el evento actual del proceso");
	destruir_pcb_sync(proceso);
	destroy_pcb(proceso);
}

// Verifica la creacion y asociacion de eventos.
static void test_creacion_de_eventos(void){
	t_pcb* proceso = crear_pcb(8, 1);
	t_dir_fisica direccion = {.id_segmento = 2, .offset = 12};
	t_evt* sleep_evt = iniciar_evt_sleep(25, proceso);
	verificar(((t_evt_sleep*)sleep_evt->data_evt)->tiempo_sleep == 25 && sleep_evt->proceso == proceso,
	          "crea un evento sleep");
	liberar_evt(sleep_evt, free);

	t_evt* stdin_evt = iniciar_evt_std_in(4, direccion, proceso);
	t_evt_std_in* stdin_data = stdin_evt->data_evt;
	verificar(stdin_data->tamanio == 4 && stdin_data->dir_fisica.id_segmento == 2 &&
	          stdin_data->dir_fisica.offset == 12, "crea un evento stdin con direccion y tamanio");
	liberar_evt(stdin_evt, free);

	t_evt* stdout_evt = iniciar_evt_std_out(strdup("dato"), proceso);
	cambiar_de_evt(proceso, stdout_evt);
	t_evt_std_out* stdout_data = stdout_evt->data_evt;
	verificar(strcmp(stdout_data->datos_leidos, "dato") == 0 && get_evt_de_proceso(proceso) == stdout_evt,
	          "crea y asocia un evento stdout");
	cambiar_de_evt(proceso, NULL);
	liberar_evt(stdout_evt, liberar_evt_stdout);

	t_evt* mutex_evt = iniciar_evt_mutex_lock(proceso);
	verificar(mutex_evt->proceso == proceso && mutex_evt->data_evt == NULL,
	          "crea un evento de espera de mutex");
	liberar_evt(mutex_evt, free);
	destruir_pcb_sync(proceso);
	destroy_pcb(proceso);
}

// Verifica que finalizar_evento despierte y una el hilo.
static void test_finalizar_evento(void){
	t_pcb* proceso = crear_pcb(15, 1);
	t_evt* evento = iniciar_evt_mutex_lock(proceso);
	int resultado = pthread_create(&evento->hilo_timeout, NULL, esperar_evento_finalizado, evento);
	verificar(resultado == 0, "inicia el hilo de espera del evento");
	if(resultado == 0){
		finalizar_evento(evento);
		verificar(evento->syscall_finalizada, "finaliza el evento y despierta su hilo");
	}
	liberar_evt(evento, free);
	destruir_pcb_sync(proceso);
	destroy_pcb(proceso);
}

// Verifica CPU e IO sin conexiones externas.
static void test_cpu_y_io_locales(void){
	t_pcb* proceso = crear_pcb(9, 1);
	t_cpu* cpu = iniciar_cpu(4, 23);
	verificar(cpu != NULL && cpu->id == 4 && cpu->fd == 23 && cpu->proceso == NULL && !cpu->desalojando,
	          "inicializa una CPU libre");
	if(cpu != NULL){
		set_proceso_thread_safe(cpu, proceso);
		verificar(get_proceso_de_cpu_thread_safe(cpu) == proceso &&
		          get_pid_de_proceso_de_cpu_thread_safe(cpu) == 9, "asocia un PCB a una CPU");
		list_add(lista_cpus, cpu);
		verificar(cpu_de_pid(9) == cpu, "busca la CPU que ejecuta un PID");
		verificar(hay_cpus_ejecutando(), "detecta una CPU con un proceso asignado");
		liberar_cpu(cpu);
		verificar(get_proceso_de_cpu_thread_safe(cpu) == NULL, "libera la CPU");
		list_remove_element(lista_cpus, cpu);
		pthread_mutex_destroy(&cpu->mutex);
		free(cpu);
	}
	t_io* io = iniciar_io(STDIN, 31);
	verificar(io != NULL && io->tipo == STDIN && io->fd == 31, "inicializa un dispositivo IO");
	free(io);
	destruir_pcb_sync(proceso);
	destroy_pcb(proceso);
}

// Verifica mensajes enviados a CPUs mediante un socket local.
static void test_mensajes_a_cpus_locales(void){
	int sockets[2];
	int resultado_socketpair = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
	verificar(resultado_socketpair == 0, "crea un canal local para simular una CPU");
	if(resultado_socketpair != 0){
		return;
	}
	t_cpu* cpu = iniciar_cpu(5, sockets[0]);
	list_add(lista_cpus, cpu);
	t_paquete* paquete = crear_paquete(SCH_CPU__PID);
	int pid = 42;
	agregar_a_paquete(paquete, &pid, sizeof(pid));
	enviar_paquete_a_todas_las_cpus(paquete);
	op_code operacion = recibir_operacion(sockets[1]);
	t_list* datos = recibir_paquete(sockets[1]);
	verificar(operacion == SCH_CPU__PID && datos != NULL && list_size(datos) == 1 &&
	          *(int*)list_get(datos, 0) == 42, "difunde un paquete a todas las CPUs");
	if(datos != NULL){
		list_destroy_and_destroy_elements(datos, free);
	}
	pedido_desalojo_a_todas_las_cpus(SCH_CPU__PEDIDO_DESALOJO);
	verificar(recibir_operacion(sockets[1]) == SCH_CPU__PEDIDO_DESALOJO && cpu->desalojando,
	          "solicita desalojo y marca la CPU");
	list_remove_element(lista_cpus, cpu);
	pthread_mutex_destroy(&cpu->mutex);
	free(cpu);
	close(sockets[0]);
	close(sockets[1]);
}

// Verifica señales segun el estado global.
static void test_senial_de_planificacion(void){
	while(sem_trywait(&s_intentar_planificar) == 0){}
	verificar(errno == EAGAIN, "vacía las señales previas del planificador");
	cambiar_estado_global(PLANIF_ACTIVA);
	intentar_planificar();
	int valor = 0;
	sem_getvalue(&s_intentar_planificar, &valor);
	verificar(valor == 1, "señaliza al planificador cuando está activo");
	sem_trywait(&s_intentar_planificar);
	cambiar_estado_global(COMPACTANDO);
	intentar_planificar();
	sem_getvalue(&s_intentar_planificar, &valor);
	verificar(valor == 0, "no señaliza al planificador durante compactacion");
	cambiar_estado_global(PLANIF_ACTIVA);
}

// Verifica el ciclo NEW, READY, EXEC y EXIT.
static void test_transiciones_de_proceso(void){
	t_pcb* proceso = crear_pcb(10, 1);
	proceso_a_new(proceso);
	verificar(proceso_new_siguiente() == proceso && proceso_de_estado(10, NUEVO) == proceso &&
	          get_proceso_de_pid_thread_safe(10) == proceso,
	          "NEW conserva y permite consultar el proceso creado");
	new_a_ready(proceso);
	verificar(get_estado(proceso) == LISTO && cantidad_procesos_ready() == 1,
	          "transiciona NEW a READY y actualiza el contador");
	t_pcb* elegido = proximo_proceso();
	verificar(elegido == proceso && cantidad_procesos_ready() == 0,
	          "extrae READY y actualiza el contador");
	registrar_proceso_exec(proceso);
	verificar(get_estado(proceso) == EJECUTANDO && proceso_de_estado(10, EJECUTANDO) == proceso,
	          "registra el proceso en EXEC");
	exec_a_blocked_cond_signal(proceso);
	verificar(get_estado(proceso) == BLOQUEADO && proceso->data_cond.cond_val,
	          "transiciona EXEC a BLOCKED y señaliza la condicion");
	blocked_a_ready(proceso);
	verificar(get_estado(proceso) == LISTO && cantidad_procesos_ready() == 1,
	          "transiciona BLOCKED a READY");
	elegido = proximo_proceso();
	registrar_proceso_exec(elegido);
	exec_a_ready_cond_signal(proceso);
	verificar(get_estado(proceso) == LISTO && proceso->data_cond.cond_val && cantidad_procesos_ready() == 1,
	          "transiciona EXEC a READY y señaliza la condicion");
	elegido = proximo_proceso();
	registrar_proceso_exec(elegido);
	exec_a_exit(proceso);
	verificar(get_estado(proceso) == FINALIZADO && proceso_de_estado(10, FINALIZADO) == proceso,
	          "transiciona EXEC a EXIT");
	destruir_pcb_sync(proceso);
	liberar_pcb_de_exit(10);
	verificar(proceso_de_estado(10, FINALIZADO) == NULL, "libera el PCB de EXIT");
}

// Verifica creacion y suspension con un mock local de KM.
static void test_creacion_y_suspension_con_socket_local(void){
	int sockets[2];
	int resultado_socketpair = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
	verificar(resultado_socketpair == 0, "crea un canal local para simular Kernel Memory");
	if(resultado_socketpair != 0){
		return;
	}
	conexion_kernel_memory = sockets[0];
	proximo_pid = 50;
	t_pcb* proceso = nuevo_proc(1, 7, "programa.prc");
	verificar(proceso != NULL && proceso->pid == 50 && proceso->ppid == 7,
	          "crea un PCB y asigna el siguiente PID");
	op_code operacion = recibir_operacion(sockets[1]);
	t_list* paquete = recibir_paquete(sockets[1]);
	verificar(operacion == SCH_KM__INIT_PROC && paquete != NULL && list_size(paquete) == 3,
	          "envia INIT_PROC con los datos del proceso");
	if(paquete != NULL && list_size(paquete) == 3){
		verificar(*(int*)list_get(paquete, 0) == 50 && *(int*)list_get(paquete, 1) == 7 &&
		          strcmp(list_get(paquete, 2), "programa.prc") == 0,
		          "serializa PID, PPID y ruta de instrucciones");
		list_destroy_and_destroy_elements(paquete, free);
	}
	suspender_proceso(proceso);
	operacion = recibir_operacion(sockets[1]);
	paquete = recibir_paquete(sockets[1]);
	verificar(operacion == SCH_KM__SUSPENDER && paquete != NULL &&
	          *(int*)list_get(paquete, 0) == 50, "envia la solicitud de suspension");
	if(paquete != NULL){
		list_destroy_and_destroy_elements(paquete, free);
	}

	new_a_ready(proceso);
	desencolar_ready_cmn();
	registrar_proceso_exec(proceso);
	exec_a_blocked_cond_signal(proceso);
	blocked_a_susp_blocked(proceso);
	desbloquear_proceso(proceso);
	operacion = recibir_operacion(sockets[1]);
	paquete = recibir_paquete(sockets[1]);
	verificar(operacion == SCH_KM__INTENTAR_DESUSPENDER && paquete != NULL &&
	          *(int*)list_get(paquete, 0) == 50, "solicita desuspender procesos en SUSP_READY");
	if(paquete != NULL){
		list_destroy_and_destroy_elements(paquete, free);
	}
	susp_ready_a_ready(proceso);
	t_pcb* elegido = desencolar_ready_cmn();
	if(elegido == proceso){
		registrar_proceso_exec(proceso);
		exec_a_exit(proceso);
	}
	verificar(elegido == proceso && get_estado(proceso) == FINALIZADO,
	          "reanuda el proceso suspendido y lo lleva a EXIT");
	destruir_pcb_sync(proceso);
	liberar_pcb_de_exit(50);
	close(sockets[0]);
	close(sockets[1]);
	conexion_kernel_memory = -1;
}

// Verifica orden CMN y cambio de prioridad.
static void test_colas_cmn_y_prioridad(void){
	t_pcb* prioridad_baja = crear_pcb(11, 2);
	t_pcb* prioridad_media = crear_pcb(12, 1);
	proceso_a_new(prioridad_baja);
	proceso_a_new(prioridad_media);
	t_pcb* procesos[] = {prioridad_baja, prioridad_media};
	
	new_a_ready(prioridad_baja);
	new_a_ready(prioridad_media);

	cambiar_prioridad(prioridad_baja, 0);

	verificar(get_prioridad_actual_thread_safe(prioridad_baja) == 0,
	          "actualiza la prioridad actual bajo CMN");

	t_pcb* primero = desencolar_ready_cmn();

	verificar(primero == prioridad_baja, "READY prioriza la cola de mayor prioridad");
	if(primero != NULL){
		registrar_proceso_exec(primero);
		exec_a_exit(primero);
	}
	t_pcb* segundo = desencolar_ready_cmn();
	verificar(segundo == prioridad_media, "READY mantiene el orden de las colas restantes");
	if(segundo != NULL){
		registrar_proceso_exec(segundo);
		exec_a_exit(segundo);
	}
	if(primero != NULL){
		destruir_pcb_sync(primero);
		liberar_pcb_de_exit(primero->pid);
	}
	if(segundo != NULL){
		destruir_pcb_sync(segundo);
		liberar_pcb_de_exit(segundo->pid);
	}
}

// Verifica adquisicion, espera y traspaso de mutex.
static void test_mutexes_locales(void){
	t_pcb* duenio = crear_pcb(13, 2);
	t_pcb* esperando = crear_pcb(14, 0);
	agregar_a_exec(duenio);
	agregar_a_exec(esperando);
	t_mutex* mutex = m_create("recurso");
	verificar(mutex != NULL && get_mutex("recurso") == mutex, "crea y busca un mutex");
	verificar(m_create("recurso") == NULL, "rechaza un nombre de mutex duplicado");
	verificar(m_wait(mutex, duenio), "adquiere un mutex libre");
	verificar(!m_wait(mutex, esperando) && list_size(mutex->procesos_en_espera) == 1,
	          "encola al proceso que encuentra el mutex ocupado");
	verificar(duenio->prioridad_actual == 0, "hereda la prioridad del proceso en espera");
	exec_a_blocked_cond_signal(esperando);
	m_signal(mutex, esperando);
	verificar(mutex->duenio == duenio && list_size(mutex->procesos_en_espera) == 1,
	          "ignora la liberacion solicitada por un proceso sin propiedad");
	m_signal(mutex, duenio);
	verificar(mutex->duenio == esperando && esperando->mutex_esperado == NULL,
	          "transfiere el mutex al proceso en espera");
	verificar(get_estado(esperando) == LISTO && cantidad_procesos_ready() == 1,
	          "desbloquea al nuevo dueño del mutex");
	verificar(duenio->prioridad_actual == duenio->prioridad_base,
	          "recalcula la prioridad del dueño anterior");
	exec_a_exit(duenio);
	t_pcb* elegido = desencolar_ready_cmn();
	if(elegido != NULL){
		registrar_proceso_exec(elegido);
		m_signal(mutex, elegido);
		exec_a_exit(elegido);
	}
	verificar(elegido == esperando && mutex->duenio == NULL, "libera el mutex sin procesos en espera");
	destruir_pcb_sync(duenio);
	liberar_pcb_de_exit(duenio->pid);
	destruir_pcb_sync(esperando);
	liberar_pcb_de_exit(esperando->pid);

	list_remove_element(lista_mutex, mutex);
	pthread_mutex_destroy(&mutex->lock);
	list_destroy(mutex->procesos_en_espera);
	free(mutex->nombre);
	free(mutex);
}

// Verifica el acarreo al sumar milisegundos.
static void test_aritmetica_de_timeout(void){
	struct timespec tiempo = {.tv_sec = 10, .tv_nsec = 900000000};
	sumar_milisegundos(&tiempo, 1500);
	verificar(tiempo.tv_sec == 12 && tiempo.tv_nsec == 400000000,
	          "normaliza el timeout al sumar milisegundos");
}

// Verifica el orden FIFO y RR en procesos aislados.
static void test_colas_fifo_y_rr_en_subprocesos(void){
	const t_planificacion algoritmos[] = {FIFO, RR};
	for(size_t i = 0; i < sizeof(algoritmos) / sizeof(algoritmos[0]); i++){
		pid_t hijo = fork();
		verificar(hijo >= 0, "crea un proceso aislado para probar una cola");
		if(hijo < 0){
			continue;
		}
		if(hijo == 0){
			cantidad_fallos = 0;
			algoritmo = algoritmos[i];
			inicializar_maquina_estados();
			t_pcb* primero = crear_pcb(30, 1);
			t_pcb* segundo = crear_pcb(31, 1);
			proceso_a_new(primero);
			proceso_a_new(segundo);
			new_a_ready(primero);
			new_a_ready(segundo);
			verificar(proximo_proceso() == primero, "el planificador selecciona primero el proceso esperado");
			verificar(desencolar_ready_fifo_rr() == segundo, "la cola conserva el siguiente proceso");
			_exit(cantidad_fallos == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
		}
		int status = 0;
		verificar(waitpid(hijo, &status, 0) == hijo && WIFEXITED(status) &&
		          WEXITSTATUS(status) == EXIT_SUCCESS,
		          algoritmos[i] == FIFO ? "FIFO despacha en orden" : "RR despacha en orden");
	}
}

// Verifica que READY permita reinsertar al frente.
static void test_reinsercion_al_frente(void){
	t_pcb* primero = crear_pcb(20, 1);
	t_pcb* segundo = crear_pcb(21, 1);
	proceso_a_new(primero);
	proceso_a_new(segundo);
	new_a_ready(primero);
	new_a_ready(segundo);
	verificar(desencolar_ready_cmn() == primero, "extrae el primer proceso antes de reinsertarlo");
	agregar_a_ready_al_frente(primero);
	t_pcb* elegido_primero = desencolar_ready_cmn();
	t_pcb* elegido_segundo = desencolar_ready_cmn();
	verificar(elegido_primero == primero && elegido_segundo == segundo,
	          "despacha primero el proceso reinsertado al frente");
	if(elegido_primero != NULL){
		registrar_proceso_exec(elegido_primero);
		exec_a_exit(elegido_primero);
	}
	if(elegido_segundo != NULL){
		registrar_proceso_exec(elegido_segundo);
		exec_a_exit(elegido_segundo);
	}
	destruir_pcb_sync(primero);
	liberar_pcb_de_exit(primero->pid);
	destruir_pcb_sync(segundo);
	liberar_pcb_de_exit(segundo->pid);
}

int main(void){
	log_level = LOG_LEVEL_ERROR; // LOG_LEVEL_TRACE para ver todo los logs, LOG_LEVEL_ERROR para ver solo errores.
	logger = log_create("/tmp/kernel_scheduler_tests.log", "kernel_scheduler_tests", true, log_level);
	printf("Running kernel_scheduler tests...\n");
	if(logger == NULL){
		return EXIT_FAILURE;
	}
	lista_cpus = list_create();
	lista_mutex = list_create();
	queues_algorithms = list_create();
	pthread_mutex_init(&m_lista_cpus, NULL);
	pthread_mutex_init(&m_lista_mutex, NULL);
	pthread_mutex_init(&m_proximo_pid, NULL);
	pthread_mutex_init(&m_estado_global, NULL);
	sem_init(&s_intentar_planificar, 0, 0);
	algoritmo = CMN;
	for(int i = 0; i < 3; i++){
		t_planificacion* algoritmo_cola = malloc(sizeof(*algoritmo_cola));
		*algoritmo_cola = RR;
		list_add(queues_algorithms, algoritmo_cola);
	}
	cambiar_estado_global(PLANIF_ACTIVA);
	inicializar_maquina_estados();

	test_configuracion_y_algoritmos();
	test_iniciar_pcb_y_accesores();
	test_creacion_de_eventos();
	test_finalizar_evento();
	test_cpu_y_io_locales();
	test_mensajes_a_cpus_locales();
	test_senial_de_planificacion();
	test_colas_fifo_y_rr_en_subprocesos();
	inicializar_maquina_estados();
	test_transiciones_de_proceso();
	test_creacion_y_suspension_con_socket_local();
	test_colas_cmn_y_prioridad();
	test_reinsercion_al_frente();
	test_mutexes_locales();
	test_aritmetica_de_timeout();
	
	sem_destroy(&s_intentar_planificar);
	pthread_mutex_destroy(&m_lista_cpus);
	pthread_mutex_destroy(&m_lista_mutex);
	pthread_mutex_destroy(&m_proximo_pid);
	pthread_mutex_destroy(&m_estado_global);
	list_destroy(lista_cpus);
	list_destroy(lista_mutex);
	list_destroy_and_destroy_elements(queues_algorithms, free);
	log_destroy(logger);
	if(cantidad_fallos > 0){
		fprintf(stderr, "%d pruebas fallaron\n", cantidad_fallos);
		return EXIT_FAILURE;
	}else{
		fprintf(stderr, "Todas las pruebas pasaron\n");
	}
	return EXIT_SUCCESS;
}