#ifndef BASE_SCH_H_
#define BASE_SCH_H_

#include <utils/utils.h>
#include <semaphore.h>
#include <pthread.h>

typedef struct t_evt t_evt;
typedef struct t_pcb t_pcb;
typedef struct t_mutex t_mutex;

typedef enum{
	PLANIF_ACTIVA,
	COMPACTANDO
}t_estado_sch;

typedef struct
{
	pthread_cond_t cond;
	pthread_mutex_t mutex_cond;
	bool cond_val;
} t_data_cond;

typedef struct
{
	t_list* sublista;
	pthread_mutex_t mutex;
} t_sublista_ready;

typedef struct
{
	t_list* sublista;
	pthread_mutex_t mutex;
	char* nombre;
	t_tipo_estado tipo;
} t_lista_estado;


struct t_pcb{
    int pid;
    int ppid;
    int prioridad_base;
	int prioridad_actual;
	t_tipo_estado estado;
	t_data_cond data_cond;
	pthread_mutex_t mutex;
	t_mutex* mutex_esperado;
	t_evt* evt_actual; 
	t_status_op status;
};

typedef struct
{ 
	int fd;
	int id;
	t_pcb* proceso; // proceso que esta ejecutando (NULL si no esta ejecutando nada)
	bool desalojando;
	pthread_mutex_t mutex;
} t_cpu;


// cosas de IO
typedef struct{
	int fd;
	t_tipo_io tipo;
} t_io;

typedef struct{
	int tiempo_sleep;
} t_evt_sleep;

typedef struct{
	int tamanio;
	t_dir_fisica dir_fisica;
}t_evt_std_in;

struct t_evt{
	void* data_evt; 
	t_pcb* proceso;
	bool syscall_finalizada;
	pthread_mutex_t mutex;
    pthread_cond_t cond;
	pthread_t hilo_timeout;
};

typedef struct{
	char* datos_leidos;
}t_evt_std_out;

typedef enum{
    CMN,
    FIFO,
    RR
} t_planificacion;

// para t_mutex
 struct t_mutex{
    char* nombre;
    int mutex_id;
    t_pcb* duenio;
    t_list* procesos_en_espera; // procesos que esperan por el mutex, en orden
    pthread_mutex_t lock;
};

// variables globales 
extern t_log* logger;
extern t_planificacion algoritmo;  // CMN, FIFO o RR
extern int proximo_pid;
extern int suspension_timeout;
extern int quantum;
extern int procesos_en_ready;
extern t_estado_sch estado_global;

extern t_lista_estado* estado_new;
extern t_lista_estado* estado_ready;
extern t_lista_estado* estado_blocked;
extern t_lista_estado* estado_exec;
extern t_lista_estado* estado_susp_blocked;
extern t_lista_estado* estado_susp_ready;
extern t_lista_estado* estado_exit;

extern t_list* lista_cpus;

extern t_list* queues_algorithms;  // algoritmo que usa cada cola cuando es CMN

// listas de eventos
extern t_list* lista_evt_sleep;  
extern t_list* lista_evt_stdin;  
extern t_list* lista_evt_stdout;  

// lista mutex (implementado)
extern t_list* lista_mutex;        // mutexs creados por los procesos (tiene cosas de tipo t_mutex adentro)

// semaforos

extern sem_t s_intentar_planificar;
extern sem_t s_nuevo_proceso_new;
extern sem_t s_evt_sleep;       // cuando hay una nueva solic de sleep
extern sem_t s_evt_stdin;
extern sem_t s_evt_stdout;

// mutexs
extern pthread_mutex_t m_transicionar;
extern pthread_mutex_t m_lista_evt_sleep;
extern pthread_mutex_t m_lista_evt_stdin;
extern pthread_mutex_t m_lista_evt_stdout;
extern pthread_mutex_t m_lista_cpus; 
extern pthread_mutex_t m_lista_mutex;
extern pthread_mutex_t m_procesos_en_ready;
extern pthread_mutex_t m_proximo_pid;
extern pthread_mutex_t m_estado_global;
// sockets
extern int conexion_kernel_memory;


// funciones para inicializar cosas

