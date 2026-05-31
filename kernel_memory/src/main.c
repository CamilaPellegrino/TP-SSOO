#include "main.h"

int main(int argc, char* argv[]) { //KERNEL MEMORY
    if(argc < 2){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/kernel_memory ./kernel_memory.config");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];

    t_config* config = iniciar_config(ruta_config);

    inicializar_variables_globales(config);

    // testear(); // Descomentar para ejecutar TESTS

    // iniciar servidor
    int kernel_memory_fd = iniciar_servidor_o_exit(puerto, logger);

    // esperar clientes
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
            pthread_t thread_sch= crear_hilo_o_exit(atender_scheduler, NULL, "atender_scheduler", logger);
            pthread_detach(thread_sch);
            free(msg);
            break;
        }case STICK_KM__CONEXION: {
            // recibo el paquete que me mando stick en una t_list
            t_list *lista_paquete = recibir_paquete(cliente_fd);

            // leo cada elemento de la t_list en el orden en que stick los agrego al paquete
            int tamanio = *(int*)list_get(lista_paquete, 0);
            char *puerto = list_get(lista_paquete, 1);
            char *ip = list_get(lista_paquete, 2);
            log_info(logger, "Me llego un stick: %d, %s, %s", tamanio, puerto, ip);
            
            // poner al stick en la lista de sticks
            t_stick* nuevo_stick = iniciar_stick(ip, puerto, tamanio, cliente_fd);
            
            agregar_stick(nuevo_stick);

            enviar_nuevo_stick_a_scheduler(nuevo_stick, sch_fd);
            
            list_destroy_and_destroy_elements(lista_paquete, free);            
            break;
        }case SWAP_KM__CONEXION:{
            log_info(logger, "SWAP");
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
            case KM_GET_INSTRUCTION: {
                t_list* paquete = recibir_paquete(cpu_fd);
                uint32_t pid = *(uint32_t*)list_get(paquete, 0);
                uint32_t pc = *(uint32_t*)list_get(paquete, 1);
                
                // MOCK: Enviar instrucción genérica para que CPU avance 
                char* instruccion = "SET AX 1"; 
                log_info(logger, "## PID: %u - Obtener instrucción: %u - Instrucción: %s", pid, pc, instruccion);
                
                enviar_mensaje(instruccion, cpu_fd, HANDSHAKE);
                list_destroy_and_destroy_elements(paquete, free);
                break;
            }case CPU_KM__PCONTEXTO:{
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
                
                list_destroy_and_destroy_elements( lista, free);
                break;
            }
            case KM_WRITE:{
                // MOCK: Responder OK sin implementar lógica real [3]
                enviar_mensaje("OK", cpu_fd, KM_CPU__RESPUESTA);
                log_info(logger, "Respuesta MOCK: OK");
                break;
            }
            case CPU_KM__ACTUALIZAR_PCB:
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

void enviar_sticks_a_cpu(int cpu_fd){
    t_paquete* paquete = crear_paquete(KM_CPU__STICKS);
    int cant_sticks = list_size(lista_sticks);
    agregar_a_paquete(paquete, &cant_sticks, sizeof(int));
    for(int i = 0; i < cant_sticks; i++){
        t_stick* stick = list_get(lista_sticks, i);
        agregar_stick_a_paquete(paquete, stick);
    }
    enviar_paquete_y_liberarlo(paquete, cpu_fd);
}

void recibir_pcb_actualizado(t_list* valores){
    int i = 0;
    // pcb
    int* pid = list_get(valores, i++);
    t_proceso* proc = proceso_de_pid(*pid);
    t_pcb* pcb = proc->pcb;

    if(proc == NULL){
        log_error(logger, "recibir_pcb_actualizado, Error: No se encontro proceso de pid %d", *pid);
        list_destroy_and_destroy_elements(valores, free);
        return;
    }
    memcpy(&pcb->pid, pid, sizeof(pcb->pid));
    memcpy(&pcb->ppid, list_get(valores, i++), sizeof(pcb->ppid));
    memcpy(&pcb->prioridad, list_get(valores, i++), sizeof(pcb->prioridad));
    // registros
    memcpy(&pcb->pc, list_get(valores, i++), sizeof(pcb->pc));

    memcpy(&pcb->ax, list_get(valores, i++), sizeof(pcb->ax));
    memcpy(&pcb->bx, list_get(valores, i++), sizeof(pcb->bx));
    memcpy(&pcb->cx, list_get(valores, i++), sizeof(pcb->cx));
    memcpy(&pcb->dx, list_get(valores, i++), sizeof(pcb->dx));

    memcpy(&pcb->eax, list_get(valores, i++), sizeof(pcb->eax));
    memcpy(&pcb->ebx, list_get(valores, i++), sizeof(pcb->ebx));
    memcpy(&pcb->ecx, list_get(valores, i++), sizeof(pcb->ecx));
    memcpy(&pcb->edx, list_get(valores, i++), sizeof(pcb->edx));

    memcpy(&pcb->si, list_get(valores, i++), sizeof(pcb->si));
    memcpy(&pcb->di, list_get(valores, i++), sizeof(pcb->di));

    list_destroy_and_destroy_elements(valores, free);
    return;
}

void agregar_pcb_al_paquete(t_pcb* pcb, t_paquete* p){
    // datos del pcb
    agregar_a_paquete(p, &pcb->pid, sizeof(pcb->pid));
    agregar_a_paquete(p, &pcb->ppid, sizeof(pcb->ppid));
    agregar_a_paquete(p, &pcb->prioridad, sizeof(pcb->prioridad));

    // registros
    agregar_a_paquete(p, &pcb->pc, sizeof(pcb->pc));

    agregar_a_paquete(p, &pcb->ax, sizeof(pcb->ax));
    agregar_a_paquete(p, &pcb->bx, sizeof(pcb->bx));
    agregar_a_paquete(p, &pcb->cx, sizeof(pcb->cx));
    agregar_a_paquete(p, &pcb->dx, sizeof(pcb->dx));

    agregar_a_paquete(p, &pcb->eax, sizeof(pcb->eax));
    agregar_a_paquete(p, &pcb->ebx, sizeof(pcb->ebx));
    agregar_a_paquete(p, &pcb->ecx, sizeof(pcb->ecx));
    agregar_a_paquete(p, &pcb->edx, sizeof(pcb->edx));

    agregar_a_paquete(p, &pcb->si, sizeof(pcb->si));
    agregar_a_paquete(p, &pcb->di, sizeof(pcb->di));
}

void atender_swap(int swap_fd){
    while(1){
        log_info(logger, "****Atendiendo al SWAP");
        op_code cod_op = recibir_operacion(swap_fd);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto SWAP");
            break;
        }
        
    }
    log_info(logger, "cerrando hilo de swap");
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
                int i = 0;
                int pid = *(int*)list_get(lista_paquete, i++);
                int ppid = *(int*)list_get(lista_paquete, i++);

                char* ruta_instrucciones = strdup((char*)list_get(lista_paquete, i++));
                list_destroy_and_destroy_elements(lista_paquete, free);

                bool ok = guardar_nuevo_proceso(pid, ppid, ruta_instrucciones);
                // enviar confirmacion a sch: 
                t_paquete* paquete_conf = crear_paquete(KM_SCH__INIT_PROC_RESP);
                agregar_a_paquete(paquete_conf, &pid, sizeof(int));
                agregar_a_paquete(paquete_conf, &ok, sizeof(bool));
                enviar_paquete_y_liberarlo(paquete_conf, sch_fd);
                break;
            }
            default:
                log_warning(logger, "atender_scheduler: Operacion desconocida, cod_op=%d", cod_op);
        }
    }
    return NULL;
}

