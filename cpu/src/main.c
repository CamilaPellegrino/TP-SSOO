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

    atender_scheduler();
    
    // liberar logger, config y conexiones
    log_destroy(logger);
    config_destroy(config);
    close(conexion_kernel_scheduler);
    close(conexion_kernel_memory);
    return 0;
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