void inicializar_variables_globales(t_config* config);
void inicializar_parametros_de_config(t_config* config); 
t_list* queues_algorithms_a_t_list(char** queues_algorithms_str);
t_lista_estado* inicializar_lista_estado(char* nombre, t_list* lista, bool init_m, t_tipo_estado tipo);
t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);
t_pcb* iniciar_pcb(int pid, int ppid, int prioridad, t_tipo_estado estado);
t_evt* iniciar_evt_sleep(int tiempo_sleep, t_pcb* proceso);
t_evt* iniciar_evt_std_in(int tamanio, t_dir_fisica, t_pcb* proceso);
t_evt* iniciar_evt_std_out(char* datos_leidos, t_pcb* proceso);
t_evt* iniciar_evt_mutex_lock(t_pcb* proceso);

// funciones para destroy

void destroy_pcb(t_pcb* pcb);

// mover entre listas de procesos

void blocked_a_susp_blocked(t_pcb* pid);
void susp_blocked_a_susp_ready(t_pcb* pid);
void susp_ready_a_ready(t_pcb* pid);
void ready_a_exec(t_pcb* pid);
void blocked_a_ready(t_pcb* proceso);
void exec_a_blocked(t_pcb* pid);
void exec_a_ready(t_pcb* pid);
void new_a_ready(t_pcb* pid);
void exec_a_blocked_cond_signal(t_pcb* proceso);
void exec_a_ready_cond_signal(t_pcb* proceso);
void exec_a_exit(t_pcb* proceso);
void agregar_a_ready(t_pcb* proceso);
bool eliminar_de_ready(t_pcb* proceso);
void agregar_a_ready_CMN(t_pcb* proceso);
bool eliminar_de_ready_CMN(t_pcb* proceso);

void suspender_proceso(t_pcb* proceso);

void agregar_proceso_a_lista(t_list* lista, t_pcb* proceso, t_tipo_estado nuevo_estado, pthread_mutex_t* m);
bool eliminar_proceso_de_lista(t_list* lista, t_pcb* proceso, char* nombre_lista, pthread_mutex_t* m);
t_pcb* nuevo_proc(int prioridad, int ppid, char* instrucciones); 
t_sublista_ready* sublista_ready_de_prioridad(int prioridad);

// para desbloquear procesos

void desbloquear_proceso(t_pcb* proceso);
void manejar_status_op(t_pcb* proceso, t_status_op status);

// obtener por clave y getters

t_pcb* proceso_de_lista(int pid, t_lista_estado* estado);
t_pcb* get_proceso_de_pid_thread_safe(int pid);
t_list* lista_from_enum(t_tipo_estado e);
t_tipo_estado get_estado(t_pcb* p);
int get_pid_thread_safe(t_pcb* p);
t_pcb* get_proceso_de_cpu_thread_safe(t_cpu*cpu);
int get_pid_de_proceso_de_cpu_thread_safe(t_cpu* cpu);
t_evt* get_evt_de_proceso(t_pcb* proceso);

// funciones para modificar y setters

void cambiar_prioridad(t_pcb* proceso, int prioridad);
void set_status(t_pcb* pcb, t_status_op status); 
void cambiar_de_evt(t_pcb* proceso, t_evt* evt);
void cambiar_estado_global(t_estado_sch e);
void set_proceso_thread_safe(t_cpu* cpu, t_pcb* proceso);

// liberar

void liberar_cpu(t_cpu* cpu);
void liberar_pcb_de_exit(int pid);
void liberar_evt(t_evt* evt, void (*free_data)(void*));
void liberar_evt_stdout(void* arg);

// funciones genericas

void finalizar_evento(t_evt* evt);
t_cpu* cpu_de_pid(int pid);
void generica_transicion_de_estados(t_pcb* proceso, t_lista_estado*, t_lista_estado* add);
t_planificacion obtener_algoritmo_planificacion(char *algoritmo_str);
void intentar_planificar();
void enviar_paquete_a_todas_las_cpus(t_paquete* paquete);
void pedido_desalojo_a_todas_las_cpus(op_code cod_op);
bool hay_cpus_ejecutando();
void sumar_milisegundos(struct timespec* ts, int milisegundos);
t_planificacion algoritmo_de_proceso(t_pcb*);

// logs

void log_obligatorio_cambio_de_estado(int pid, char* estado_anterior, char* estado_actual);
void loguear_tamanio_listas_de_estado();
void imprimir_estado_procesos();

#endif /* BASE_SCH_H_ */