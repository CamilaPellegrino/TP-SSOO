#include "main.h"



int main(int argc, char* argv[]){
    // ejemplo para ejecutar: ./bin/cpu ./cpu.config 0
    if(argc < 3){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/cpu ./cpu.config 0");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];
    id_cpu = atoi(argv[2]);

    saludar("cpu");
    
    t_config* config;
    char *puerto_kernel_scheduler;
    char *puerto_kernel_memory;

    // iniciar config
    config = iniciar_config(ruta_config);

    // crear logger
    t_log_level log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));
    char ruta[32];
    snprintf(ruta, sizeof(ruta), "cpu_%d.log", id_cpu);

    logger = iniciar_logger(ruta, "ProcesoCPU", log_level);

    char*ip_sch = config_get_string_value(config, "IP_SCH");
    char*ip_km = config_get_string_value(config, "IP_KM");
    
    // inicializar variables
    inicializar_variables();

    puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    puerto_kernel_scheduler = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");

    // conectar a kernel scheduler
    conexion_kernel_scheduler = crear_conexion(ip_sch, puerto_kernel_scheduler);
    exit_si_error_conexion(conexion_kernel_scheduler, logger, "kernel scheduler");
    handshake_cliente(conexion_kernel_scheduler,logger);
    
    // enviar info propia a sch:
    t_paquete *paquete_conexion_sch = crear_paquete(CPU_SCH__CONEXION);
    agregar_a_paquete(paquete_conexion_sch, &id_cpu, sizeof(id_cpu));
    enviar_paquete_y_liberarlo(paquete_conexion_sch, conexion_kernel_scheduler);

    // conectar a kernel memory
    conexion_kernel_memory = crear_conexion(ip_km, puerto_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    handshake_cliente(conexion_kernel_memory,logger);
        
    //mando informacion propia al km
    t_paquete *paquete_conexion_km = crear_paquete(CPU_KM__CONEXION);
    agregar_a_paquete(paquete_conexion_km, &id_cpu, sizeof(id_cpu));
    enviar_paquete_y_liberarlo(paquete_conexion_km, conexion_kernel_memory);

    recibir_sticks_de_km();
    
    // hilo que ejecuta instrucciones:
    pthread_t hilo_ejecucion;
    pthread_create(&hilo_ejecucion, NULL, ejecutar, NULL);
    pthread_detach(hilo_ejecucion);

    // queda escuchando mensajes que envie el scheduler
    while (1){
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
        if(cod_op == -1){
            log_error(logger, "Error: se desconecto scheduler"); 
            break; 
        }
        if(cod_op == SCH_CPU__PID){
            log_debug(logger, "Recibiendo pid");
            t_list* data = recibir_paquete(conexion_kernel_scheduler);
            pthread_mutex_lock(&m_pid_pendiente);
            int prox_pid = *(int*)list_get(data, 0);
            pid_pendiente = prox_pid;
            pthread_mutex_unlock(&m_pid_pendiente);
            list_destroy_and_destroy_elements(data, free);
            
        } 
        switch(cod_op){
            case SCH_CPU__NUEVO_STICK: {
                log_debug(logger, "Conexion de modulo stick");
                t_list * lista_del_paquete = recibir_paquete(conexion_kernel_scheduler);
                
                char* ip_stick = list_get(lista_del_paquete, 0);
                char* puerto_stick = list_get(lista_del_paquete, 1);
                int* tamanio = list_get(lista_del_paquete, 2); 
     
                // conectar a memory stick
                int conexion_memory_stick = crear_conexion(ip_stick, puerto_stick);
                exit_si_error_conexion(conexion_memory_stick, logger, "memory stick");
                handshake_cliente(conexion_memory_stick,logger);
                
                conectarse_a_stick(ip_stick, puerto_stick, *tamanio);
                // liberar lista_del_paquete
                list_destroy_and_destroy_elements(lista_del_paquete, free);
                break;
            }
            default:{
                procesar_evento(cod_op);
                break;
            }
        }
    }
    
    // liberar logger, config y conexiones
    log_destroy(logger);
    config_destroy(config);
    close(conexion_kernel_scheduler);
    close(conexion_kernel_memory);
    return 0;
}

// ===================== Atender al scheduler( maquina de estados y pedidos de desalojo) =====================

