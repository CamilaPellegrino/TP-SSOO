#ifndef BASE_SCH_H_
#define BASE_SCH_H_

#include <utils/utils.h>
#include <semaphore.h>
#include <pthread.h>

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

typedef struct{
    int pid;
    int ppid;
    int prioridad;
	t_tipo_estado estado;
	t_data_cond data_cond;
	pthread_mutex_t mutex;
} t_pcb;
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
	int tamanio; // cantidad de bits a leer
	int dir_logica; // TODO: Como implementar esto? 3er entrega
}t_evt_std_in_out;

typedef struct{
	void* data_evt;
	t_pcb* proceso;
	bool syscall_finalizada;
	pthread_mutex_t mutex;
    pthread_cond_t cond;
	pthread_t hilo_timeout;
} t_evt;
typedef enum{
    CMN,
    FIFO,
    RR
} t_planificacion;

// para t_mutex
typedef struct{
    char* nombre;
    int mutex_id;
    t_pcb* duenio;
    t_list* procesos_en_espera; // procesos que esperan por el mutex, en orden
    pthread_mutex_t lock;
} t_mutex;

// variables globales 
extern t_log* logger;
extern t_planificacion algoritmo;  // CMN, FIFO o RR
extern int proximo_pid;
extern int suspension_timeout;
extern int quantum;

extern t_list* lista_cpus;
extern t_list* lista_io;
extern t_list* lista_new;
extern t_list* lista_ready;        // lista procesos en ready
extern t_list* lista_exec;         // lista de procesos en exec
extern t_list* lista_blocked;      // procesos en blocked
extern t_list* lista_susp_blocked;
extern t_list* lista_susp_ready;
extern t_list* lista_exit;

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
extern pthread_mutex_t m_lista_evt_sleep;
extern pthread_mutex_t m_lista_evt_stdin;
extern pthread_mutex_t m_lista_evt_stdout;
extern pthread_mutex_t m_lista_new;
extern pthread_mutex_t m_lista_ready;
extern pthread_mutex_t m_lista_exec;
extern pthread_mutex_t m_lista_blocked;
extern pthread_mutex_t m_lista_susp_blocked;
extern pthread_mutex_t m_lista_susp_ready;
extern pthread_mutex_t m_lista_exit;
extern pthread_mutex_t m_lista_cpus;
extern pthread_mutex_t m_lista_mutex;

extern pthread_mutex_t m_proximo_pid;

// sockets
extern int conexion_kernel_memory;



// funciones para inicializar cosas
void inicializar_variables_globales(t_config* config);
void inicializar_parametros_de_config(t_config* config);
t_list* queues_algorithms_a_t_list(char** queues_algorithms_str);
t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);
t_pcb* iniciar_pcb(int pid, int ppid, int prioridad, t_tipo_estado estado);
t_evt* iniciar_evt_sleep(int tiempo_sleep, t_pcb* proceso);
t_evt* iniciar_evt_std_in_out(int tamanio, int dir_logica, t_pcb* proceso);

// Funciones para modificar
void cambiar_prioridad(t_pcb* proceso, int prioridad);

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
void agregar_proceso_a_lista(t_list* lista, t_pcb* proceso, t_tipo_estado nuevo_estado, pthread_mutex_t* m);
bool eliminar_proceso_de_lista(t_list* lista, t_pcb* proceso, char* nombre_lista, pthread_mutex_t* m);
t_pcb* nuevo_proc(int prioridad, int ppid, char* instrucciones);
t_sublista_ready* sublista_ready_de_prioridad(int prioridad);
void desbloquear_proceso(t_pcb* proceso);

// funciones genericas
t_planificacion obtener_algoritmo_planificacion(char *algoritmo_str);
void enviar_paquete_a_todas_las_cpus(t_paquete* paquete);
void sumar_milisegundos(struct timespec* ts, int milisegundos);
void loguear_tamanio_listas_de_estado();
t_planificacion algoritmo_de_proceso(t_pcb*);
 
// liberar
void liberar_cpu(t_cpu* cpu);     // liberar la cpu, osea que no tenga asignado ningun proceso (no le manda nada a la cpu, solo hace cpu->proceso=NULL)
void liberar_pcb_de_exit(int pid);

#endif /* BASE_SCH_H_ */