#include "ciclo_instruccion.h"

/* Funciones para ejecutar el ciclo de instruccion
 * Ciclo de instruccion conformado por 3 pasos:
 * 
 * Etapa   | Accion                                                     | Retorno         |
 * --------+------------------------------------------------------------+-----------------+
 * Fetch   | Solicitar la siguiente instruccion a ejecutar al modulo KM | String          |
 * Decode  | Decodificar la instruccion obtenida                        | t_instruccion*  |
 * Execute | Ejecutar la instruccion                                    | --              |
 * 
 */
void ciclo_instruccion(t_pcb *pcb){
        //FETCH
        char *instruccion = fetch(pcb);
        if(instruccion == NULL){
            log_error(logger, "FETCH devolvio NULL");
            t_instruccion_decodificada * i = malloc(sizeof(t_instruccion_decodificada));
            i->registros = list_create();
            iniciar_instr_exit(i, INSTRUCCION_INVALIDA);
            ejecutar_exit(i, pcb);
            destruir_instruccion(i);
            return;
        }
        int pid = pcb->pid;
        log_info(logger, "## PID: %d - FETCH - Program Counter: %d", pid, pcb->registros.pc);
        
        //DECODE
        t_instruccion_decodificada * instruccion_decodificada = malloc(sizeof(t_instruccion_decodificada));
        decode(instruccion, pcb, instruccion_decodificada);
        int pc_antiguo = pcb->registros.pc;
        //execute
        bool actualizar_contexto = execute(instruccion_decodificada, pcb);
        log_info(logger, "## PID: %d - Ejecutando: %s", pid, instruccion);
        if(instruccion_decodificada->tipo == I_MEM_ALLOC){
            enviar_pcb_actualizado_a_km(pcb);
        }
        if(pcb->registros.pc == pc_antiguo)
            pcb->registros.pc++;

        if(actualizar_contexto){
            enviar_pcb_actualizado_a_km(pcb);
        } 
        destruir_instruccion(instruccion_decodificada);
        free(instruccion);
}

char* fetch(t_pcb *pcb){

    t_paquete* paquete = crear_paquete(CPU_KM__FETCH);

    agregar_a_paquete(paquete, &(pcb->pid), sizeof(int));

    agregar_a_paquete(paquete, &(pcb->registros.pc), sizeof(uint32_t));

    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    op_code cod_op = recibir_operacion(conexion_kernel_memory);

    if(cod_op != KM_CPU__INSTRUCCION){
        log_error(logger, "Error FETCH");
        return NULL;
    }
    t_list* lista = recibir_paquete(conexion_kernel_memory);

    char* instruccion = strdup(list_get(lista, 0));

    list_destroy_and_destroy_elements(lista, free);
    return instruccion;
}