void procesar_evento(op_code cod_op){
    pthread_mutex_lock(&m_estado_cpu);  
    switch(estado_cpu){
        case EXEC:{
            transicion_desde_exec(cod_op);
            break;
        }
        case WAIT_SYS:{
            transicion_desde_wait_sys(cod_op);
            break;
        }
        case WAIT_SYS_Y_PROX_DESALOJO:{
            transicion_desde_wait_sys_y_prox_desalojo(cod_op);
            break;
        }
        case LIBRE:{
            transicion_desde_libre(cod_op);
            break;
        }
        case WAIT_MEM_ALLOC:{
            transicion_desde_wait_mem_alloc(cod_op);
            break;
        }
        case WAIT_MEM_ALLOC_Y_PROX_DESALOJO:{
            transicion_desde_wait_mem_alloc_y_prox_desalojo(cod_op);
            break;
        }
    }
    pthread_mutex_unlock(&m_estado_cpu);
}

void transicion_desde_exec(op_code cod_op){
    switch(cod_op){
        case SCH_CPU__ERROR_SYSCALL:
        case SCH_CPU__COMPACTACION:
        case SCH_CPU__PEDIDO_DESALOJO:{
            v_pedido_de_desalojo = true;
            break;
        }
        default:{}
    }
}

void transicion_desde_wait_sys(op_code cod_op){
    switch(cod_op){
        case SCH_CPU__COMPACTACION:
        case SCH_CPU__PEDIDO_DESALOJO:{ 
            transicionar(WAIT_SYS_Y_PROX_DESALOJO);
            break;
        }
        case SCH_CPU__FIN_SYSCALL:{ 
            transicionar(EXEC);
            v_pedido_de_desalojo = false;
            pthread_cond_signal(&cond_ejecutar);
            break;
        }
        case SCH_CPU__SYS_BLOQUEANTE:
            v_pedido_de_desalojo = true;
            pthread_cond_signal(&cond_ejecutar);
        case SCH_CPU__ERROR_SYSCALL: {
            transicionar(LIBRE);
            break;
        }
        default:{}
    }
}

void transicion_desde_wait_sys_y_prox_desalojo(op_code cod_op){
    switch(cod_op){
        case SCH_CPU__SYS_BLOQUEANTE:
        case SCH_CPU__FIN_SYSCALL:{ 
            // transicionar(LIBRE);
            // enviar_confirmacion();
            transicionar(LIBRE);
            v_pedido_de_desalojo = true;
            pthread_cond_signal(&cond_ejecutar);
            break;
        }
        case SCH_CPU__ERROR_SYSCALL: {
            transicionar(LIBRE);
            break;
        }
        default:{}
    }
}

void transicion_desde_libre(op_code cod_op){
    switch(cod_op){
        case SCH_CPU__PID:{
            transicionar(EXEC);
            v_pedido_de_desalojo = false;
            pthread_cond_signal(&cond_ejecutar);
            break;
        }
        case SCH_CPU__PEDIDO_DESALOJO:{
            enviar_confirmacion();
            break;
        }
        case SCH_CPU__ERROR_SYSCALL: {
            transicionar(LIBRE);
            break;
        }
        default:{}
    }
}

void transicion_desde_wait_mem_alloc(op_code cod_op){
    switch(cod_op){
        case SCH_CPU__PEDIDO_DESALOJO:{
            transicionar(WAIT_MEM_ALLOC_Y_PROX_DESALOJO);
            break;
        }
        case SCH_CPU__FIN_SYSCALL:{
            transicionar(EXEC);
            pthread_cond_signal(&cond_ejecutar);
            break;
        }
        case SCH_CPU__COMPACTACION:{
            transicionar(LIBRE);
            enviar_confirmacion();
            break;
        }
        case SCH_CPU__ERROR_SYSCALL: {
            transicionar(LIBRE);
            break;
        }
        default:{}
    }
}

void transicion_desde_wait_mem_alloc_y_prox_desalojo(op_code cod_op){
    switch(cod_op){
        case SCH_CPU__COMPACTACION:
            transicionar(LIBRE);
            enviar_confirmacion();
            break;
        case SCH_CPU__FIN_SYSCALL:{
            // transicionar(LIBRE);
            // enviar_confirmacion();
            v_pedido_de_desalojo = true;
            pthread_cond_signal(&cond_ejecutar);
            break;
        }
        case SCH_CPU__ERROR_SYSCALL: {
            transicionar(LIBRE);
            break;
        }
        default:{}
    }
}


