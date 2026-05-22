#include "main.h"

int main(int argc, char* argv[]) { // ejecucion con valgrind: valgrind --leak-check=yes ./bin/kernel_scheduler kernel_scheduler.config a
    if(argc < 3){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/kernel_scheduler ./kernel_scheduler.config archivo.txt");
        exit(EXIT_FAILURE);
    }

    saludar("kernel_scheduler");
    
    char *ruta_config = argv[1];
    char *instrucciones_pid_0 = argv[2];

    t_config *config = iniciar_config(ruta_config); 

    // leer de config
    char* ip = config_get_string_value (config, "IP");
    char* puerto_kernel_memory = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    char* puerto_kernel_scheduler = config_get_string_value (config, "PUERTO_KERNEL_SCHEDULER");

    inicializar_variables_globales(config); // tambien inicializa el logger con el log_level de la config
 
    // testear(); // descomentar esto si solo queres testear y defini el test en test.c

    // conectar a kernel memory
    conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    log_info(logger, "## Conectado a Kernel Memory");
    handshake_cliente(conexion_kernel_memory, logger);

    // enviar mensaje a kernel memory
    enviar_mensaje("Hola, soy sche", conexion_kernel_memory, SCH_KM__CONEXION);
    
    pthread_t thread_km = crear_hilo_o_exit(atender_km, NULL, "thread_km");
    pthread_detach(thread_km);
    // iniciar servidor
    int kernel_scheduler_fd = iniciar_servidor_o_exit(puerto_kernel_scheduler); 

    // crear hilos de planificacion

    pthread_t hilo_largo_plazo = crear_hilo_o_exit(planificador_largo_plazo, NULL, "hilo_largo_plazo");
    pthread_t hilo_corto_plazo = crear_hilo_o_exit(planificador_corto_plazo, NULL, "hilo_corto_plazo");
    pthread_detach(hilo_largo_plazo);
    pthread_detach(hilo_corto_plazo);

    log_debug(logger, "hilos de planificacion creados");

    // agregar el proceso de pid 0
    t_pcb* pcb_pid_0 = nuevo_proc(0, -1, instrucciones_pid_0);
    
    // esperar clientes
    while(true){
        int *cliente_fd = esperar_cliente(kernel_scheduler_fd);
        log_info(logger, "Me llego un cliente, %d", *cliente_fd);

        handshake_servidor(*cliente_fd, logger);
        
        // crear del hilo que atiende al cliente que se acaba de conectar
        pthread_t thread;
        pthread_create(&thread, NULL, atender_cliente, cliente_fd);
        pthread_detach(thread);
    }
    return 0;
}

void* atender_cliente(void *arg){
    int *cliente_fd_ptr = (int *) arg;
    int cliente_fd = *cliente_fd_ptr;
    free(cliente_fd_ptr);
    op_code cod_op = recibir_operacion(cliente_fd);
    switch(cod_op){
        case CPU_SCH__CONEXION: {
            t_list* lista_paquete = recibir_paquete(cliente_fd);
            int* id_cpu = list_get(lista_paquete, 0);
            t_cpu* cpu = iniciar_cpu(*id_cpu, cliente_fd); 
            list_add(lista_cpus, cpu); // la agrego a la lista
            atender_cpu(cpu);
            break;
            
        }case IO_SCH__CONEXION: { 
            t_list *lista_paquete = recibir_paquete(cliente_fd);   // recibo un paquete con el tipo de io
            t_tipo_io *tipo_io = list_get(lista_paquete, 0);
            log_info(logger, "Me llego io de tipo: %d", *tipo_io);
            t_io* io = iniciar_io(*tipo_io, cliente_fd);

            list_add(lista_io, io);

            atender_io(io); 
            break;
        }default: 
            log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
    }
    return NULL;
}

