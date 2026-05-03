#ifndef UTILS_H_
#define UTILS_H_

#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>

#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<commons/collections/list.h>

#include <pthread.h>
typedef enum
{   // ORIGEN_DESTINO__OPERACION
	// Conexiones
	HANDSHAKE,
	SCH_KM__CONEXION,
	STICK_KM__CONEXION,
	SWAP_KM__CONEXION,
	CPU_SCH__CONEXION,
	IO_SCH__CONEXION,
	CPU_KM__CONEXION,
	CPU_STICK__CONEXION,
	// mensajes con destino a SCH
	KM_SCH__NUEVO_STICK,
	KM_SCH__BSOD,
	// mensajes con destino a CPU
	SCH_CPU__NUEVO_STICK,
	SCH_CPU__PID
	// mensajes con destino a KM
	// ...

}op_code;

typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

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

typedef enum
{
	SLEEP,
	STDIN,
	STDOUT
} t_tipo_io;
typedef struct
{
	int fd;
	t_tipo_io tipo;
} t_io;

typedef enum
{
	NUEVO,
	LISTO,
	EJECUTANDO,
	BLOQUEADO,
	FINALIZADO,
	SUSP_LISTO,
	SUSP_BLOQUEADO
} t_tipo_estado;

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

//declaracion de funciones

//handshake

void handshake_servidor(int cliente_fd, t_log* logger);
void handshake_cliente(int conexion, t_log* logger);

// conexiones con sockets

int crear_conexion(char* ip, char* puerto);
void exit_si_error_conexion(int, t_log *, char *);
void liberar_conexion(int socket_cliente);
int iniciar_servidor(char*);
int* esperar_cliente(int);

// paquetes

t_paquete* crear_paquete(op_code);
void eliminar_paquete(t_paquete* paquete);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void agregar_string_a_paquete(t_paquete* paquete, char *string);

// enviar por sockets

void enviar_operacion(int, op_code);
void enviar_mensaje(char*, int, op_code);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void enviar_paquete_y_liberarlo(t_paquete* paquete, int socket_cliente);

// recibir por sockets

t_list* recibir_paquete(int);
char* recibir_mensaje(int);
int recibir_operacion(int);
void* recibir_buffer(int*, int);

// configs

t_config* iniciar_config(char*);

// loggers

t_log* iniciar_logger(char*, char*, t_log_level);
t_log* iniciar_logger_log_level_string(char*, char*, char*);

// stick

t_stick* iniciar_stick(char* ip, char* puerto, int tamanio, int cliente_fd);
void destruir_stick(t_stick* stick);

// cpu

t_cpu* iniciar_cpu(int id, int fd);
t_io* iniciar_io(t_tipo_io tipo_io, int io_fd);

#endif  /* UTILS_H_ */ 