void enviar_confirmacion(){
    set_pedir_segmentos(false);
    enviar_operacion(conexion_kernel_scheduler, CPU_SCH__EJECUCION_DETENIDA);
}



void procesar_desalojo_pendiente(t_pcb* pcb){
    log_debug(logger, "Pedido de desalojo detectado, pasando a LIBRE");
    transicionar(LIBRE);
    enviar_pcb_actualizado_a_km(pcb);
    v_pedido_de_desalojo = false;
    enviar_confirmacion();
}

void esperar_a_poder_ejecutar(t_pcb* pcb){
    pthread_mutex_lock(&m_estado_cpu);
    while(1){
        if(v_pedido_de_desalojo){
            log_info(logger, "## Interrupción recibida");
            procesar_desalojo_pendiente(pcb);
        }
        if(estado_cpu == EXEC){
            break;
        }
    pthread_cond_wait(&cond_ejecutar, &m_estado_cpu);
    }
    pthread_mutex_unlock(&m_estado_cpu);
}




// ===================== Conexion con sticks =====================
void conectarse_a_stick(char* ip, char* puerto, uint32_t tamanio){
    // conectar a memory stick
    int conexion_memory_stick = crear_conexion(ip, puerto);
    exit_si_error_conexion(conexion_memory_stick, logger, "memory stick");

    handshake_cliente(conexion_memory_stick, logger);

    log_info(logger, "Conectado a stick -> ip: %s, puerto: %s", ip, puerto);

    // crear stick
    t_stick* stick = iniciar_stick(ip, puerto, tamanio, conexion_memory_stick);
    t_paquete *data = crear_paquete(CPU_STICK__CONEXION);
    agregar_a_paquete(data, &id_cpu, sizeof(id_cpu));
    enviar_paquete(data, conexion_memory_stick);
    // agregar a lista local
    list_add(lista_sticks, stick);

    pthread_t p = crear_hilo_o_exit(atender_stick, stick, "atender_stick", logger);
    pthread_detach(p);
}

void* atender_stick(void* arg){
    t_stick* stick = (t_stick*)arg;
    while(1){
        op_code cod_op = recibir_operacion(stick->fd);
        if(cod_op == -1){
            log_debug(logger, "BSOD");
            enviar_operacion(conexion_kernel_memory, CPU_KM__BSOD);
            break;
        }
    }
    return NULL;
}

void recibir_sticks_de_km(){ // + seg_max_size
    op_code cod_op = recibir_operacion(conexion_kernel_memory);
    if(cod_op != KM_CPU__STICKS){
        log_error(logger, "Error: Kernel Memory no envio las sticks");
        exit(EXIT_FAILURE);
    }
    t_list* paquete = recibir_paquete(conexion_kernel_memory);
    seg_max_size_global = *(int*)list_get(paquete, 0);
    log_debug(logger, "seg_max_size_global: %d", seg_max_size_global);
    int desplazamiento = 1;
    int cantidad;
    memcpy(&cantidad, list_get(paquete, desplazamiento++), sizeof(int));
    for(int i = 0; i < cantidad; i++){
        int tamanio;
        char* puerto;
        char* ip;
        memcpy(&tamanio, list_get(paquete, desplazamiento++), sizeof(int));
        puerto = list_get(paquete, desplazamiento++);
        ip = list_get(paquete, desplazamiento++);

        conectarse_a_stick(ip, puerto, tamanio);
    }
    
    list_destroy_and_destroy_elements(paquete, free);
}




