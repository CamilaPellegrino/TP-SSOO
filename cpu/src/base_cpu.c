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

void destruir_instruccion(t_instruccion_decodificada*i){
    list_destroy_and_destroy_elements(i->registros, free);
    free(i);
}


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

t_dir_fisica iniciar_dir_fisica(int id_segmento, int offset){
    t_dir_fisica dir;
    dir.id_segmento = id_segmento;
    dir.offset = offset;
    return dir;

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

void *registro_a_bytes(especificacion_registro *reg){
    uint8_t *bytes = malloc(reg->tamanio);
    if (bytes == NULL)
        return NULL;

    if (reg->tamanio == sizeof(uint8_t)) {
        bytes[0] = *(uint8_t *)reg->ptro_reg;
    } else if (reg->tamanio == sizeof(uint32_t)) {
        uint32_t valor = *(uint32_t *)reg->ptro_reg;

        bytes[0] = (valor >> 24) & 0xFF;
        bytes[1] = (valor >> 16) & 0xFF;
        bytes[2] = (valor >> 8)  & 0xFF;
        bytes[3] =  valor        & 0xFF;
    }

    return bytes;
}
// ======== MMU ========

uint32_t calc_id_segmento(int dir_logica){
    return floor(dir_logica / seg_max_size_global);
}
uint32_t calc_offset(int dir_logica){
    return floor(dir_logica % seg_max_size_global);
}

int mmu(t_pcb* pcb, uint32_t tamanio, uint32_t num_seg, uint32_t desp) {
    t_segmento* seg = NULL;

    for (int i = 0; i < list_size(pcb->tabla_segmentos); i++) {
        t_segmento* s = list_get(pcb->tabla_segmentos, i);
        if ((uint32_t)s->id_segmento == num_seg) {
            seg = s;
            break;
        }
    }

    if (seg == NULL ||
        desp > seg->tamanio ||
        desp + tamanio > seg->tamanio) {
        return -1;
    }

    return (int32_t)(seg->base + desp);
}


// ======== OTROS ========



void iniciar_instr_exit(t_instruccion_decodificada* instr, t_status_op s){
    log_debug(logger, "En exit");
    instr->tipo = I_EXIT;
    t_status_op* status = malloc(sizeof(t_status_op)); 
    *status = s;
    list_add(instr->registros, status);
}

void inicializar_variables(){
    v_pedido_de_desalojo = false;
    pid_pendiente = -1;
    lista_sticks = list_create();
    pthread_cond_init(&cond_ejecutar, NULL);
    pthread_mutex_init(&m_estado_cpu, NULL);
    transicionar(LIBRE);
    seg_max_size_global = 256;
    v_pedir_segmentos = false;
    pthread_mutex_init(&m_pedir_segmentos, NULL);
    enviar_contexto_y_desalojar = false;
    pthread_mutex_init(&m_enviar_contexto_y_desalojar, NULL);

}

bool get_pedir_segmentos(){
    pthread_mutex_lock(&m_pedir_segmentos);
    bool r = v_pedir_segmentos;
    pthread_mutex_unlock(&m_pedir_segmentos);
    return r;
}

void set_pedir_segmentos(bool r){
    pthread_mutex_lock(&m_pedir_segmentos);
    v_pedir_segmentos = r;
    pthread_mutex_unlock(&m_pedir_segmentos);
}


bool get_enviar_contexto_y_desalojar(){
    pthread_mutex_lock(&m_enviar_contexto_y_desalojar);
    bool x = enviar_contexto_y_desalojar;
    pthread_mutex_unlock(&m_enviar_contexto_y_desalojar);
    return x;
}

void set_enviar_contexto_y_desalojar(bool x){
    pthread_mutex_lock(&m_enviar_contexto_y_desalojar);
    enviar_contexto_y_desalojar = x;
    pthread_mutex_unlock(&m_enviar_contexto_y_desalojar);
}


void transicionar(t_estado_cpu estado){
    estado_cpu = estado;
    log_debug(logger, "NUEVO ESTADO: %d", estado);
}

void transicionar_thread_safe(t_estado_cpu estado){
    pthread_mutex_lock(&m_estado_cpu);
    transicionar(estado);
    pthread_mutex_unlock(&m_estado_cpu);
}

void enviar_pcb_actualizado_a_km(t_pcb* pcb){
    if(pcb == NULL){
        return;
    }
    log_debug(logger, "<%d> Enviando contexto actualizado a KM, proximo pc: %d", pcb->pid, pcb->registros.pc);
    t_paquete* p = crear_paquete(CPU_KM__ACTUALIZAR_PCB);
    agregar_pcb_al_paquete(pcb, p);

    enviar_paquete_y_liberarlo(p, conexion_kernel_memory);

    op_code cod_op = recibir_operacion(conexion_kernel_memory);
    log_debug(logger, "PCB cargado en km, op=%d", cod_op);
}

void agregar_pcb_al_paquete(t_pcb* pcb, t_paquete* p){
    // datos del pcb
    agregar_a_paquete(p, &pcb->pid, sizeof(pcb->pid));
    agregar_a_paquete(p, &pcb->ppid, sizeof(pcb->ppid));
    agregar_a_paquete(p, &pcb->priodidad, sizeof(pcb->priodidad));

    // registros
    agregar_a_paquete(p, &pcb->registros.pc, sizeof(pcb->registros.pc));

    agregar_a_paquete(p, &pcb->registros.ax, sizeof(pcb->registros.ax));
    agregar_a_paquete(p, &pcb->registros.bx, sizeof(pcb->registros.bx));
    agregar_a_paquete(p, &pcb->registros.cx, sizeof(pcb->registros.cx));
    agregar_a_paquete(p, &pcb->registros.dx, sizeof(pcb->registros.dx));

    agregar_a_paquete(p, &pcb->registros.eax, sizeof(pcb->registros.eax));
    agregar_a_paquete(p, &pcb->registros.ebx, sizeof(pcb->registros.ebx));
    agregar_a_paquete(p, &pcb->registros.ecx, sizeof(pcb->registros.ecx));
    agregar_a_paquete(p, &pcb->registros.edx, sizeof(pcb->registros.edx));

    agregar_a_paquete(p, &pcb->registros.si, sizeof(pcb->registros.si));
    agregar_a_paquete(p, &pcb->registros.di, sizeof(pcb->registros.di));

}
