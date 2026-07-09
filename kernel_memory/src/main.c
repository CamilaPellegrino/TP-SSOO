#include "main.h"

int main(int argc, char* argv[]) {
    if(argc < 2){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/kernel_memory ./kernel_memory.config");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];

    t_config* config = iniciar_config(ruta_config);
    inicializar_variables_globales(config);

    // testear(); // Descomentar para ejecutar TESTS

    int kernel_memory_fd = iniciar_servidor_o_exit(puerto, logger);

    // pthread_t thread_stick = crear_hilo_o_exit(atender_stick, NULL, "atender_stick", logger);
    // pthread_detach(thread_stick);

    while(true){ 
        int *cliente_fd = esperar_cliente(kernel_memory_fd);
        
        log_info(logger, "Me llego un cliente, %d", *cliente_fd);
        handshake_servidor(*cliente_fd, logger);

        atender_cliente(cliente_fd);
    }
    list_destroy_and_destroy_elements(lista_sticks, free);
}

void* atender_cliente(void *arg){
    int *cliente_fd_ptr = (int *) arg;
    int cliente_fd = *cliente_fd_ptr;
    free(cliente_fd_ptr);
    op_code cod_op = recibir_operacion(cliente_fd);
    switch(cod_op){
        case SCH_KM__CONEXION: {
            char *msg = recibir_mensaje(cliente_fd);
            sch_fd = cliente_fd;
            log_info(logger, "## Kernel Scheduler Conectado - FD del socket: <%d>", sch_fd);
            pthread_t thread_sch = crear_hilo_o_exit(atender_scheduler, NULL, "atender_scheduler", logger);
            pthread_detach(thread_sch);
            free(msg);
            break;
        }case STICK_KM__CONEXION: {
            t_list *lista_paquete = recibir_paquete(cliente_fd);

            int tamanio = *(int*)list_get(lista_paquete, 0);
            char *puerto = list_get(lista_paquete, 1);
            char *ip = list_get(lista_paquete, 2);
            log_debug(logger, "Me llego un stick: %d, %s, %s", tamanio, puerto, ip);
            
            t_stick* nuevo_stick = iniciar_stick(ip, puerto, tamanio, cliente_fd);
            
            agregar_espacio_mem_thread_safe(nuevo_stick->tamanio);
            agregar_stick(nuevo_stick);

            enviar_nuevo_stick_a_scheduler(nuevo_stick, sch_fd);
            log_info(logger, "## Memory Stick de <%d> bytes Conectada", nuevo_stick->tamanio);
          
            // pthread_t thread_stick = crear_hilo_o_exit(atender_stick, nuevo_stick, "atender_stick", logger);
            // pthread_detach(thread_stick);

            list_destroy_and_destroy_elements(lista_paquete, free);
            break;
        }case SWAP_KM__CONEXION:{
            t_list* data = recibir_paquete(cliente_fd);
            conexion_swap = cliente_fd;
            tam_bloque = *(int*)list_get(data, 0);
            cant_bloques = *(int*)list_get(data, 1);
            log_info(logger, "## Swap conectado, Tamaño de bloque: %d, cantidad de bloques: %d", tam_bloque, cant_bloques);
            list_destroy_and_destroy_elements(data, free);
            
            pthread_t thread_stick = crear_hilo_o_exit(atender_stick, NULL, "atender_stick", logger);
            pthread_detach(thread_stick);
            break;
        }case CPU_KM__CONEXION:{
            t_list *lista_paquete = recibir_paquete(cliente_fd);
            int *cpu_id = list_get(lista_paquete, 0);

            t_cpu* cpu = iniciar_cpu(*cpu_id, cliente_fd);
            // list_add(lista_cpus, cpu);

            // mandarle todos los sticks que se conectaron hasta ahora
            enviar_sticks_a_cpu(cliente_fd); // + seg_max_size
            pthread_t thread_cpu = crear_hilo_o_exit(atender_cpu, cpu, "atender_cpu", logger);
            pthread_detach(thread_cpu);
            list_destroy_and_destroy_elements(lista_paquete, free);
            break;
        }default: 
            log_warning(logger, "Warning: Operacion desconocida en atender_cliente, cod_op = %d", cod_op);
    }
    return NULL;
}

void* atender_cpu(void* arg){
    t_cpu* cpu = (t_cpu*)arg;
    int cpu_fd = cpu->fd;
    int cpu_id = cpu->id;
    log_info(logger, "## CPU <%d> Conectada", cpu_id);
    while(1){
        op_code cod_op = recibir_operacion(cpu_fd);
        
        if(cod_op == -1){
            // log_warning(logger, "se desconecto CPU");
            break;
        }
        log_debug(logger, "LLEGO OP de CPU=%d", cod_op);
        switch(cod_op) {
            case CPU_KM__BSOD:{
                log_error(logger, "BSOD");
                exit(EXIT_FAILURE);
                break;
            }
            case CPU_KM__PCONTEXTO:{
                t_list *lista = recibir_paquete(cpu_fd);
                atender_cpu_pcontexto(cpu, lista);
                break;
            }case CPU_KM__PSEGMENTOS:{
                t_list* lista = recibir_paquete(cpu_fd);
                atender_cpu_psegmentos(cpu, lista);
                break;
            }          
            case CPU_KM__FETCH: {
                t_list* lista = recibir_paquete(cpu_fd);
                atender_cpu_fetch(cpu, lista);
                break;
            }case CPU_KM__ACTUALIZAR_PCB:{
                log_debug(logger, "Pedido de actualizar contexto");
                t_list* p = recibir_paquete(cpu_fd);
                recibir_pcb_actualizado(p);
                enviar_operacion(cpu_fd, KM_CPU__PCB_GUARDADO);
                log_debug(logger, "PCB actualizado");
                list_destroy_and_destroy_elements(p, free);
                break;
            }case CPU_KM__MOV_OUT:{
                log_debug(logger, "CPU pide: MOV_OUT");
                t_list* data = recibir_paquete(cpu_fd);
                atender_cpu_mov_out(data, cpu_fd);
                list_destroy_and_destroy_elements(data, free);
                break;
            }case CPU_KM__MOV_IN:{
                log_debug(logger, "CPU pide: MOV_IN");
                t_list* data = recibir_paquete(cpu_fd);
                atender_cpu_mov_in(data, cpu_fd);
                list_destroy_and_destroy_elements(data, free);
                break;
            }case CPU_KM__COPY_MEM:{
                log_debug(logger, "CPU pide: COPY_MEM");
                t_list* data = recibir_paquete(cpu_fd);
                atender_cpu_copy_mem(data, cpu_fd);
                list_destroy_and_destroy_elements(data, free);
                break;
            }default:{
                log_warning(logger,"atender_cpu: op desconocida, op=%d", cod_op);
            }
        }
    }
    close(cpu_fd);
    log_info(logger, "cerrando hilo de CPU");
    destroy_cpu(cpu);
    return NULL;
}

void atender_cpu_psegmentos(t_cpu* cpu, t_list* data){
    int pid = *(int*)list_get(data, 0);
    t_paquete* paquete = crear_paquete(KM_CPU__RTA_CONTEXTO);
    t_proceso* proceso_actual = proceso_de_pid_thread_safe(pid);

    if(proceso_actual != NULL){
        log_debug(logger, "Enviando segmentos de proceso <%d> a cpu <%d>", pid, cpu->id);
        pthread_mutex_lock(&proceso_actual->mutex);
        t_list* segmentos = proceso_actual->lista_segmentos;
        pthread_mutex_unlock(&proceso_actual->mutex);
        agregar_segmentos_al_paquete(segmentos, paquete);
        enviar_paquete_y_liberarlo(paquete, cpu->fd);
    }else{
        log_error(logger, "Atender_cpu de id <%d>, Error: pid <%d> no encontrado", cpu->id, pid);
    }
    list_destroy_and_destroy_elements(data, free);
}

void atender_cpu_pcontexto(t_cpu* cpu, t_list* data){
    int pid = *(int*)list_get(data, 0);

    t_paquete* paquete = crear_paquete(KM_CPU__RTA_CONTEXTO);
    t_proceso* proceso_actual = proceso_de_pid_thread_safe(pid);

    if(proceso_actual != NULL){
        pthread_mutex_lock(&proceso_actual->mutex);
        log_debug(logger, "Enviando contexto de proceso <%d> a cpu <%d>", pid, cpu->id);
        agregar_pcb_al_paquete(proceso_actual->pcb, paquete);
        t_list* segmentos_del_proc = proceso_actual->lista_segmentos;
        pthread_mutex_unlock(&proceso_actual->mutex);
        agregar_segmentos_al_paquete(segmentos_del_proc, paquete);
        enviar_paquete_y_liberarlo(paquete, cpu->fd);
    }else{
        log_error(logger, "Atender_cpu de id <%d>, Error: pid <%d> no encontrado", cpu->id, pid);
    }
    list_destroy_and_destroy_elements(data, free);
}

void atender_cpu_fetch(t_cpu* cpu, t_list* data){
    int i = 0;
    int pid = *(int*) list_get(data, i++);
    uint32_t pc = *(uint32_t*) list_get(data, i++);

    pthread_mutex_lock(&m_lista_procesos);
    t_proceso* proceso = proceso_de_pid(pid);

    if(proceso != NULL){
        pthread_mutex_lock(&proceso->mutex);
    }
    pthread_mutex_unlock(&m_lista_procesos);

    if(proceso != NULL){
        char* instruccion = list_get(proceso->instrucciones, pc);
        log_info(logger, "## PID: <PID> - Obtener instrucción: <%d> - Instrucción: <%s>", pc, instruccion);
        t_paquete* paquete = crear_paquete(KM_CPU__INSTRUCCION);
        agregar_string_a_paquete(paquete, instruccion);
        pthread_mutex_unlock(&proceso->mutex);
        enviar_paquete_y_liberarlo(paquete, cpu->fd);
    }else{
        log_error(logger, "Atender_cpu de id <%d>, Error: pid <%d> no encontrado", cpu->id, pid);
    }

    list_destroy_and_destroy_elements(data, free);
}

void atender_cpu_copy_mem(t_list* data, int cpu_fd){
    int i = 0;
    int pid = *(int*)list_get(data, i++);
    t_dir_fisica dir_fisica_origen = *(t_dir_fisica*)list_get(data, i++);
    t_dir_fisica dir_fisica_destino = *(t_dir_fisica*)list_get(data, i++);

    int tamanio = *(int*)list_get(data, i++);
    
    int base_origen = calc_dir_fisica(pid, dir_fisica_origen.id_segmento, dir_fisica_origen.offset);
    int base_destino = calc_dir_fisica(pid, dir_fisica_destino.id_segmento, dir_fisica_destino.offset);

    t_evt* evt_copy_mem = iniciar_evt_mover(pid, base_origen, tamanio, base_destino);

    agregar_evt_a_stick(evt_copy_mem);

    sem_wait(&evt_copy_mem->s_fin);
    
    log_info(logger, "## PID: <%d> - <COPY_MEM> - Dir. Física origen: <%d> - Dir. Física destino: <%d> - Tamaño: <%d>", pid, base_origen, base_destino, tamanio);

    liberar_evt(evt_copy_mem);

    esperar_ms(instruction_delay_ms);

    enviar_operacion(cpu_fd, KM_CPU__RESPUESTA);
}

void atender_cpu_mov_in(t_list* data, int cpu_fd){
    int i = 0;
    t_dir_fisica dir_fisica = *(t_dir_fisica*)list_get(data, i++);
    int tamanio = *(int*)list_get(data, i++);
    int pid = *(int*)list_get(data, i++);

    int base = calc_dir_fisica(pid, dir_fisica.id_segmento, dir_fisica.offset);

    t_evt* evt_read = iniciar_evt_read(pid, base, tamanio, cpu_fd);
    t_data_read* data_read = (t_data_read*)evt_read->data;

    agregar_evt_a_stick(evt_read);

    sem_wait(&evt_read->s_fin);

    log_info(logger, "## PID: <%d> - <Lectura> - Dir. Física: <%d> - Tamaño: <%d>", pid, base, tamanio);

    esperar_ms(instruction_delay_ms);

    void* datos_leidos = malloc(tamanio);
    if (datos_leidos == NULL) {
        liberar_evt(evt_read);
        return;
    }

    memcpy(datos_leidos, data_read->datos_leidos, tamanio);

    liberar_evt(evt_read);
    t_paquete* paquete = crear_paquete(KM_CPU__RESPUESTA);
    agregar_a_paquete(paquete, datos_leidos, tamanio);
    enviar_paquete_y_liberarlo(paquete, cpu_fd);

    free(datos_leidos);
}

void atender_cpu_mov_out(t_list* data, int cpu_fd){
    int i = 0;
    t_dir_fisica dir_fisica = *(t_dir_fisica*)list_get(data, i++);
    int tamanio = *(int*)list_get(data, i++);
    void* datos_escritos = list_get(data, i++);
    log_debug(logger, "Por escribir con mov_out, tamanio: %d", tamanio);
    imprimir_bytes(datos_escritos, tamanio);
    int pid = *(int*)list_get(data, i++);
    int base = calc_dir_fisica(pid, dir_fisica.id_segmento, dir_fisica.offset);
    log_debug(logger, "calcule dir fisica");
    t_evt* evt_write = iniciar_evt_write(pid, base, tamanio, datos_escritos, cpu_fd);

    agregar_evt_a_stick(evt_write);

    sem_wait(&evt_write->s_fin);
    
    log_info(logger, "## PID: <%d> - <Escritura> - Dir. Física: <%d> - Tamaño: <%d>", pid, base, tamanio);

    liberar_evt(evt_write);

    esperar_ms(instruction_delay_ms);

    enviar_operacion(cpu_fd, KM_CPU__RESPUESTA);
}

void* atender_scheduler(void*){
    while(1){
        op_code cod_op = recibir_operacion(sch_fd);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto scheduler");
            // desconectar todo
            exit(EXIT_FAILURE);
        }
        log_debug(logger, "LLEGO OP de SCH=%d", cod_op);
        switch(cod_op){
            case SCH_KM__INIT_PROC:{
                t_list* lista_paquete = recibir_paquete(sch_fd);
                atender_sch_init_proc(lista_paquete);
                list_destroy_and_destroy_elements(lista_paquete, free);
                break;
            }
            case KM_READ:{
                t_list* data = recibir_paquete(sch_fd);
                atender_sch_read(data);
                list_destroy_and_destroy_elements(data, free);
                break;
            }
            case KM_WRITE:{
                t_list* data = recibir_paquete(sch_fd);
                atender_sch_write(data);
                list_destroy_and_destroy_elements(data, free);
                break;
            }
            case SCH_KM__MEM_ALLOC:{
                t_list* data = recibir_paquete(sch_fd);
                atender_sch_mem_alloc(data);
                list_destroy_and_destroy_elements(data, free);
                break;
            }
            case SCH_KM__MEM_FREE:{
                t_list* data = recibir_paquete(sch_fd);
                atender_sch_mem_free(data);
                list_destroy_and_destroy_elements(data, free);
                break;
            }
            case SCH_KM__COMENZAR_COMPACTACION:{
                log_info(logger, "Iniciando compactacion");
                bool ok = compactar_memoria();
                log_info(logger, "Compactacion completa");
                imprimir_estado_mem_thread_safe();
                if(!ok){
                    log_error(logger, "No se pudo compactar la memoria");
                    exit(EXIT_FAILURE);
                }
                enviar_operacion(sch_fd, KM_SCH__COMPACTACION_COMPLETA);
                break;
            }
            case SCH_KM__EXIT:{
                t_list* data = recibir_paquete(sch_fd);
                int pid = *(int*)list_get(data, 0);
                log_debug(logger, "Exit de pid %d en progreso", pid);
                pthread_mutex_lock(&m_lista_procesos);
                t_proceso* proceso = proceso_de_pid(pid);
                if (proceso == NULL) {
                    pthread_mutex_unlock(&m_lista_procesos);
                    break;
                }
                list_remove_element(lista_procesos, proceso);
                pthread_mutex_unlock(&m_lista_procesos);
                destruir_proceso(proceso);
                log_debug(logger, "Exit de pid %d completo", pid);
                enviar_operacion(sch_fd, KM_SCH__EXIT_OK);
                list_destroy_and_destroy_elements(data, free);
                free(proceso);
                break;
            }
            case SCH_KM__SUSPENDER:{
                t_list* data = recibir_paquete(sch_fd);
                atender_sch_suspender(data);
                list_destroy_and_destroy_elements(data, free);
                break;
            }
            case SCH_KM__INTENTAR_DESUSPENDER:{
                t_list* data = recibir_paquete(sch_fd);
                atender_sch_intentar_desuspender(data);
                list_destroy_and_destroy_elements(data, free);
                break;
            }
            default:
                log_warning(logger, "atender_scheduler: Operacion desconocida, cod_op=%d", cod_op);
        }
    }
    return NULL;
}

