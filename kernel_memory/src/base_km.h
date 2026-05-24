#ifndef BASE_KM_H_
#define BASE_KM_H_

#include <utils/utils.h>
typedef struct 
{
    int tamanio;
    int fd;
    char* puerto;
    char* ip;
}t_stick;

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
} t_proceso;

// estructuras para manejo de memoria: 
typedef struct {
    int id_segmento;
    uint32_t base;
    uint32_t tamanio;
} t_segmento;

// variables globales

extern t_log *logger;
extern t_list *lista_sticks;
extern t_list *lista_cpus;

extern int sch_fd; 

extern t_list* lista_procesos;

// De config
extern char* scripts_basepath;
extern char* puerto;

// Funciones

// iniciar cosas
void inicializar_variables_globales(t_config*);
t_proceso* iniciar_proceso(t_pcb*, t_list*);
t_stick* iniciar_stick(char* ip, char* puerto, int tamanio, int cliente_fd);
void destruir_stick(t_stick* stick);
t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);
char* ruta_completa(char* base, char* nombre_archivo);

// ...
t_proceso* proceso_de_pid(int pid);
void agregar_stick_a_paquete(t_paquete* paquete, t_stick* stick);

#endif