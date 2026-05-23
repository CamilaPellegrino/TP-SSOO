#include "base_km.h"
// iniciar
t_stick* iniciar_stick(char* ip, char* puerto, int tamanio, int cliente_fd){
	t_stick* nuevo_stick = malloc(sizeof(t_stick));
	if (nuevo_stick != NULL) {
		nuevo_stick->ip = strdup(ip);
		nuevo_stick->puerto = strdup(puerto);
		nuevo_stick->fd = cliente_fd;
		nuevo_stick->tamanio = tamanio;
	}
	return nuevo_stick;
}

void destruir_stick(t_stick* stick){
	free(stick);
}

t_cpu* iniciar_cpu(int id, int fd){
	t_cpu* nuevo_cpu = malloc(sizeof(t_stick));
	if (nuevo_cpu != NULL) {
		nuevo_cpu->id = id;
		nuevo_cpu->fd = fd;
	}
	return nuevo_cpu;
}

t_io* iniciar_io(t_tipo_io tipo, int io_fd){
	t_io* nueva_io = malloc(sizeof(t_io));
	if(nueva_io != NULL){
		nueva_io->tipo = tipo;
		nueva_io->fd = io_fd;
	}
	return nueva_io;
}


char* ruta_completa(char* base, char* nombre_archivo){
	size_t len1 = strlen(base);
    size_t len2 = strlen(nombre_archivo);

    char* resultado = malloc(len1 + len2 + 2);

    if (resultado == NULL) {
        return NULL;
    }

    sprintf(resultado, "%s/%s", base, nombre_archivo);
    return resultado;
}