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
            log_info(logger, "Me llego el scheduler, mensaje recibido: %s", msg);
            pthread_t thread_sch = crear_hilo_o_exit(atender_scheduler, NULL, "atender_scheduler", logger);
            pthread_detach(thread_sch);
            free(msg);
            break;
        }case STICK_KM__CONEXION: {
            t_list *lista_paquete = recibir_paquete(cliente_fd);

            int tamanio = *(int*)list_get(lista_paquete, 0);
            char *puerto = list_get(lista_paquete, 1);
            char *ip = list_get(lista_paquete, 2);
            log_info(logger, "Me llego un stick: %d, %s, %s", tamanio, puerto, ip);
            
            t_stick* nuevo_stick = iniciar_stick(ip, puerto, tamanio, cliente_fd);
            
            agregar_espacio_mem(nuevo_stick->tamanio);
            agregar_stick(nuevo_stick);

            enviar_nuevo_stick_a_scheduler(nuevo_stick, sch_fd);
            
            pthread_t thread_stick = crear_hilo_o_exit(atender_stick, nuevo_stick, "atender_stick", logger);
            pthread_detach(thread_stick);

            list_destroy_and_destroy_elements(lista_paquete, free);
            break;
        }case SWAP_KM__CONEXION:{
            log_warning(logger, "Conexion SWAP no atendida");
            // atender_swap(cliente_fd);
            break;
        }case CPU_KM__CONEXION:{
            t_list *lista_paquete = recibir_paquete(cliente_fd);
            int *cpu_id = list_get(lista_paquete, 0);

            // agregar CPU a la lista de cpus
            t_cpu* cpu = iniciar_cpu(*cpu_id, cliente_fd);
            list_add(lista_cpus, cpu);

            // mandarle todos los sticks que se conectaron hasta ahora
            enviar_sticks_a_cpu(cliente_fd);
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
            log_warning(logger, "se desconecto CPU");
            break;
        }
        switch(cod_op) {
            case CPU_KM__PCONTEXTO:{
                //tiene que recibir pid, puede mandar todo el pcb
                t_list *lista = recibir_paquete(cpu_fd);
                int pid = *(int*)list_get(lista, 0);
            
                t_paquete* paquete = crear_paquete(KM_CPU__RTA_CONTEXTO);
                t_proceso* proceso_actual = proceso_de_pid(pid);

                if(proceso_actual != NULL){
                    log_info(logger, "Enviando contexto de proceso <%d> a cpu <%d>", pid, cpu_id);
                    agregar_pcb_al_paquete(proceso_actual->pcb, paquete);
                    enviar_paquete_y_liberarlo(paquete, cpu_fd);
                }else{
                    log_error(logger, "Atender_cpu de id <%d>, Error: pid <%d> no encontrado", cpu_id, pid);
                }
                list_destroy_and_destroy_elements(lista, free);
                break;
            }case CPU_KM__FETCH: {
                t_list* lista = recibir_paquete(cpu_fd);
                int i = 0;
                int pid = *(int*) list_get(lista, i++);
                uint32_t pc = *(uint32_t*) list_get(lista, i++);
                t_proceso* proceso = proceso_de_pid(pid);
                if(proceso != NULL){
                    log_info(logger, "FETCH recibido PID: %d PC: %u", pid, pc);
                    char* instruccion = list_get(proceso->instrucciones, pc);
                    t_paquete* paquete = crear_paquete(KM_CPU__INSTRUCCION);

                    agregar_string_a_paquete( paquete, instruccion);
                    enviar_paquete_y_liberarlo( paquete, cpu_fd);
                }else{
                    log_error(logger, "Atender_cpu de id <%d>, Error: pid <%d> no encontrado", cpu_id, pid);
                }
                
                list_destroy_and_destroy_elements(lista, free);
                break;
            }case CPU_KM__ACTUALIZAR_PCB:
                t_list* p = recibir_paquete(cpu_fd);
                recibir_pcb_actualizado(p);
                break;
            default:{
                log_warning(logger,"atender_cpu: op desconocida, op=%d", cod_op);
            }

        }
    }
    close(cpu_fd);
    log_info(logger, "cerrando hilo de CPU");
    return NULL;
}