void decode(char *instruccion, t_pcb *pcb,  t_instruccion_decodificada * instruccion_decodificada){

    instruccion_decodificada->registros =  list_create();
    
    if(instruccion == NULL){
        log_error(logger, "Instruccion NULL");
        return;
    }
    char ** partes = string_split(instruccion, " ");
    if(string_equals_ignore_case(partes[0], "SET")){
        instruccion_decodificada->tipo = I_SET;
        especificacion_registro *parametro_aux;
        parametro_aux = obtener_registro(partes[1], pcb);
        list_add(instruccion_decodificada->registros, parametro_aux);
        int* numero = malloc(sizeof(int));
        *numero = (int)strtol((partes[2]), NULL, 10);
        list_add(instruccion_decodificada->registros, numero);
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "NOOP")){
        instruccion_decodificada->tipo = I_NOOP;
        string_array_destroy(partes);
        return;
    }

    else if(string_equals_ignore_case(partes[0], "SUM")){
        especificacion_registro *destino, *origen;
        instruccion_decodificada->tipo = I_SUM;
        destino = obtener_registro(partes[1],pcb);
        list_add(instruccion_decodificada->registros, destino);
        origen = obtener_registro(partes[2],pcb);
        list_add(instruccion_decodificada->registros, origen);
        string_array_destroy(partes);
        return;
    }

    else if(string_equals_ignore_case(partes[0], "SUB")){
        especificacion_registro *destino, *origen;
        instruccion_decodificada->tipo = I_SUB;
        destino = obtener_registro(partes[1], pcb);
        list_add(instruccion_decodificada->registros, destino);
        origen= obtener_registro(partes[2], pcb);
        list_add(instruccion_decodificada->registros, origen);
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "JNZ")){
        instruccion_decodificada->tipo = I_JNZ;
        especificacion_registro *parametro_aux;
        parametro_aux = obtener_registro(partes[1], pcb);
        list_add(instruccion_decodificada->registros, parametro_aux);
        int* numero = malloc(sizeof(int));
        *numero = (int)strtol((partes[2]), NULL, 10);
        list_add(instruccion_decodificada->registros, numero);
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "MOV_OUT")){
        instruccion_decodificada->tipo = I_MOV_OUT;
        especificacion_registro* parametro_aux = obtener_registro(partes[1], pcb);
        list_add(instruccion_decodificada->registros, parametro_aux);
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "MOV_IN")){
        instruccion_decodificada->tipo = I_MOV_IN;
        especificacion_registro* parametro_aux = obtener_registro(partes[1], pcb);
        list_add(instruccion_decodificada->registros, parametro_aux);
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "EXIT")){
        iniciar_instr_exit(instruccion_decodificada, OK);
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "SLEEP")){
        instruccion_decodificada->tipo = I_SLEEP;
        int* numero = malloc(sizeof(int));
        *numero = (int)strtol((partes[1]), NULL, 10);
        list_add(instruccion_decodificada->registros, numero); // tiempo_sleep
        string_array_destroy(partes);
        return;
    }    
    else if(string_equals_ignore_case(partes[0], "STDIN")){
        instruccion_decodificada->tipo = I_STDIN;
        especificacion_registro* param1 = obtener_registro(partes[1], pcb);
        especificacion_registro* param2 = obtener_registro(partes[2], pcb);
        list_add(instruccion_decodificada->registros, param1); // dir logica
        list_add(instruccion_decodificada->registros, param2); // tamanio
        string_array_destroy(partes);
        return;
    }    
    else if(string_equals_ignore_case(partes[0], "STDOUT")){
        instruccion_decodificada->tipo = I_STDOUT;
        especificacion_registro* param1 = obtener_registro(partes[1], pcb);
        especificacion_registro* param2 = obtener_registro(partes[2], pcb);
        list_add(instruccion_decodificada->registros, param1); // dir logica
        list_add(instruccion_decodificada->registros, param2); // tamanio
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "MUTEX_LOCK")){
        instruccion_decodificada->tipo = I_MUTEX_LOCK;
        char* nombre_mutex = strdup(partes[1]);
        list_add(instruccion_decodificada->registros, nombre_mutex); // nombre
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "MUTEX_UNLOCK")){
        instruccion_decodificada->tipo = I_MUTEX_UNLOCK;
        char* nombre_mutex = strdup(partes[1]);
        list_add(instruccion_decodificada->registros, nombre_mutex); // nombre
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "MUTEX_CREATE")){
        instruccion_decodificada->tipo = I_MUTEX_CREATE;
        char* nombre_mutex = strdup(partes[1]);
        list_add(instruccion_decodificada->registros, nombre_mutex); // nombre
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "INIT_PROC")){
        instruccion_decodificada->tipo = I_INIT_PROC;
        char* ruta_instr = strdup(partes[1]);
        int* prioridad = malloc(sizeof(int));
        *prioridad = (int)strtol(partes[2], NULL, 10);
        list_add(instruccion_decodificada->registros, ruta_instr);
        list_add(instruccion_decodificada->registros, prioridad);
    }
    else if(string_equals_ignore_case(partes[0], "MEM_ALLOC")){
        instruccion_decodificada->tipo = I_MEM_ALLOC;
        int* id_segmento = malloc(sizeof(int));
        *id_segmento = (int)strtol(partes[1], NULL, 10);
        int* tamanio = malloc(sizeof(int));
        *tamanio = (int)strtol(partes[2], NULL, 10);
        list_add(instruccion_decodificada->registros, id_segmento);
        list_add(instruccion_decodificada->registros, tamanio);
    }
    else if(string_equals_ignore_case(partes[0], "MEM_FREE")){
        instruccion_decodificada->tipo = I_MEM_FREE;
        int* id_segmento = malloc(sizeof(int));
        *id_segmento = (int)strtol(partes[1], NULL, 10);
        list_add(instruccion_decodificada->registros, id_segmento);
    }
    else if(string_equals_ignore_case(partes[0], "COPY_MEM")){
        instruccion_decodificada->tipo = I_COPY_MEM;
        especificacion_registro* reg_tam = obtener_registro(partes[1], pcb);
        list_add(instruccion_decodificada->registros, reg_tam);
        string_array_destroy(partes);
        return;
    }else{
        iniciar_instr_exit(instruccion_decodificada, INSTRUCCION_INVALIDA);
    }
    string_array_destroy(partes);
    return;
}

