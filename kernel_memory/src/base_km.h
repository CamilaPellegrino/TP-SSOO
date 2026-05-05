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


t_stick* iniciar_stick(char* ip, char* puerto, int tamanio, int cliente_fd);
void destruir_stick(t_stick* stick);
t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);


#endif