bool guardar_nuevo_proceso(int pid, int ppid, char* ruta){
    t_pcb* pcb = calloc(1, sizeof(t_pcb));
    if (pcb == NULL) {
        return NULL;
    }
    pcb->pid = pid;
    pcb->ppid = ppid;
    t_list* instrucciones = instrucciones_de_ruta(ruta);
    t_proceso* proceso = iniciar_proceso(pcb, instrucciones);
    if(proceso == NULL){
        return false;
    }
    list_add(lista_procesos, proceso);
    log_info(logger, "Nuevo proceso de PID <%d> guardado", pcb->pid);
    return true;
}

t_list* instrucciones_de_ruta(char* nombre_archivo){
    char* ruta = ruta_completa(scripts_basepath, nombre_archivo);
    FILE* archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        return NULL;
    }
    t_list* lineas = list_create();
    char* linea = NULL;
    size_t len = 0;
    ssize_t leidos;

    while ((leidos = getline(&linea, &len, archivo)) != -1) {
        if (leidos > 0 && linea[leidos - 1] == '\n') {
            linea[leidos - 1] = '\0';
        }
        list_add(lineas, strdup(linea));
    }
    free(ruta);
    free(linea);
    fclose(archivo);
    return lineas;
}

void atender_stick(int stick_fd, int *tamanio){
    log_info(logger, "## Memory Stick de %d bytes Conectada", *tamanio);

    while(1){
        op_code cod_op = recibir_operacion(stick_fd);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto stick");
            // corrupcion de memoria
            enviar_operacion(sch_fd, KM_SCH__BSOD);
            exit(EXIT_FAILURE);
            break;
        }
    };
    log_info(logger, "cerrando hilo de stick");
}

void enviar_nuevo_stick_a_scheduler(t_stick* nuevo_stick, int sch_fd){
    t_paquete* paquete = crear_paquete(KM_SCH__NUEVO_STICK);
    agregar_string_a_paquete(paquete, nuevo_stick->ip);
    agregar_string_a_paquete(paquete, nuevo_stick->puerto);
    agregar_a_paquete(paquete, &(nuevo_stick->tamanio), sizeof(int));
    enviar_paquete_y_liberarlo(paquete, sch_fd);
    log_info(logger, "Enviando stick con IP %s al SCHED por el FD %d",nuevo_stick->ip, sch_fd);
}