#include "utils.h"

void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

// hadnshake

void handshake_cliente(int conexion,t_log* logger){
    int32_t handshake = 1;
    int32_t result;
    send(conexion, &handshake, sizeof(int32_t), 0);
    recv(conexion, &result, sizeof(int32_t), MSG_WAITALL);

    if (result == 0){
        log_info(logger,"Handshake Exitoso!");
		return;
    }else {// Handshake ERROR
        log_error(logger,"Error inesperado en el handshake" );
        exit(EXIT_FAILURE);
    
    }
} 

void handshake_servidor(int cliente_fd, t_log* logger){
    int32_t handshake;
    int32_t resultOk = 0;
    int32_t resultError = -1;
	
    recv(cliente_fd, &handshake, sizeof(int32_t), MSG_WAITALL);
    if (handshake == 1) {
        send(cliente_fd, &resultOk, sizeof(int32_t), 0);
		log_info(logger,"ResultOk - enviando respuesta");
    }
    else {
		log_error(logger,"Error inesperado en el handshake");
        send(cliente_fd, &resultError, sizeof(int32_t), 0);
    }
}

/*while (1) {
    pthread_t thread;
    int *fd_conexion_ptr = malloc(sizeof(int));
    fd_conexion_ptr = accept(fd_escucha, NULL, NULL);
	
	switch(caso){
pthread_create(&thread,
                    NULL,
                    (void) esperarcliente,
                    fd_conexion_ptr);
    pthread_detach(thread);

		
	}
    pthread_create(&thread,
                    NULL,
                    (void) esperarcliente,
                    fd_conexion_ptr);
    pthread_detach(thread);
}
*/
// conexiones

int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	int socket_cliente = socket(server_info->ai_family
						  , server_info->ai_socktype
						  , server_info->ai_protocol);
	if (socket_cliente == -1) {
        freeaddrinfo(server_info);
        return -1;
    }
	// Ahora que tenemos el socket, vamos a conectarlo
	if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        close(socket_cliente);
        freeaddrinfo(server_info);
        return -1;
    }

	freeaddrinfo(server_info);

	return socket_cliente;
}

void exit_si_error_conexion(int conexion, t_log *logger, char *mensaje){ // si no pudo conectarse sale del programa
	if(conexion == -1){
		log_error(logger, "No se pudo conectar a %s", mensaje);
		exit(EXIT_FAILURE);
	}
}

void liberar_conexion(int socket_cliente){
	close(socket_cliente);
}

int iniciar_servidor(char* puerto){
	int socket_servidor;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, puerto, &hints, &servinfo) != 0) {
        perror("getaddrinfo");
        return -1;
    }

	// Creamos el socket de escucha del servidor
	socket_servidor = socket(servinfo->ai_family
					, servinfo->ai_socktype
					, servinfo->ai_protocol);
	
	if (socket_servidor == -1) {
        perror("socket");
        freeaddrinfo(servinfo);
        return -1;
    }
	
	setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));		
	// Asociamos el socket a un puerto
	if (bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("bind");
        close(socket_servidor);
        freeaddrinfo(servinfo);
        return -1;
    }
	if (listen(socket_servidor, SOMAXCONN) == -1) {
        perror("listen");
        close(socket_servidor);
        freeaddrinfo(servinfo);
        return -1;
    }

	freeaddrinfo(servinfo);
	return socket_servidor;
}

int* esperar_cliente(int socket_servidor)
{
	// Aceptamos un nuevo cliente
	int *socket_cliente = malloc(sizeof(int));
	*socket_cliente = accept(socket_servidor, NULL, NULL);
	// int *socket_cliente = NULL;
	// int aux = accept(socket_servidor, NULL, NULL);
	// socket_cliente = &aux;
	printf("socket_cliente = %d\n", *socket_cliente);
	return socket_cliente;
}

//El malloc() lo realizamos debido a que, como pthread_create() solamente acepta como parámetro un puntero hacia una posición de memoria, 
//si le pasáramos un puntero a un int que se encuentra en el stack usando &, en el momento en el que el hilo quiera acceder al valor éste 
//se habrá pisado luego del siguiente accept().
//Entonces llegará un punto en el que todos los hilos que creemos van a estar usando siempre el mismo file descriptor (y eso probablemente genere condiciones de carrera).

// paquete

t_paquete* crear_paquete(op_code op_code)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = op_code;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void agregar_string_a_paquete(t_paquete* paquete, char *string){
	agregar_a_paquete(paquete, string, strlen(string) + 1);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

// enviar

void enviar_operacion(int socket, op_code cod_op){
	send(socket, &cod_op, sizeof(int), 0);
}

void enviar_mensaje(char* mensaje, int socket_cliente, op_code op_code)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = op_code;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void enviar_paquete_y_liberarlo(t_paquete* paquete, int socket_cliente){
	enviar_paquete(paquete, socket_cliente);
	eliminar_paquete(paquete);
}

// recibir
int recibir_operacion(int socket_cliente){
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0){
		return cod_op;
	}else
	{
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

char* recibir_mensaje(int socket_cliente)
{
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	return buffer;
}

t_list* recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

// config 

t_config* iniciar_config(char* ruta)
{
	t_config* nuevo_config = config_create(ruta);
	if(nuevo_config == NULL){
		perror("Error: no se pudo iniciar el config");
		exit(EXIT_FAILURE);
	}
	return nuevo_config;
}

// logger

t_log* iniciar_logger_log_level_string(char* ruta, char* process_name, char* log_level_str){
	t_log_level log_level = log_level_from_string(log_level_str);

    if(log_level == -1){
        printf("ERROR: no se pudo crear el logger\n");
        exit(EXIT_FAILURE);
    }
	return iniciar_logger(ruta, process_name, log_level);
}

t_log* iniciar_logger(char* ruta, char* process_name, t_log_level log_level)
{
	t_log* nuevo_logger = log_create(ruta, process_name, 1, log_level);
	if(nuevo_logger == NULL){
		perror("error al iniciar el logger");
		exit(EXIT_FAILURE); // termina el programa
	}
	return nuevo_logger;
}

