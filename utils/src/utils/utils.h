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
// #include<commons/string.h>
#include<commons/config.h>
#include<commons/collections/list.h>

#include <pthread.h>
typedef enum
{
	MENSAJE,
	PAQUETE,
	HANDSHAKE,
	SCH_KM__CONEXION,
	STICK_KM__CONEXION,
	SWAP_KM__CONEXION,
	CPU_SCH__CONEXION,
	IO_SCH__CONEXION,
	CPU_KM__CONEXION,
	SCH_CPU__PID
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


//handshake

void handshake_servidor(int cliente_fd, t_log* logger);
void handshake_cliente(int conexion,t_log* logger);

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




#endif  /* UTILS_H_ */ 