void atender_sch_intentar_desuspender(t_list* data){
    int pid = *(int*)list_get(data, 0);
    bool d = intentar_desuspender(pid);
    if(d){
        t_paquete* rta = crear_paquete(KM_SCH__DESUSPENDIDO);
        agregar_a_paquete(rta, &pid, sizeof(pid));
        enviar_paquete_y_liberarlo(rta, sch_fd);
    }
}

void atender_sch_suspender(t_list* data){
    int pid = *(int*)list_get(data, 0);
    log_info(logger, "## <%d> Suspendiendo proceso", pid);
    /*bool suspendido = */suspender_thread_safe(pid);
    enviar_operacion(sch_fd, KM_SCH__SUSPENDIDO);
}

void atender_sch_read(t_list* data){
    t_dir_fisica dir_fisica = *(t_dir_fisica*)list_get(data, 0);
    int tamanio = *(int*) list_get(data, 1);
    int pid =*(int*)  list_get(data, 2);
    int dir_fisica_base = calc_dir_fisica(pid, dir_fisica.id_segmento, dir_fisica.offset);
    log_debug(logger, "KM_WRITE | pid=%d | dir_fisica=%d | tamanio=%d", pid, dir_fisica_base, tamanio);
    
    t_evt* evt_read = iniciar_evt_read(pid, dir_fisica_base, tamanio, sch_fd);

    agregar_evt_a_stick(evt_read);

    sem_wait(&evt_read->s_fin);

    log_info(logger, "## PID: <%d> - <Lectura> - Dir. Física: <%d> - Tamaño: <%d>", pid, dir_fisica_base, tamanio);

    t_data_read* d = (t_data_read*) evt_read->data;
    void* datos_leidos = d->datos_leidos;

    esperar_ms(instruction_delay_ms);

    t_paquete* paquete = crear_paquete(RTA_READ);
    agregar_a_paquete(paquete, &pid, sizeof(pid));
    agregar_string_a_paquete(paquete, datos_leidos);
    enviar_paquete_y_liberarlo(paquete, sch_fd);
    imprimir_estado_mem_thread_safe();
    liberar_evt(evt_read);
}

