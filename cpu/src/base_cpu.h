#ifndef BASE_CPU_H_
#define BASE_CPU_H_

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
	int priodidad; // le sacamos la prioridad? solo la necesita scheduler y ya la tiene
	t_registros registros;
    t_list* tabla_segmentos;
} t_pcb;

typedef struct
{
	int id_segmento;
	uint32_t base;
    uint32_t limite;
} t_segmento; //niki

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

typedef struct {
    instrucciones tipo;

    t_list * registros; // void*

} t_instruccion_decodificada;


typedef struct {
    instrucciones tipo;

    t_list * registros;

} t_instruccion_decodificada;

typedef enum{
    SYSCALL_SLEEP,
    SYSCALL_STDOUT,
    SYSCALL_STDIN,
    SYSCALL_EXIT,
    SYSCALL_MUTEX_LOCK,
    SYSCALL_MUTEX_UNLOCK,
    SYSCALL_MUTEX_CREATE,
    SYSCALL_MEM_ALLOC,
    SYSCALL_MEM_FREE,
    SYSCALL_INIT_PROC
} t_syscall;

t_stick* iniciar_stick(char* ip, char* puerto, int tamanio, int cliente_fd);
void destruir_stick(t_stick* stick);
t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);


#endif