void* atender_cpu(t_cpu* cpu){
    int cpu_fd = cpu->fd;
    int cpu_id = cpu->id;
    
    sem_post(&s_nueva_cpu_libre); 
    log_info(logger, "## CPU <%d> Conectada", cpu_id);
    while(1){
        op_code cod_op = recibir_operacion(cpu_fd);
        log_debug(logger, "llego operacion de cpu <%d>", cpu_id);
        if(cod_op == -1){
            log_warning(logger, "Se desconecto cpu de id:%d", cpu_id);
            break;
        }
        switch(cod_op){
            case CPU_SCH__SLEEP:{
                log_info(logger, "## (<%d>) - Solicito syscall: <SLEEP>", cpu->proceso->pid);
                t_list* lista_paquete = recibir_paquete(cpu_fd);
                int tiempo_sleep = *(int*)list_get(lista_paquete, 0);

                atender_cpu_syscall_sleep(tiempo_sleep, cpu);
                break;
            }case CPU_SCH__MUTEX_CREATE:{
                log_info(logger, "## (<%d>) - Solicito syscall: <MUTEX_CREATE>", cpu->proceso->pid);
                t_list* lista_paquete = recibir_paquete(cpu_fd);
                char* nombre_mutex = (char*)list_get(lista_paquete, 0);
                atender_cpu_syscall_mutex_create(nombre_mutex, cpu);
                break;
            }case CPU_SCH__MUTEX_LOCK:{
                log_info(logger, "## (<%d>) - Solicito syscall: <MUTEX_LOCK>", cpu->proceso->pid);
                t_list* lista_paquete = recibir_paquete(cpu_fd);
                char* nombre_mutex = (char*)list_get(lista_paquete, 0);
                atender_cpu_syscall_mutex_lock(nombre_mutex, cpu);
                break;
            }case CPU_SCH__MUTEX_UNLOCK:{
                log_info(logger, "## (<%d>) - Solicito syscall: <MUTEX_UNLOCK>", cpu->proceso->pid);
                t_list* lista_paquete = recibir_paquete(cpu_fd);
                char* nombre_mutex = (char*)list_get(lista_paquete, 0);
                atender_cpu_syscall_mutex_unlock(nombre_mutex, cpu);
                break;
            }case CPU_SCH__STDIN:{
                log_info(logger, "## (<%d>) - Solicito syscall: <STDIN>", cpu->proceso->pid);
                t_list* lista_paquete = recibir_paquete(cpu_fd);
                int dir_logica = *(int*)list_get(lista_paquete, 0);
                int tamanio = *(int*)list_get(lista_paquete, 1);
                atender_cpu_syscall_stdin(tamanio, dir_logica, cpu);
                break;
            }case CPU_SCH__STDOUT:{
                log_info(logger, "## (<%d>) - Solicito syscall: <STDOUT>", cpu->proceso->pid);
                t_list* lista_paquete = recibir_paquete(cpu_fd);
                int dir_logica = *(int*)list_get(lista_paquete, 0);
                int tamanio = *(int*)list_get(lista_paquete, 1);
                atender_cpu_syscall_stdout(tamanio, dir_logica, cpu);
                break;
            }case CPU_SCH__INIT_PROC:{
                log_info(logger, "## (<%d>) - Solicito syscall: <INIT_PROC>", cpu->proceso->pid);
                t_list* lista_paquete = recibir_paquete(cpu_fd);
                char* instrucciones = list_get(lista_paquete, 0);
                int prioridad = *(int*)list_get(lista_paquete, 1);
                t_pcb* pcb = nuevo_proc(prioridad, cpu->proceso->pid, instrucciones);
                break;
            }case CPU_SCH__EXIT:{
                log_info(logger, "## (<%d>) - Solicito syscall: <EXIT>", cpu->proceso->pid);
                t_pcb* proceso_exit = cpu->proceso;
                liberar_cpu(cpu);
                manejar_proceso_exit(proceso_exit);
                break;
            }default:
                log_warning(logger, "operacion desconocida en hilo que atiende a cpu id: %d, cod_op: %d", cpu_id, cod_op);
        }
        loguear_tamanio_listas_de_estado();
    }
    return NULL;
}
void atender_cpu_syscall_mutex_unlock(char* nombre_mutex, t_cpu* cpu){
    t_mutex* mutex = get_mutex(nombre_mutex);
    t_pcb* proceso = cpu->proceso;
    if(mutex == NULL){
        // TODO: no existe un mutex con ese nombre en lista_mutex, devolver a CPU codigo de error
    }
    m_signal(mutex, proceso);
    log_info(logger, "## (<%d>) Libera el Mutex <%s>", proceso->pid, nombre_mutex);
    enviar_operacion(cpu->fd, SCH_CPU__REANUDAR_EJECUCION);
}

