#ifndef BASE_KM_H_
#define BASE_KM_H_

#include <utils/utils.h>
#include <semaphore.h>
#include <pthread.h>
typedef struct 
{
    int tamanio;
    int fd;
    char* puerto;
    char* ip;
	// t_list* lista_evt;
	// pthread_mutex_t m_lista_evt;
	// sem_t s_list_evt;
}t_stick;

typedef enum
{
	SCH_LECTURA,
	SCH_ESCRITURA,
	SCH_MOVER // ,
	// SUSPENSION
	// DESUSPENSION
}t_tipo_evt;

typedef struct
{
	int fd;
	int id;
} t_cpu;

typedef struct
{
	int fd;
	t_tipo_io tipo;
} t_io;

typedef struct
{
	int pid;
	int ppid;
	int prioridad;
	uint32_t pc;
	uint8_t ax;
	uint8_t bx;
	uint8_t cx;
	uint8_t dx;
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t si;
	uint32_t di;
} t_pcb;

typedef struct {
	t_pcb* pcb;
	t_list* instrucciones;
	t_list* lista_segmentos;
} t_proceso;
 
 typedef struct
{
	t_tipo_evt tipo;
	int pid;
	void* data;
	pthread_mutex_t mutex;
}t_evt;

typedef struct
{
	int base;
	int tamanio;
}t_data_read;

typedef struct
{
	int base;
	int tamanio;
	void* bytes;
}t_data_write;

typedef struct
{
	int base_leer;
	int base_escribir;
	int tamanio;
}t_data_mover;

// estructuras para manejo de memoria: 
typedef struct {
	int pid;
    int id_segmento;
    uint32_t base;  //dir fisica
    uint32_t tamanio;
} t_segmento;

typedef struct {
	uint32_t base;
	uint32_t tamanio;
} t_hueco;

typedef struct{
    int base_en_stick;
    int tamanio;
    t_stick* stick;
}t_data_pedido_stick;

typedef enum {
	BEST_FIT,
	WORST_FIT
}t_fit;
// variables globales

extern t_log *logger;
extern t_list *lista_sticks;
extern t_list *lista_cpus;
extern int sch_fd; 

// listas para cosas de segmentos
extern t_list *lista_segmentos_global;
extern t_list *lista_procesos;
extern t_list *lista_huecos;
extern t_list *lista_eventos_stick; 

// variables para cosas de segmentos
extern t_fit algoritmo_fit;
extern int tamanio_total_mem;
extern int tamanio_total_libre;

// De config
extern char* scripts_basepath;
extern char* puerto;

// mutex
extern pthread_mutex_t m_lista_segmentos_global;
extern pthread_mutex_t m_lista_procesos;
extern pthread_mutex_t m_lista_huecos;
extern pthread_mutex_t m_lista_eventos_stick;
extern pthread_mutex_t m_manejar_memoria;

extern sem_t s_lista_eventos_stick;
extern sem_t s_fin_mover;

// Funciones

// iniciar cosas
void inicializar_variables_globales(t_config*);
t_proceso* iniciar_proceso(t_pcb*, t_list*);
t_stick* iniciar_stick(char* ip, char* puerto, int tamanio, int cliente_fd);
void destruir_stick(t_stick* stick);
t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);
t_evt* iniciar_evt_read(int pid, int base, int tamanio);
t_evt* iniciar_evt_write(int pid, int base, int tamanio, void* bytes);
t_evt* iniciar_evt_mover(int pid, int base_leer, int tamanio, int base_escribir);

// ...
t_proceso* proceso_de_pid(int pid);
t_segmento* segmento_de_id(int id_segmento);
t_stick* stick_por_id(int nro_stick);
bool guardar_nuevo_proceso(int pid, int ppid, char* ruta_instrucciones);

void agregar_evt_a_stick(t_evt* evt);
void agregar_stick_a_paquete(t_paquete* paquete, t_stick* stick);
void agregar_stick(t_stick* stick);

void enviar_nuevo_stick_a_scheduler(t_stick* nuevo_stick, int sch_fd);
void enviar_sticks_a_cpu(int cpu_fd);

void recibir_pcb_actualizado(t_list* valores);
void agregar_pcb_al_paquete(t_pcb* pcb, t_paquete* p); 
void agregar_segmentos_al_paquete(t_list* segmentos, t_paquete* p);
// Manejo de rutas
t_list* instrucciones_de_ruta(char* ruta);
char* ruta_completa(char* base, char* nombre_archivo);

// liberar

void liberar_evt(t_evt* evt);

// Otros

void imprimir_bytes(void* data, int tamanio);
void imprimir_estado_mem();
void imprimir_huecos();
void imprimir_segmentos(); 
#endif