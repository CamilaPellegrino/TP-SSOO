#ifndef BASE_SCH_H_
#define BASE_SCH_H_

#include <utils/utils.h>


typedef struct
{
	int fd;
	int id;
	t_pcb* proceso; // proceso que esta ejecutando (NULL si no esta ejecutando nada)
} t_cpu;

typedef struct
{
	int fd;
	t_tipo_io tipo;
} t_io;

typedef enum{
    CMN,
    FIFO,
    RR
} t_planificacion;

typedef struct{
    int pid;
    int prioridad;
	t_tipo_estado estado;
} t_pcb;


t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);


#endif