void atender_cpu_syscall_mutex_lock(char* nombre_mutex, t_cpu* cpu){
    t_mutex* mutex = get_mutex(nombre_mutex);
    if(mutex == NULL){
        log_error(logger, "NotFoundException: Mutex de nombre <%s> no declarado", nombre_mutex);
        enviar_operacion(cpu->fd, SCH_CPU__DETENER_EJECUCION);
        exec_a_exit(cpu->proceso);
        liberar_cpu(cpu);
        return;
        // TODO: no existe un mutex con ese nombre en lista_mutex, devolver a CPU codigo de error
    }
    t_pcb* proceso = cpu->proceso;
    bool reservado = m_wait(mutex, proceso);
    if(reservado){
        log_debug(logger, "atender_cpu_syscall_mutex_lock: mutex %s reservado", nombre_mutex);
        enviar_operacion(cpu->fd, SCH_CPU__REANUDAR_EJECUCION);
    }else{
        log_debug(logger, "atender_cpu_syscall_mutex_lock: mutex %s no disponible, bloqueando proceso y desalojando de cpu", nombre_mutex);
        
        // detener ejecucion de cpu
        // enviar_operacion(cpu->fd, SCH_CPU__DETENER_EJECUCION);
        liberar_cpu(cpu);
        log_debug(logger, "test1: libero cpu");
        // bloquear proceso
        exec_a_blocked_cond_signal(proceso);
        log_debug(logger, "test1: paso a blocked");

    }

}

void atender_cpu_syscall_stdout(int tamanio, int dir_logica, t_cpu* cpu){
    t_pcb* proceso = cpu->proceso;
    exec_a_blocked_cond_signal(proceso);
    liberar_cpu(cpu);
    log_debug(logger, "tamanio a leer: %d, dir logica: %d", tamanio, dir_logica);
    // crear evento
    t_evt* evt = iniciar_evt_std_in_out(tamanio, dir_logica, proceso);
    // crear el hilo de timeout para que dps del timeout se suspenda el proceso
    evt->hilo_timeout = crear_hilo_o_exit(hilo_timeout, evt, "hilo_esperar_timeout");
    
    // agregar evt a lista de evts
    pthread_mutex_lock(&m_lista_evt_stdout);
    list_add(lista_evt_stdout, evt);
    pthread_mutex_unlock(&m_lista_evt_stdout);
    sem_post(&s_evt_stdout);
    log_debug(logger, "atender_cpu: sem_post(&s_evt_stdout)");
}

void atender_cpu_syscall_stdin(int tamanio, int dir_logica, t_cpu* cpu){ // TODO: Codigo muy parecido a atender_cpu_syscall_sleep, juntarlo
    t_pcb* proceso = cpu->proceso;
    exec_a_blocked_cond_signal(proceso);
    liberar_cpu(cpu);
    log_debug(logger, "tamanio a leer: %d, dir logica: %d", tamanio, dir_logica);
    // crear evento
    t_evt* evt = iniciar_evt_std_in_out(tamanio, dir_logica, proceso);
    // crear el hilo de timeout para que dps del timeout se suspenda el proceso
    evt->hilo_timeout = crear_hilo_o_exit(hilo_timeout, evt, "hilo_esperar_timeout");
    
    // agregar evt a lista de evts
    pthread_mutex_lock(&m_lista_evt_stdin);
    list_add(lista_evt_stdin, evt);
    pthread_mutex_unlock(&m_lista_evt_stdin);
    sem_post(&s_evt_stdin);
    log_debug(logger, "atender_cpu: sem_post(&s_evt_stdin)");
}