// ===================== Conexion con KM =====================
void pedir_contexto(int pid, t_pcb* pcb){
    t_paquete *paquete = crear_paquete(CPU_KM__PCONTEXTO);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    
    op_code cod_op = recibir_operacion(conexion_kernel_memory);

    if(cod_op != KM_CPU__RTA_CONTEXTO) {
        printf("ERROR CONTEXTO\n");
        exit(EXIT_FAILURE);
    }
    t_list* lista_contexto = recibir_paquete(conexion_kernel_memory);

    int i = 0;
    pcb->pid = *(int*) list_get(lista_contexto, i++);
    pcb->ppid = *(int*)list_get(lista_contexto, i++);
    pcb->priodidad = *(int*)list_get(lista_contexto, i++);

    pcb->registros.pc = *(uint32_t*) list_get(lista_contexto, i++);

    pcb->registros.ax = *(uint8_t*) list_get(lista_contexto, i++);
    pcb->registros.bx = *(uint8_t*) list_get(lista_contexto, i++);
    pcb->registros.cx = *(uint8_t*) list_get(lista_contexto, i++);
    pcb->registros.dx = *(uint8_t*) list_get(lista_contexto, i++);

    pcb->registros.eax = *(uint32_t*) list_get(lista_contexto, i++);
    pcb->registros.ebx = *(uint32_t*) list_get(lista_contexto, i++);
    pcb->registros.ecx = *(uint32_t*) list_get(lista_contexto, i++);
    pcb->registros.edx = *(uint32_t*) list_get(lista_contexto, i++);

    pcb->registros.si = *(uint32_t*) list_get(lista_contexto, i++);
    pcb->registros.di = *(uint32_t*) list_get(lista_contexto, i++);
    
    int cantidad_segmentos = *(int*) list_get(lista_contexto, i++);

    list_clean_and_destroy_elements(pcb->tabla_segmentos, free);
    for(int j = 0; j < cantidad_segmentos; j++) {
        t_segmento* seg_recibido = malloc(sizeof(t_segmento));
        memcpy(seg_recibido, list_get(lista_contexto, i++), sizeof(t_segmento));
        log_debug(logger, "(%d) Recibiendo segmento de id: %d | base: %d | tamanio: %d", seg_recibido->pid, seg_recibido->id_segmento, seg_recibido->base, seg_recibido->tamanio);
        list_add(pcb->tabla_segmentos, seg_recibido);
    }
    
    list_destroy_and_destroy_elements(lista_contexto, free);
}

void pedir_segmentos(int pid, t_pcb* pcb){
    t_paquete *paquete = crear_paquete(CPU_KM__PSEGMENTOS);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    op_code cod_op = recibir_operacion(conexion_kernel_memory);

    if(cod_op != KM_CPU__RTA_CONTEXTO) {
        printf("ERROR CONTEXTO\n");
        exit(EXIT_FAILURE);
    }
    t_list* lista_contexto = recibir_paquete(conexion_kernel_memory);
    int i = 0;
    int cantidad_segmentos = *(int*) list_get(lista_contexto,i++);

    list_clean_and_destroy_elements(pcb->tabla_segmentos, free);
    for(int j = 0; j < cantidad_segmentos; j++) {
        t_segmento* seg_recibido = malloc(sizeof(t_segmento));
        memcpy(seg_recibido, list_get(lista_contexto, i++), sizeof(t_segmento));
        log_debug(logger, "(%d) Recibiendo segmento de id: %d | base: %d | tamanio: %d", seg_recibido->pid, seg_recibido->id_segmento, seg_recibido->base, seg_recibido->tamanio);
        list_add(pcb->tabla_segmentos, seg_recibido);
    }
    list_destroy_and_destroy_elements(lista_contexto, free);
}


void* ejecutar(){
    t_pcb* pcb = malloc(sizeof(t_pcb));
    pcb->tabla_segmentos = list_create();
    pcb->pid = -1;
    while(1){
        esperar_a_poder_ejecutar(pcb); 
        log_debug(logger, "ya puedo ejecutar");
        pthread_mutex_lock(&m_pid_pendiente);

        if(pid_pendiente >= 0){
            pedir_contexto(pid_pendiente, pcb);
            log_debug(logger, "ejecutar: pcb de pid %d cargado", pid_pendiente);
            pid_pendiente = -1;
        }else if(get_pedir_segmentos() && pcb->pid >= 0){
            log_debug(logger, "Pidiendo segmentos actualizados");
            pedir_segmentos(pcb->pid, pcb);
            set_pedir_segmentos(false);
        }
        pthread_mutex_unlock(&m_pid_pendiente);

        ciclo_instruccion(pcb);
    }
    return NULL;
}
