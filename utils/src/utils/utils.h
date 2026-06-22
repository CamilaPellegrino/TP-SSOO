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
	KM_SCH__EXIT_OK,
	KM_SCH__INIT_PROC_RESP,
	KM_SCH__RTA_MEM_ALLOC,
	KM_SCH__RTA_MEM_FREE,
	KM_SCH__LECTURA,
	KM_SCH__PEDIDO_COMPACTACION,
	KM_SCH__COMPACTACION_COMPLETA,
	CPU_SCH__INIT_PROC,
	CPU_SCH__SLEEP,
	CPU_SCH__STDIN,
	CPU_SCH__STDOUT,
	CPU_SCH__MUTEX_CREATE,
	CPU_SCH__MUTEX_LOCK,
	CPU_SCH__MUTEX_UNLOCK,
	CPU_SCH__MEM_ALLOC,
	CPU_SCH__MEM_FREE,
	CPU_SCH__EXIT,
	CPU_SCH__EJECUCION_DETENIDA,
	IO_SCH__OK,
	IO_SCH__ERROR,
	// mensajes con destino a CPU
	SCH_CPU__NUEVO_STICK,
	SCH_CPU__PID,
	SCH_CPU__PEDIDO_DESALOJO,
	SCH_CPU__FIN_SYSCALL,
	SCH_CPU__SYS_BLOQUEANTE,
	SCH_CPU__COMPACTACION,

	KM_CPU__RESPUESTA,
	KM_CPU__INSTRUCCION,
	KM_CPU__RTA_CONTEXTO,
	KM_CPU__STICKS,
	KM_CPU__PCB_GUARDADO,
	// mensajes con destino a KM
	KM_READ,
	KM_WRITE,
	CPU_KM__PCONTEXTO,
	CPU_KM__PSEGMENTOS,
	CPU_KM__ACTUALIZAR_PCB,
	CPU_KM__FETCH,
	SCH_KM__EXIT,
	SCH_KM__INIT_PROC,
	SCH_KM__MEM_ALLOC,
	SCH_KM__MEM_FREE,
	CPU_KM__MOV_OUT,
	CPU_KM__MOV_IN,
	CPU_KM__COPY_MEM,
	SCH_KM__COMENZAR_COMPACTACION,
	SWAP_KM__OK,
	// mensajes con destino a IO
	SCH_IO__SOLICITUD,
	// mensajes con destino a SWAP
	KM_SWAP__ESCRITURA,
	KM_SWAP__LECTURA,
	// mensajes con destino a Sticks
	X_STICK__ESCRITURA,
	X_STICK__LECTURA,
	// confirmaciones de sticks
	STICK_X__OK,
	STICK_X__ERROR,
	// confirmaciones de KM
	RTA_READ,
	RTA_WRITE
}op_code;

typedef enum{
	OK,
	ERROR
} t_status_op;

typedef struct{
	int id_segmento;
	int offset; 
}t_dir_fisica;
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
typedef enum
{
	SLEEP,
	STDIN,
	STDOUT,
	INVALIDO
} t_tipo_io;

typedef enum
{
	NUEVO,
	LISTO,
	EJECUTANDO,
	BLOQUEADO,
	FINALIZADO,
	SUSP_LISTO,
	SUSP_BLOQUEADO,
} t_tipo_estado;

//declaracion de funciones

//handshake

void handshake_servidor(int cliente_fd, t_log* logger);
void handshake_cliente(int conexion, t_log* logger);

// conexiones con sockets

int crear_conexion(char* ip, char* puerto);
void exit_si_error_conexion(int, t_log *, char *);
void liberar_conexion(int socket_cliente);
int iniciar_servidor(char*);
int iniciar_servidor_o_exit(char* puerto, t_log* logger);
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

// otras
char* status_op_a_string(t_status_op s);
pthread_t crear_hilo_o_exit(void* (*funcion)(void*), void* arg, char* nombre_hilo, t_log* logger);
void imprimir_bytes(void* data, int tamanio);

#endif  /* UTILS_H_ */