void atender_cpu_syscall_sleep(int tiempo_sleep, t_cpu* cpu){
    t_pcb* proceso = cpu->proceso;
    exec_a_blocked_cond_signal(proceso);
    liberar_cpu(cpu);
    log_debug(logger, "tiempo: %d", tiempo_sleep);
    
    // crear evento
    t_evt* evt = iniciar_evt_sleep(tiempo_sleep, proceso);
    // crear el hilo de timeout para que dps del timeout se suspenda el proceso
    evt->hilo_timeout = crear_hilo_o_exit(hilo_timeout, evt, "hilo_esperar_timeout");
    
    log_debug(logger, "TEST01: hilo timeout creado");
    // agregar evt a lista de evts
    pthread_mutex_lock(&m_lista_evt_sleep);
    log_debug(logger, "TEST01: mutex tomado");

    list_add(lista_evt_sleep, evt);
    pthread_mutex_unlock(&m_lista_evt_sleep);
    sem_post(&s_evt_sleep);
    log_debug(logger, "atender_cpu: sem_post(&s_evt_sleep)");
}

void atender_cpu_syscall_mutex_create(char* nombre_mutex, t_cpu* cpu){
    int cpu_fd = cpu->fd;
    t_mutex* mutex = m_create(nombre_mutex);
    
    pthread_mutex_lock(&m_lista_mutex);
    list_add(lista_mutex, mutex);
    pthread_mutex_unlock(&m_lista_mutex);

    log_debug(logger, "nuevo mutex agregado, tamaño lista ahora: %d", list_size(lista_mutex));

    // mandar a CPU confirmacion de que se termino la syscall (esta no es bloqueante)
    enviar_operacion(cpu_fd, SCH_CPU__REANUDAR_EJECUCION); // reanuda la ejecucion con el mismo pcb que tenia cargado. Solo para este caso creo, porque no desaloja el, cpu espera (ver issues)
}

// Atender modulo KM
void* atender_km(void*){
    while(1){
        op_code cod_op = recibir_operacion(conexion_kernel_memory);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto kernel memory");
            // exit(EXIT_FAILURE);
            break;
        }

        switch(cod_op){
            case KM_SCH__NUEVO_STICK: {
                log_info(logger, "Conexion de modulo stick");
                t_list* lista_paquete = recibir_paquete(conexion_kernel_memory);
                char* ip_stick = list_get(lista_paquete, 0);
                char* puerto_stick = list_get(lista_paquete, 1);
                int *tamanio_stick = list_get(lista_paquete, 2);
                list_destroy(lista_paquete);
                
                t_paquete* paquete = crear_paquete(SCH_CPU__NUEVO_STICK);
                agregar_string_a_paquete(paquete, ip_stick);
                agregar_string_a_paquete(paquete, puerto_stick);
                agregar_a_paquete(paquete, tamanio_stick, sizeof(int));
                enviar_paquete_a_todas_las_cpus(paquete);

                break;
            }case KM_SCH__BSOD: {
                log_error(logger, "BSOD, cerrando todo");
                exit(EXIT_FAILURE);
            }case KM_SCH__EXIT_OK: {
                t_list* data_pcb = recibir_paquete(conexion_kernel_memory);
                int pid = *(int*)list_get(data_pcb, 0);
                liberar_pcb_de_exit(pid);
                break;
            }case KM_SCH__INIT_PROC_RESP:{
                t_list* paquete = recibir_paquete(conexion_kernel_memory);
                int pid = *(int*)list_get(paquete, 0);
                bool ok = *(bool*)list_get(paquete,1);
                if(ok){
                    log_debug(logger, "Init proc de pid %d OK", pid);
                    sem_post(&s_nuevo_proceso_new);
                }else{
                    log_warning(logger, "error en init_proc de pid %d", pid);
                }
                break;
            }default: 
                log_warning(logger, "Warning: Operacion desconocida, cod_op = %d",cod_op);
        }
    }
    return NULL;
}