void atender_sch_write(t_list* data){
    int i = 0;
    t_dir_fisica dir_fisica = *(t_dir_fisica*)list_get(data, i++);
    int tamanio = *(int*) list_get(data, i++);
    void* datos_escritos = list_get(data, i++);
    int pid =*(int*) list_get(data, i++);
    
    if(proceso_suspendido_thread_safe(pid)){
        log_debug(logger, "Proceso suspendido, desuspendiendo");
        t_evt* evt_desuspender = iniciar_evt_suspender(pid);
        evt_desuspender->tipo = DESUSPENSION;
        agregar_evt_a_stick(evt_desuspender);
        sem_wait(&evt_desuspender->s_fin);
        liberar_evt(evt_desuspender);
    }

    int base = calc_dir_fisica(pid, dir_fisica.id_segmento, dir_fisica.offset);

    log_debug(logger, "KM_WRITE | pid=%d | base=%d | tamanio=%d", pid, base, tamanio);
    
    imprimir_bytes(datos_escritos,tamanio);

    t_evt* evt_write = iniciar_evt_write(pid, base, tamanio, datos_escritos, sch_fd);

    agregar_evt_a_stick(evt_write);
    sem_wait(&evt_write->s_fin);

    log_info(logger, "## PID: <%d> - <Escritura> - Dir. Física: <%d> - Tamaño: <%d>", pid, base, tamanio);

    esperar_ms(instruction_delay_ms);

    liberar_evt(evt_write);
    t_paquete* paquete = crear_paquete(RTA_WRITE);
    agregar_a_paquete(paquete, &pid, sizeof(pid));
    enviar_paquete_y_liberarlo(paquete, sch_fd);

}

