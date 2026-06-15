#include "base_cpu.h"

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


/////////////////////////

especificacion_registro* obtener_registro(char* registro_crudo, t_pcb *pcb){
    especificacion_registro *registro = malloc(sizeof(especificacion_registro));
    if(string_equals_ignore_case(registro_crudo, "AX")){
        registro->ptro_reg = &(pcb->registros.ax);
        registro->tamanio = sizeof(uint8_t);
        return registro;
    }
    if(string_equals_ignore_case(registro_crudo, "BX")){
        registro->ptro_reg = &(pcb->registros.bx);
        registro->tamanio = sizeof(uint8_t);
        return registro;
    }
    if(string_equals_ignore_case(registro_crudo, "CX")){
        registro->ptro_reg = &(pcb->registros.cx);
        registro->tamanio = sizeof(uint8_t);
        return registro;
    }
    if(string_equals_ignore_case(registro_crudo, "DX")){
        registro->ptro_reg = &(pcb->registros.dx);
        registro->tamanio = sizeof(uint8_t);
        return registro;
    }
    if(string_equals_ignore_case(registro_crudo, "EAX")){
        registro->ptro_reg = &(pcb->registros.eax);
        registro->tamanio = sizeof(uint32_t);
        return registro;
    }
    if(string_equals_ignore_case(registro_crudo,"EBX")){
        registro->ptro_reg = &(pcb->registros.ebx);
        registro->tamanio = sizeof(uint32_t);
        return registro;
    }
    if(string_equals_ignore_case(registro_crudo, "ECX")){
        registro->ptro_reg = &(pcb->registros.ecx);
        registro->tamanio = sizeof(uint32_t);
        return registro;
    }
    if(string_equals_ignore_case(registro_crudo, "EDX")){
        registro->ptro_reg = &(pcb->registros.edx);
        registro->tamanio = sizeof(uint32_t);
        return registro;
    }
    if(string_equals_ignore_case(registro_crudo, "SI")){
        registro->ptro_reg = &(pcb->registros.si);
        registro->tamanio = sizeof(uint32_t);
        return registro;
    }
    if(string_equals_ignore_case(registro_crudo, "DI")){
        registro->ptro_reg = &(pcb->registros.di);
        registro->tamanio = sizeof(uint32_t);
        return registro;
    }
    if(string_equals_ignore_case(registro_crudo, "PC")){
        registro->ptro_reg = &(pcb->registros.pc);
        registro->tamanio = sizeof(uint32_t);
        return registro;
    }
    return NULL;
}

void escribir_registro(especificacion_registro *reg, int valor){
    switch(reg->tamanio){
        case sizeof(uint8_t):
            *(uint8_t*) reg->ptro_reg = (uint8_t) valor;
            break;
        case sizeof(uint32_t):
            *(uint32_t*) reg->ptro_reg = (uint32_t) valor;
            break;
    }
    return;
}

uint32_t leer_registro(especificacion_registro* reg){
    if(reg->tamanio ==sizeof(uint8_t))
        return*(uint8_t*)reg->ptro_reg;
    return*(uint32_t*)reg->ptro_reg;
}