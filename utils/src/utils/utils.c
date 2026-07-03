#include "utils.h"
void send_all(int socket, void* buffer, size_t size);

void* serializar_paquete(t_paquete* paquete, int bytes){
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
    send_all(conexion, &handshake, sizeof(int32_t));
    recv(conexion, &result, sizeof(int32_t), MSG_WAITALL);

    if (result == 0){
        log_info(logger,"Handshake Exitoso de cliente!");
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
        send_all(cliente_fd, &resultOk, sizeof(int32_t));
		log_info(logger,"Handshake Exitoso de Servidor!");
    }
    else {
		log_error(logger,"Error inesperado en el handshake");
        send_all(cliente_fd, &resultError, sizeof(int32_t));
    }
}

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
		log_error(logger, "Error: No se pudo conectar a %s", mensaje);
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

int iniciar_servidor_o_exit(char* puerto, t_log* logger){
    int fd = iniciar_servidor(puerto);

    if(fd == -1){
        log_error(logger, "Error: No se pudo iniciar el servidor");
        exit(EXIT_FAILURE);
    }
    return fd;
}

int* esperar_cliente(int socket_servidor)
{
	int *socket_cliente = malloc(sizeof(int));
	*socket_cliente = accept(socket_servidor, NULL, NULL);
	return socket_cliente;
}

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
	send_all(socket, &cod_op, sizeof(int));
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

	send_all(socket_cliente, a_enviar, bytes);

	free(a_enviar);
	eliminar_paquete(paquete);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send_all(socket_cliente, a_enviar, bytes);

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

void send_all(int socket, void* buffer, size_t size){
    size_t enviados = 0;

    while(enviados < size){
        int r = send(socket, buffer + enviados, size - enviados, 0);
        if(r <= 0){// error
            return;
        }

        enviados += r;
    }
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
		exit(EXIT_FAILURE);
	}
	return nuevo_logger;
}

// otras

char* status_op_a_string(t_status_op s){
	static char* status_strings[] = {
		"OK",
		"ERROR",
		"SEG_FAULT",
		"RECURSO_NO_EXISTE",
		"RECURSO_YA_EXISTE",
		"INSTRUCCION_INVALIDA",
		"FIN_INVALIDO"
	};
    if (s < 0 || s >= FIN_INVALIDO + 1)
        return "UNKNOWN_STATUS";
    return status_strings[s];
}

pthread_t crear_hilo_o_exit(void* (*funcion)(void*), void* arg, char* nombre_hilo, t_log* logger){
    pthread_t hilo;
    int resultado = pthread_create(&hilo, NULL, funcion, arg);

    if(resultado != 0){
        log_error(logger, "Error al crear el hilo %s. Codigo: %d", nombre_hilo, resultado);
        exit(EXIT_FAILURE);
    }
    return hilo;
}

void imprimir_bytes(void* data, int tamanio){
    uint8_t* bytes = (uint8_t*) data;
        printf("Bytes: ");
        for(int j = 0; j < tamanio; j++) {
            printf("%02X ", bytes[j]);
        }

    printf("\n");
}

// // Definimos un tipo de puntero a función que acepte dos argumentos
// typedef void (*t_closure_con_arg)(void*, void*);

// void list_iterate_con_argumento(t_list* lista, t_closure_con_arg closure, void* argumento_extra) {
//     if (lista == NULL || closure == NULL) return;

//     // Iteramos sobre todos los elementos de la lista
//     for (int i = 0; i < list_size(lista); i++) {
//         void* elemento = list_get(lista, i); // Obtenemos la CPU
//         closure(elemento, argumento_extra);  // Llamamos a la función
//     }
// }