void atender_sch_init_proc(t_list* data){
    int i = 0;
    int pid = *(int*)list_get(data, i++);
    int ppid = *(int*)list_get(data, i++);

    char* ruta_instrucciones = (char*)list_get(data, i++);
    bool ok = guardar_nuevo_proceso(pid, ppid, ruta_instrucciones);

    esperar_ms(instruction_delay_ms);

    t_paquete* paquete_conf = crear_paquete(KM_SCH__INIT_PROC_RESP);
    agregar_a_paquete(paquete_conf, &pid, sizeof(int));
    agregar_a_paquete(paquete_conf, &ppid, sizeof(int));
    agregar_a_paquete(paquete_conf, &ok, sizeof(bool));
    enviar_paquete_y_liberarlo(paquete_conf, sch_fd);
}

void atender_sch_mem_alloc(t_list* data){
    int i = 0;
    int pid = *(int*)list_get(data, i++);
    int id_segmento = *(int*)list_get(data, i++);
    int tamanio = *(int*)list_get(data, i++);
    t_proceso* p = proceso_de_pid_thread_safe(pid);
    t_status_op status = OK;
    if(p == NULL){
        log_debug(logger, "Proceso NULL");
        status = ERROR;
    }else{
        log_info(logger, "## <%d> Atendiendo syscall MEM_ALLOC %d %d", pid, id_segmento, tamanio);
        if(existe_segmento_de_id_de_proc_thread_safe(id_segmento, p)){
            status = RECURSO_YA_EXISTE;
        }else{
            t_segmento* segmento = crear_segmento_thread_safe(p, id_segmento, tamanio);
            if(segmento == NULL){
                status = MEM_INSUFICIENTE;
                if(hay_espacio_total_thread_safe(tamanio)){
                    log_info(logger, "## <%d> Se dispara compactación tras intentar MEM_ALLOC, tamanio libre: %d, tamanio a reservar: %d", pid, tamanio_total_libre, tamanio);
                    
                    esperar_ms(instruction_delay_ms);

                    enviar_operacion(sch_fd, KM_SCH__PEDIDO_COMPACTACION);
                    return;
                }
            }
        }
    }

    esperar_ms(instruction_delay_ms);

    t_paquete* paquete = crear_paquete(KM_SCH__RTA_MEM_ALLOC);
    agregar_a_paquete(paquete, &pid, sizeof(pid));
    agregar_a_paquete(paquete, &status, sizeof(status));

    enviar_paquete_y_liberarlo(paquete, sch_fd);
}

void atender_sch_mem_free(t_list* data){
    int i = 0;
    int pid = *(int*)list_get(data, i++);
    int id_segmento = *(int*)list_get(data, i++);
    
    t_proceso* p = proceso_de_pid_thread_safe(pid);
    t_segmento* segmento = segmento_de_id(id_segmento, pid);
    t_status_op status = OK;
    if(p == NULL || segmento == NULL){
        log_debug(logger, "Proceso o segmento no encontrados");
        status = ERROR;
    }else{
        log_info(logger, "## <%d> Atendiendo syscall MEM_FREE %d", pid, id_segmento);
        eliminar_segmento_thread_safe(segmento, p);
        imprimir_estado_mem_thread_safe();
    }
    
    esperar_ms(instruction_delay_ms);

    t_paquete* paquete = crear_paquete(KM_SCH__RTA_MEM_FREE);
    agregar_a_paquete(paquete, &pid, sizeof(pid));
    agregar_a_paquete(paquete, &status, sizeof(status));

    enviar_paquete_y_liberarlo(paquete, sch_fd);
}