void* atender_scheduler(void*){
    log_info(logger, "## Kernel Scheduler Conectado - FD del socket: %d", sch_fd);
    while(1){
        op_code cod_op = recibir_operacion(sch_fd);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto scheduler");
            // desconectar todo
            exit(EXIT_FAILURE);
        }
        switch(cod_op){
            case SCH_KM__INIT_PROC:{
                t_list* lista_paquete = recibir_paquete(sch_fd);
                atender_sch_init_proc(lista_paquete);
                break;
            }case KM_READ:{
                t_list* data = recibir_paquete(sch_fd);
                atender_sch_read(data);
                break;
            }case KM_WRITE:{
                t_list* data = recibir_paquete(sch_fd);
                atender_sch_write(data);
                break;
            }case SCH_KM__MEM_ALLOC:{
                t_list* data = recibir_paquete(sch_fd);
                atender_sch_mem_alloc(data);
                break;
            }
            case SCH_KM__MEM_FREE:{
                t_list* data = recibir_paquete(sch_fd);
                atender_sch_mem_free(data);
                break;
            }
            default:
                log_warning(logger, "atender_scheduler: Operacion desconocida, cod_op=%d", cod_op);
        }
    }
    return NULL;
}

void atender_sch_read(t_list* data){
    int dir_fisica_base = *(int*) list_get(data, 0);
    int tamanio = *(int*) list_get(data, 1);
    int nro_stick = *(int*) list_get(data, 2);
    int pid =*(int*)  list_get(data, 3);
    
    log_debug(logger, "KM_WRITE | pid=%d | dir_fisica=%d | stick=%d | tamanio=%d", pid, dir_fisica_base, nro_stick, tamanio);
    
    t_evt* evt_read = iniciar_evt_read(pid, dir_fisica_base, tamanio);

    agregar_evt_a_stick(nro_stick, evt_read);
}

void atender_sch_write(t_list* data){
    int i = 0;
    int dir_fisica = *(int*) list_get(data, i++);
    int nro_stick = *(int*) list_get(data, i++);
    int tamanio = *(int*) list_get(data, i++);
    void* datos_escritos = list_get(data, i++);
    int pid =*(int*) list_get(data, i++);
    
    int datos_hardcodeado = 0;

    log_debug(logger, "KM_WRITE | pid=%d | dir_fisica=%d | stick=%d | tamanio=%d", pid, dir_fisica, nro_stick, tamanio);
    
    imprimir_bytes(datos_escritos,tamanio);

    t_evt* evt_write = iniciar_evt_write(pid, dir_fisica, tamanio, datos_escritos);

    agregar_evt_a_stick(nro_stick, evt_write);
}

void atender_sch_init_proc(t_list* data){
    int i = 0;
    int pid = *(int*)list_get(data, i++);
    int ppid = *(int*)list_get(data, i++);

    char* ruta_instrucciones = (char*)list_get(data, i++);
    bool ok = guardar_nuevo_proceso(pid, ppid, ruta_instrucciones);
    list_destroy_and_destroy_elements(data, free);


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
    t_proceso* p = proceso_de_pid(pid);
    t_status_op status = OK;
    if(p == NULL){
        log_debug(logger, "Proceso NULL");
        status = ERROR;
    }else{
        log_info(logger, "## <%d> Atendiendo syscall MEM_ALLOC %d %d", pid, id_segmento, tamanio);
        t_segmento* segmento = crear_segmento(p, id_segmento, tamanio);
        if(segmento == NULL){
            status = ERROR;
        }
    }
    t_paquete* paquete = crear_paquete(KM_SCH__RTA_MEM_ALLOC);
    agregar_a_paquete(paquete, &pid, sizeof(pid));
    agregar_a_paquete(paquete, &status, sizeof(status));

    enviar_paquete_y_liberarlo(paquete, sch_fd);
}

void atender_sch_mem_free(t_list* data){
    int i = 0;
    int pid = *(int*)list_get(data, i++);
    int id_segmento = *(int*)list_get(data, i++);
    
    t_proceso* p = proceso_de_pid(pid);
    t_segmento* segmento = segmento_de_id(id_segmento);
    t_status_op status = OK;
    if(p == NULL || segmento == NULL){
        log_debug(logger, "Proceso o segmento no encontrados");
        status = ERROR;
    }else{
        log_info(logger, "## <%d> Atendiendo syscall MEM_FREE %d", pid, id_segmento);
        eliminar_segmento(segmento, p);
    }
    t_paquete* paquete = crear_paquete(KM_SCH__RTA_MEM_FREE);
    agregar_a_paquete(paquete, &pid, sizeof(pid));
    agregar_a_paquete(paquete, &status, sizeof(status));

    enviar_paquete_y_liberarlo(paquete, sch_fd);
}