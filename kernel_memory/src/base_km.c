#include "base_km.h"
// inicializar cosas
void inicializar_variables_globales(t_config* config){
	puerto = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    scripts_basepath = config_get_string_value(config, "SCRIPTS_BASEPATH");

    t_log_level log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));
    logger = iniciar_logger("kernel_memory.log", "ProcesoKernelMemory", log_level);
    
    lista_sticks   = list_create();
    lista_cpus     = list_create();
    lista_procesos = list_create();

}

t_proceso* iniciar_proceso(t_pcb* pcb, t_list* instrucciones){
	if(instrucciones == NULL || pcb == NULL){ return NULL; }
    t_proceso* proceso = malloc(sizeof(t_proceso));
	if(proceso == NULL){ return NULL; }
    proceso->pcb = pcb;
    proceso->instrucciones = instrucciones;
    return proceso;
}

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

// ...
t_proceso* proceso_de_pid(int pid){
    for(int i = 0; i < list_size(lista_procesos); i++){
        t_proceso* proceso = list_get(lista_procesos, i);
        if(proceso->pcb->pid == pid){
            return proceso;
        }
    }
    return NULL;
}