bool execute(t_instruccion_decodificada * instruccion, t_pcb *pcb){
    switch (instruccion->tipo){
        case I_NOOP:
            break;
        case I_SET: //Asigna al registro el valor pasado como parámetro. SET ax 5
            ejecutar_set(instruccion, pcb);
            break;
        case I_SUM:
            ejecutar_sum(instruccion, pcb);
            break;
        case I_SUB:
            ejecutar_sub(instruccion, pcb);
            break;
        case I_JNZ:
            ejecutar_jnz(instruccion, pcb);
            break;
        case I_COPY_MEM:
            ejecutar_copy_mem(instruccion, pcb);
            break;
        case I_MOV_OUT:{
            ejecutar_mov_out(instruccion, pcb);
            break;
        }
        case I_MOV_IN:{
            ejecutar_mov_in(instruccion, pcb);
            break;
        }
///////////////////////////syscalls//////////////////
        case I_SLEEP:
            ejecutar_sleep(instruccion);
            return true;
        case I_MUTEX_CREATE:
            ejecutar_m_create(instruccion);
            break;
        case I_MUTEX_LOCK:
            ejecutar_m_lock(instruccion);
            break;
        case I_MUTEX_UNLOCK:
            ejecutar_m_unlock(instruccion);
            break;
        case I_STDIN:
            ejecutar_stdin(instruccion, pcb);
            return true;
        case I_STDOUT:
            ejecutar_stdout(instruccion, pcb);
            return true;
        case I_EXIT:
            ejecutar_exit(instruccion, pcb);
            break;
        case I_INIT_PROC:
            ejecutar_init_proc(instruccion);
            break;
        case I_MEM_ALLOC:
            ejecutar_mem_alloc(instruccion);
            break;
        case I_MEM_FREE:
            ejecutar_mem_free(instruccion);
            break;
        default:
            log_warning(logger, "Instruccion no implementada, tipo %d", instruccion->tipo);
            break;
    }
    return false;
}


// instrucciones

void ejecutar_set(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    especificacion_registro* reg =list_get(instruccion->registros,0);
    int *input  =list_get(instruccion->registros,1);
    escribir_registro(reg, *input);
    return;
}

void ejecutar_sum(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    log_info(logger, "Ejecutando instruccion SUM");
    especificacion_registro *destino = list_get(instruccion->registros,0);
    especificacion_registro *origen = list_get(instruccion->registros,1);
    uint32_t suma =leer_registro(destino)+leer_registro(origen);
    escribir_registro(destino, suma);
    return;
}

void ejecutar_sub(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    log_info(logger, "Ejecutando instruccion SUB");
    especificacion_registro *minuendo = list_get(instruccion->registros,0);
    especificacion_registro *sustraendo = list_get(instruccion->registros,1);
    uint32_t resta =leer_registro(minuendo)-leer_registro(sustraendo);
    escribir_registro(minuendo, resta);
    return;
}

void ejecutar_jnz(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    log_info(logger, "Ejecutando instruccion JNZ");
    especificacion_registro* reg_ = list_get(instruccion->registros,0);
    int* nuevo_pc =list_get(instruccion->registros,1);
    log_debug(logger, "pc antes: %i", pcb->registros.pc);
    if(leer_registro(reg_) != 0)
        pcb->registros.pc = *nuevo_pc;
    log_debug(logger, "pc despues: %i", pcb->registros.pc);
    return;
}

void ejecutar_copy_mem(t_instruccion_decodificada* instruccion, t_pcb* pcb){
    log_info(logger, "ejecutando instruccion Copy_mem");
    especificacion_registro *reg = list_get(instruccion->registros, 0);
    uint32_t tamanio = leer_registro(reg);
    uint32_t origen = pcb->registros.si;
    uint32_t destino = pcb->registros.di;
    int id_segmento_origen = calc_id_segmento(origen);
    int offset_origen = calc_offset(origen);

    int id_segmento_destino = calc_id_segmento(destino);
    int offset_destino = calc_offset(destino);
    if(mmu(pcb, tamanio, id_segmento_origen, offset_origen) == -1 || mmu(pcb, tamanio, id_segmento_destino, offset_destino) == -1){
        manejar_seg_fault(pcb);
        return;
    }
    t_dir_fisica dir_fisica_origen = iniciar_dir_fisica(id_segmento_origen, offset_origen);
    t_dir_fisica dir_fisica_destino = iniciar_dir_fisica(id_segmento_destino, offset_destino);

    t_paquete* paquete = crear_paquete(CPU_KM__COPY_MEM);
    agregar_a_paquete(paquete, &pcb->pid, sizeof(pcb->pid));
    agregar_a_paquete(paquete, &dir_fisica_origen, sizeof(dir_fisica_origen));
    agregar_a_paquete(paquete, &dir_fisica_destino, sizeof(dir_fisica_destino));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));

    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    recibir_operacion(conexion_kernel_memory);
}

