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

typedef void (*t_decodificador)(char **partes, t_pcb *pcb,
                                t_instruccion_decodificada *instruccion);

typedef struct {
    char *nombre;
    t_decodificador decodificar;
} t_decodificador_instruccion;

static void decode_set(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    instruccion->tipo = I_SET;
    list_add(instruccion->registros, obtener_registro(partes[1], pcb));
    int *numero = malloc(sizeof(int));
    *numero = (int)strtol(partes[2], NULL, 10);
    list_add(instruccion->registros, numero);
}

static void decode_noop(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    (void)partes; (void)pcb;
    instruccion->tipo = I_NOOP;
}

static void decode_sum(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    instruccion->tipo = I_SUM;
    list_add(instruccion->registros, obtener_registro(partes[1], pcb));
    list_add(instruccion->registros, obtener_registro(partes[2], pcb));
}

static void decode_sub(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    instruccion->tipo = I_SUB;
    list_add(instruccion->registros, obtener_registro(partes[1], pcb));
    list_add(instruccion->registros, obtener_registro(partes[2], pcb));
}

static void decode_jnz(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    instruccion->tipo = I_JNZ;
    list_add(instruccion->registros, obtener_registro(partes[1], pcb));
    int *numero = malloc(sizeof(int));
    *numero = (int)strtol(partes[2], NULL, 10);
    list_add(instruccion->registros, numero);
}

static void decode_mov_out(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    instruccion->tipo = I_MOV_OUT;
    list_add(instruccion->registros, obtener_registro(partes[1], pcb));
}

static void decode_mov_in(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    instruccion->tipo = I_MOV_IN;
    list_add(instruccion->registros, obtener_registro(partes[1], pcb));
} 
  
static void decode_exit(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    (void)partes; (void)pcb;
    iniciar_instr_exit(instruccion, OK);
}

static void decode_sleep(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    (void)pcb;
    instruccion->tipo = I_SLEEP;
    int *numero = malloc(sizeof(int));
    *numero = (int)strtol(partes[1], NULL, 10);
    list_add(instruccion->registros, numero);
}

static void decode_stdin(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    instruccion->tipo = I_STDIN;
    list_add(instruccion->registros, obtener_registro(partes[1], pcb));
    list_add(instruccion->registros, obtener_registro(partes[2], pcb));
}

static void decode_stdout(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    instruccion->tipo = I_STDOUT;
    list_add(instruccion->registros, obtener_registro(partes[1], pcb));
    list_add(instruccion->registros, obtener_registro(partes[2], pcb));
}

static void decode_mutex_lock(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    (void)pcb;
    instruccion->tipo = I_MUTEX_LOCK;
    list_add(instruccion->registros, strdup(partes[1]));
}

static void decode_mutex_unlock(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    (void)pcb;
    instruccion->tipo = I_MUTEX_UNLOCK;
    list_add(instruccion->registros, strdup(partes[1]));
}

static void decode_mutex_create(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    (void)pcb;
    instruccion->tipo = I_MUTEX_CREATE;
    list_add(instruccion->registros, strdup(partes[1]));
}

static void decode_init_proc(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    (void)pcb;
    instruccion->tipo = I_INIT_PROC;
    list_add(instruccion->registros, strdup(partes[1]));
    int *prioridad = malloc(sizeof(int));
    *prioridad = (int)strtol(partes[2], NULL, 10);
    list_add(instruccion->registros, prioridad);
}

static void decode_mem_alloc(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    (void)pcb;
    instruccion->tipo = I_MEM_ALLOC;
    int *id_segmento = malloc(sizeof(int));
    *id_segmento = (int)strtol(partes[1], NULL, 10);
    int *tamanio = malloc(sizeof(int));
    *tamanio = (int)strtol(partes[2], NULL, 10);
    list_add(instruccion->registros, id_segmento);
    list_add(instruccion->registros, tamanio);
}

static void decode_mem_free(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    (void)pcb;
    instruccion->tipo = I_MEM_FREE;
    int *id_segmento = malloc(sizeof(int));
    *id_segmento = (int)strtol(partes[1], NULL, 10);
    list_add(instruccion->registros, id_segmento);
}

static void decode_copy_mem(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
    instruccion->tipo = I_COPY_MEM;
    list_add(instruccion->registros, obtener_registro(partes[1], pcb));
}

void decode(char *instruccion, t_pcb *pcb, t_instruccion_decodificada *instruccion_decodificada){
    static t_decodificador_instruccion decodificadores[] = {
        {"SET", decode_set},
        {"NOOP", decode_noop},
        {"SUM", decode_sum},
        {"SUB", decode_sub},
        {"JNZ", decode_jnz},
        {"MOV_OUT", decode_mov_out},
        {"MOV_IN", decode_mov_in},
        {"EXIT", decode_exit},
        {"SLEEP", decode_sleep},
        {"STDIN", decode_stdin},
        {"STDOUT", decode_stdout},
        {"MUTEX_LOCK", decode_mutex_lock},
        {"MUTEX_UNLOCK", decode_mutex_unlock},
        {"MUTEX_CREATE", decode_mutex_create},
        {"INIT_PROC", decode_init_proc},
        {"MEM_ALLOC", decode_mem_alloc},
        {"MEM_FREE", decode_mem_free},
        {"COPY_MEM", decode_copy_mem}
    };
    instruccion_decodificada->registros = list_create();

    if(instruccion == NULL){
        log_error(logger, "Instruccion NULL");
        iniciar_instr_exit(instruccion_decodificada, INSTRUCCION_INVALIDA);
        return;
    }

    char **partes = string_split(instruccion, " ");
    if(partes == NULL || partes[0] == NULL){
        iniciar_instr_exit(instruccion_decodificada, INSTRUCCION_INVALIDA);
        string_array_destroy(partes);
        return;
    }

    for(size_t i = 0; i < sizeof(decodificadores) / sizeof(decodificadores[0]); i++){
        if(string_equals_ignore_case(partes[0], decodificadores[i].nombre)){
            decodificadores[i].decodificar(partes, pcb, instruccion_decodificada);
            string_array_destroy(partes);
            return;
        }
    }

    iniciar_instr_exit(instruccion_decodificada, INSTRUCCION_INVALIDA);
    string_array_destroy(partes);
}

