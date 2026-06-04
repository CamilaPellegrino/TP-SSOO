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

    // iniciar servidor
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
            
            pthread_t thread_stick = crear_hilo_o_exit(atender_stick, nuevo_stick, "atender_stick", logger);
            pthread_detach(thread_stick);

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
            }case KM_READ:{
                t_list* data = recibir_paquete(sch_fd);
                int dir_fisica = *(int*) list_get(data, 0);
                int tamanio = *(int*) list_get(data, 1);
                int nro_stick = *(int*) list_get(data, 2);
                int pid =*(int*)  list_get(data, 3);
                char* datos_leidos = "Si esto anda soy una crack B)";
                t_paquete* paquete = crear_paquete(RTA_READ);
                agregar_a_paquete(paquete, &pid, sizeof(pid));
                agregar_string_a_paquete(paquete, datos_leidos);

                enviar_paquete_y_liberarlo(paquete, sch_fd);
            }
            default:
                log_warning(logger, "atender_scheduler: Operacion desconocida, cod_op=%d", cod_op);
        }
    }
    return NULL;
}