void ejecutar_mov_in(t_instruccion_decodificada* instr, t_pcb* pcb){
    especificacion_registro* r_datos = list_get(instr->registros, 0);
    uint32_t dir_logica = pcb->registros.si;
    int id_segmento = calc_id_segmento(dir_logica);
    int offset = calc_offset(dir_logica);
    int tamanio = r_datos->tamanio;
    int dir_fisica_stick = mmu(pcb, tamanio, id_segmento, offset);
    if(dir_fisica_stick == -1){
        manejar_seg_fault(pcb);
        return;
    }
    t_dir_fisica dir_fisica = iniciar_dir_fisica(id_segmento, offset);
    t_paquete* paquete = crear_paquete(CPU_KM__MOV_IN);
    agregar_a_paquete(paquete, &dir_fisica, sizeof(dir_fisica));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    agregar_a_paquete(paquete, &pcb->pid, sizeof(pcb->pid));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    /*op_code cod_op = */recibir_operacion(conexion_kernel_memory);
    t_list* data = recibir_paquete(conexion_kernel_memory);
    uint32_t valor = *(uint32_t*)list_get(data, 0);
    log_info(logger, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %d", pcb->pid, dir_fisica_stick, valor);
    escribir_registro(r_datos, valor);
}

void ejecutar_mov_out(t_instruccion_decodificada* instr, t_pcb* pcb){
    especificacion_registro* r_datos = list_get(instr->registros, 0);
    void* datos = registro_a_bytes(r_datos);
    uint32_t dir_logica = pcb->registros.di;
    int id_segmento = calc_id_segmento(dir_logica);
    int offset = calc_offset(dir_logica);
    int tamanio = r_datos->tamanio;
    int dir_fisica_stick = mmu(pcb, tamanio, id_segmento, offset);
    if(dir_fisica_stick == -1){
        manejar_seg_fault(pcb);
        return;
    }
    t_dir_fisica dir_fisica = iniciar_dir_fisica(id_segmento, offset);
    if(r_datos->tamanio == sizeof(uint8_t)){
        log_info(logger, "PID: <%d> - Acción: <ESCRIBIR> - Dirección Física: %d - Valor: %d", pcb->pid, dir_fisica_stick, *(uint8_t*)r_datos->ptro_reg);
    }else if(r_datos->tamanio == sizeof(uint32_t)){
        log_info(logger, "PID: <%d> - Acción: <ESCRIBIR> - Dirección Física: %d - Valor: %d", pcb->pid, dir_fisica_stick, *(uint32_t*)r_datos->ptro_reg);
    }
    t_paquete* paquete =  crear_paquete(CPU_KM__MOV_OUT);
    agregar_a_paquete(paquete, &dir_fisica, sizeof(dir_fisica));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    agregar_a_paquete(paquete, datos, tamanio);
    agregar_a_paquete(paquete, &pcb->pid, sizeof(pcb->pid));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    recibir_operacion(conexion_kernel_memory);
}

// syscalls: 
void ejecutar_mem_free(t_instruccion_decodificada* instr){
    set_pedir_segmentos(true);
    transicionar_thread_safe(WAIT_SYS);
    int id_segmento = *(int*)list_get(instr->registros, 0);
    t_paquete* paquete = crear_paquete(CPU_SCH__MEM_FREE);
    agregar_a_paquete(paquete, &id_segmento, sizeof(id_segmento));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_mem_alloc(t_instruccion_decodificada* instr){
    set_pedir_segmentos(true);
    transicionar_thread_safe(WAIT_MEM_ALLOC);
    int id_segmento = *(int*)list_get(instr->registros, 0);
    int tamanio = *(int*)list_get(instr->registros, 1);
    t_paquete* paquete = crear_paquete(CPU_SCH__MEM_ALLOC);
    agregar_a_paquete(paquete, &id_segmento, sizeof(id_segmento));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_init_proc(t_instruccion_decodificada* instr){
    transicionar_thread_safe(WAIT_SYS);
    char* ruta_archivo_instrucciones = list_get(instr->registros, 0);
    int prioridad = *(int*)list_get(instr->registros, 1);
    t_paquete* paquete = crear_paquete(CPU_SCH__INIT_PROC);
    agregar_string_a_paquete(paquete, ruta_archivo_instrucciones);
    agregar_a_paquete(paquete, &prioridad, sizeof(prioridad));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_exit(t_instruccion_decodificada* instr, t_pcb* pcb){
    t_status_op status = *(t_status_op*)list_get(instr->registros, 0);
    log_info(logger, "%d EXIT[%s]", pcb->pid, status_op_a_string(status));
    transicionar_thread_safe(LIBRE);
    log_debug(logger, "por enviar contexto");
    enviar_pcb_actualizado_a_km(pcb);
    log_debug(logger, "contexto enviado, mandando pedido a scheduler");
    t_paquete* data = crear_paquete(CPU_SCH__EXIT);
    agregar_a_paquete(data, &status, sizeof(status));
    enviar_paquete_y_liberarlo(data, conexion_kernel_scheduler);
}

void ejecutar_m_unlock(t_instruccion_decodificada* instr){
    transicionar_thread_safe(WAIT_SYS);
    char* nombre = (char*)list_get(instr->registros, 0);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_UNLOCK);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_m_lock(t_instruccion_decodificada* instr){
    transicionar_thread_safe(WAIT_SYS);
    char * nombre = list_get(instr->registros, 0);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_LOCK);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_m_create(t_instruccion_decodificada* instr){
    transicionar_thread_safe(WAIT_SYS);
    char * nombre = list_get(instr->registros, 0);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_CREATE);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_stdout(t_instruccion_decodificada* instr, t_pcb* pcb){
    transicionar_thread_safe(LIBRE);
    especificacion_registro *param1 = list_get(instr->registros,0);
    especificacion_registro *param2 = list_get(instr->registros,1);
    uint32_t dir_logica = leer_registro(param1);
    uint32_t tamanio = leer_registro(param2);
    
    int id_segmento = calc_id_segmento(dir_logica);
    int offset = calc_offset(dir_logica);
    int dir_fisica_stick = mmu(pcb, tamanio, id_segmento, offset);
    if(dir_fisica_stick == -1){
        manejar_seg_fault(pcb);
        return;
    }
    t_dir_fisica dir_fisica = iniciar_dir_fisica(id_segmento, offset);

    t_paquete* paquete = crear_paquete(CPU_SCH__STDOUT);
    agregar_a_paquete(paquete, &dir_fisica, sizeof(dir_fisica));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_stdin(t_instruccion_decodificada* instr, t_pcb* pcb){
    transicionar_thread_safe(LIBRE);
    especificacion_registro *param1 = list_get(instr->registros,0);
    especificacion_registro *param2 = list_get(instr->registros,1);
    uint32_t dir_logica = leer_registro(param1);
    uint32_t tamanio = leer_registro(param2);
    
    int id_segmento = calc_id_segmento(dir_logica);
    int offset = calc_offset(dir_logica);
    if(mmu(pcb, tamanio, id_segmento, offset) == -1){
        manejar_seg_fault(pcb);
        return;
    }
    t_dir_fisica dir_fisica = iniciar_dir_fisica(id_segmento, offset);

    t_paquete* paquete = crear_paquete(CPU_SCH__STDIN);
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    agregar_a_paquete(paquete, &dir_fisica, sizeof(dir_fisica));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_sleep(t_instruccion_decodificada* instruccion){
    transicionar_thread_safe(LIBRE);
    int tiempo_sleep = *(int*)list_get(instruccion->registros, 0);
    
    // setear v_pedido_desalojo = false ? lo mismo en stdin stdout
    t_paquete* paquete = crear_paquete(CPU_SCH__SLEEP);
    agregar_a_paquete(paquete, &tiempo_sleep, sizeof(tiempo_sleep));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}


// Otros
void manejar_seg_fault(t_pcb* pcb){
    log_debug(logger, "<%d> ERROR: seg_fault", pcb->pid);
    t_instruccion_decodificada* i = malloc(sizeof(t_instruccion_decodificada));
    i->registros = list_create();
    iniciar_instr_exit(i, SEG_FAULT);
    ejecutar_exit(i, pcb);
    destruir_instruccion(i);
}