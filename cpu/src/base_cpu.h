#ifndef BASE_CPU_H_
#define BASE_CPU_H_

#include <utils/utils.h>
#include <math.h>
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

typedef struct{
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
} t_registros;

typedef struct
{
	// identificadores del proceso
	int pid;
	int ppid;
	int priodidad; 
	t_registros registros;
    t_list* tabla_segmentos;

} t_pcb;
typedef struct{
	void *ptro_reg;
	int tamanio;
} especificacion_registro;

typedef enum {
    I_NOOP,
    I_SET,
    I_SUM,
    I_SUB,
    I_JNZ,
    I_SET_PC,
    I_COPY_MEM,
    I_MOV_IN,
    I_MOV_OUT,
    I_MUTEX_CREATE, // sys  no bloqueante
    I_MUTEX_LOCK,   // sys  bloqueante
    I_MUTEX_UNLOCK, // sys  no bloqueante
    I_MEM_ALLOC,    // sys  
    I_MEM_FREE,     // sys
    I_SLEEP,        // sys  bloqueante
    I_STDOUT,       // sys  bloqueante
    I_STDIN,        // sys  bloqueante
    I_INIT_PROC,    // sys  no bloqueante
    I_EXIT          // sys  no bloqueante
} instrucciones;

typedef enum{
    LIBRE,
    EXEC,
    WAIT_SYS,
    WAIT_SYS_Y_PROX_DESALOJO,
    WAIT_MEM_ALLOC,
    WAIT_MEM_ALLOC_Y_PROX_DESALOJO
} t_estado_cpu;

typedef enum{
    PEDIR,
    ENVIAR,
    NADA
}t_prox_accion;
typedef struct {
    instrucciones tipo;

    t_list * registros; 

} t_instruccion_decodificada;

typedef struct {
	int pid;
    int id_segmento;
    uint32_t base;  //dir fisica
    uint32_t tamanio;
} t_segmento;

// Variables globales

// variables globales
extern t_log* logger;
extern t_list* lista_sticks;  // lista global para guardar los memory sticks a los que me conecte

extern bool enviar_contexto_y_desalojar;
extern pthread_mutex_t m_enviar_contexto_y_desalojar;

extern bool v_pedido_de_desalojo;
extern pthread_cond_t cond_ejecutar;

extern bool v_pedir_segmentos;
extern pthread_mutex_t m_pedir_segmentos;

extern int pid_pendiente;
extern pthread_mutex_t m_pid_pendiente;

extern t_estado_cpu estado_cpu;
extern pthread_mutex_t m_estado_cpu;

// otros
extern int conexion_kernel_scheduler;
extern int conexion_kernel_memory;

extern int seg_max_size_global;
extern int id_cpu;

// declaracion de funciones

t_stick* iniciar_stick(char* ip, char* puerto, int tamanio, int cliente_fd);
void destruir_stick(t_stick* stick);
t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);
void destruir_instruccion(t_instruccion_decodificada*);
especificacion_registro * obtener_registro(char* registro_crudo, t_pcb *pcb);
t_dir_fisica iniciar_dir_fisica(int id_segmento, int offset);
void escribir_registro(especificacion_registro *reg, int valor);
uint32_t leer_registro(especificacion_registro* reg);
void *registro_a_bytes(especificacion_registro *reg);

// MMU 
int mmu(t_pcb* pcb, uint32_t, uint32_t, uint32_t);
uint32_t calc_id_segmento(int);
uint32_t calc_offset(int);

// Otros

void iniciar_instr_exit(t_instruccion_decodificada* instr, t_status_op s);
void inicializar_variables();

void set_pedir_segmentos(bool r);
bool get_pedir_segmentos();
void set_enviar_contexto_y_desalojar(bool x);
bool get_enviar_contexto_y_desalojar();

void transicionar(t_estado_cpu estado);
void transicionar_thread_safe(t_estado_cpu estado);

void enviar_pcb_actualizado_a_km(t_pcb* pcb);
void agregar_pcb_al_paquete(t_pcb* pcb, t_paquete* p);

#endif