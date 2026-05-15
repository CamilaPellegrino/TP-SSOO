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

typedef struct
{
	// identificadores del proceso
	int pid;
	int ppid;
	int priodidad;
	t_tipo_estado estado;
	// registros de estado
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

typedef enum {
    INST_NOOP,
    INST_SET,
    INST_SUM,
    INST_SUB,
    INST_JNZ,
    INST_SET_PC,
    INST_COPY_MEM,
    INST_MOV_IN,
    INST_MOV_OUT,
    INST_MUTEX_CREATE, // sys  no bloqueante
    INST_MUTEX_LOCK,   // sys  bloqueante
    INST_MUTEX_UNLOCK, // sys  no bloqueante
    INST_MEM_ALLOC,    // sys  
    INST_MEM_FREE,     // sys
    INST_SLEEP,        // sys  bloqueante
    INST_STDOUT,       // sys  bloqueante
    INST_STDIN,        // sys  bloqueante
    INST_INIT_PROC,    // sys  no bloqueante
    INST_EXIT          // sys  no bloqueante
} t_tipo_instruccion;

typedef struct{
	t_tipo_instruccion tipo;
	char* param1;
	char* param2;

} t_instruccion;

t_stick* iniciar_stick(char* ip, char* puerto, int tamanio, int cliente_fd);
void destruir_stick(t_stick* stick);
t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);


#endif