typedef bool (*t_ejecutador)(t_instruccion_decodificada *instruccion, t_pcb *pcb);

static bool ejecutar_noop(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    (void)instruccion; (void)pcb;
    return false;
}

static bool ejecutar_set_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    ejecutar_set(instruccion, pcb);
    return false;
}

static bool ejecutar_sum_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    ejecutar_sum(instruccion, pcb);
    return false;
}

static bool ejecutar_sub_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    ejecutar_sub(instruccion, pcb);
    return false;
}

static bool ejecutar_jnz_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    ejecutar_jnz(instruccion, pcb);
    return false;
}

static bool ejecutar_copy_mem_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    ejecutar_copy_mem(instruccion, pcb);
    return false;
}

static bool ejecutar_mov_in_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    ejecutar_mov_in(instruccion, pcb);
    return false;
}

static bool ejecutar_mov_out_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    ejecutar_mov_out(instruccion, pcb);
    return false;
}

static bool ejecutar_mutex_create_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    (void)pcb;
    ejecutar_m_create(instruccion);
    return false;
}

static bool ejecutar_mutex_lock_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    (void)pcb;
    ejecutar_m_lock(instruccion);
    return false;
}

static bool ejecutar_mutex_unlock_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    (void)pcb;
    ejecutar_m_unlock(instruccion);
    return false;
}

static bool ejecutar_mem_alloc_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    (void)pcb;
    ejecutar_mem_alloc(instruccion);
    return false;
}

static bool ejecutar_mem_free_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    (void)pcb;
    ejecutar_mem_free(instruccion);
    return false;
}

static bool ejecutar_sleep_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    (void)pcb;
    ejecutar_sleep(instruccion);
    return true;
}

static bool ejecutar_stdout_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    ejecutar_stdout(instruccion, pcb);
    return true;
}

static bool ejecutar_stdin_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    ejecutar_stdin(instruccion, pcb);
    return true;
}

static bool ejecutar_init_proc_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    (void)pcb;
    ejecutar_init_proc(instruccion);
    return false;
}

static bool ejecutar_exit_callback(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    ejecutar_exit(instruccion, pcb);
    return false;
}

bool execute(t_instruccion_decodificada *instruccion, t_pcb *pcb){
    static t_ejecutador ejecutadores[] = {
        [I_NOOP]         = ejecutar_noop,
        [I_SET]          = ejecutar_set_callback,
        [I_SUM]          = ejecutar_sum_callback,
        [I_SUB]          = ejecutar_sub_callback,
        [I_JNZ]          = ejecutar_jnz_callback,
        [I_COPY_MEM]     = ejecutar_copy_mem_callback,
        [I_MOV_IN]       = ejecutar_mov_in_callback,
        [I_MOV_OUT]      = ejecutar_mov_out_callback,
        [I_MUTEX_CREATE] = ejecutar_mutex_create_callback,
        [I_MUTEX_LOCK]   = ejecutar_mutex_lock_callback,
        [I_MUTEX_UNLOCK] = ejecutar_mutex_unlock_callback,
        [I_MEM_ALLOC]    = ejecutar_mem_alloc_callback,
        [I_MEM_FREE]     = ejecutar_mem_free_callback,
        [I_SLEEP]        = ejecutar_sleep_callback,
        [I_STDOUT]       = ejecutar_stdout_callback,
        [I_STDIN]        = ejecutar_stdin_callback,
        [I_INIT_PROC]    = ejecutar_init_proc_callback,
        [I_EXIT]         = ejecutar_exit_callback
    };

    if(instruccion == NULL || (unsigned)instruccion->tipo >=
       sizeof(ejecutadores) / sizeof(ejecutadores[0]) ||
       ejecutadores[instruccion->tipo] == NULL){
        log_warning(logger, "Instruccion no implementada");
        return false;
    }
    return ejecutadores[instruccion->tipo](instruccion, pcb);
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
    list_destroy_and_destroy_elements(data, free);
}

void ejecutar_mov_out(t_instruccion_decodificada* instr, t_pcb* pcb){
    especificacion_registro* r_datos = list_get(instr->registros, 0);
    uint32_t dir_logica = pcb->registros.di;
    int id_segmento = calc_id_segmento(dir_logica);
    int offset = calc_offset(dir_logica);
    int tamanio = r_datos->tamanio;
    int dir_fisica_stick = mmu(pcb, tamanio, id_segmento, offset);
    if(dir_fisica_stick == -1){
        manejar_seg_fault(pcb);
        return;
    }
    void* datos = registro_a_bytes(r_datos);
    if(datos == NULL){
        log_error(logger, "No se pudo reservar memoria para MOV_OUT");
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
    free(datos);
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