#include <utils/hello.h>
#include <utils/utils.h>
#include "base_cpu.h"
#include <semaphore.h>

void* ejecutar();
void detener_ejecucion();
void reanudar_ejecucion();
void inicializar_variables();
void ejecutar_sleep(t_instruccion_decodificada* instr);
void ejecutar_stdin(t_instruccion_decodificada* instr);
void ejecutar_stdout(t_instruccion_decodificada* instr);
void ejecutar_m_create(t_instruccion_decodificada* instr);
void ejecutar_m_lock(t_instruccion_decodificada* instr);
void ejecutar_m_unlock(t_instruccion_decodificada* instr);
void ejecutar_exit(t_instruccion_decodificada* instr);
void esperar_a_poder_ejecutar(); 
void ejecutar_exit(t_instruccion_decodificada* instr);
void ejecutar_init_proc(t_instruccion_decodificada* instr);

// variables globales
t_log* logger;
t_list* lista_sticks;  // lista global para guardar los memory sticks a los que me conecte

bool v_ejecutar;
pthread_cond_t cond_ejecutar;
pthread_mutex_t m_ejecutar;

int pid_pendiente;
pthread_mutex_t m_pid_pendiente;

// otros
int conexion_kernel_scheduler;
int conexion_kernel_memory;

///////////////////Funciones ALAN///////////////////
t_pcb* pedir_contexto(int pid);
void ciclo_instruccion(t_pcb *pcb);
char* fetch(t_pcb *pcb);
void decode(char *instruccion, t_pcb* pcb,  t_instruccion_decodificada * instruccion_decodificada);
void execute(t_instruccion_decodificada *instruccion, t_pcb *pcb);
especificacion_registro * obtener_registro(char* registro_crudo, t_pcb *pcb);
void escribir_registro(especificacion_registro *reg, int valor);
uint32_t leer_registro(especificacion_registro* reg);
void ejecutar_syscall(t_instruccion_decodificada *syscall, t_pcb* pcb);
//////////////////////////

int main(int argc, char* argv[]){
    // ejemplo para ejecutar: ./bin/cpu ./cpu.config 0
    if(argc < 3){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/cpu ./cpu.config 0");
        exit(EXIT_FAILURE);
    }

    char *ruta_config = argv[1];
    int id_cpu = atoi(argv[2]);

    saludar("cpu");
    
    t_config* config;
    char *ip;
    char *puerto_kernel_scheduler;
    char *puerto_kernel_memory;

    // crear logger
    logger = iniciar_logger("cpu.log", "ProcesoCPU", LOG_LEVEL_DEBUG);

    // iniciar config
    config = iniciar_config(ruta_config);
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }

    ip = config_get_string_value(config, "IP");
    
    puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    puerto_kernel_scheduler = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");

    // conectar a kernel scheduler
    conexion_kernel_scheduler = crear_conexion(ip, puerto_kernel_scheduler);
    exit_si_error_conexion(conexion_kernel_scheduler, logger, "kernel scheduler");
    handshake_cliente(conexion_kernel_scheduler,logger);
    
    // enviar info propia a sch:
    t_paquete *paquete_conexion_sch = crear_paquete(CPU_SCH__CONEXION);
    agregar_a_paquete(paquete_conexion_sch, &id_cpu, sizeof(id_cpu));
    enviar_paquete_y_liberarlo(paquete_conexion_sch, conexion_kernel_scheduler);

    // conectar a kernel memory
    conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    handshake_cliente(conexion_kernel_memory,logger);
        
    //mando informacion propia al km
    t_paquete *paquete_conexion_km = crear_paquete(CPU_KM__CONEXION);
    agregar_a_paquete(paquete_conexion_km, &id_cpu, sizeof(id_cpu));
    enviar_paquete_y_liberarlo(paquete_conexion_km, conexion_kernel_memory);

    // inicializar variables
    inicializar_variables();
    
    // hilo que ejecuta instrucciones:
    pthread_t hilo_ejecucion;
    pthread_create(&hilo_ejecucion, NULL, ejecutar, NULL);
    pthread_detach(hilo_ejecucion);

    // queda escuchando mensajes que envie el scheduler
    while (1){
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);log_debug(logger, "RECIBI OP=%d", cod_op);
        if(cod_op == -1){
            log_error(logger, "Error: se desconecto scheduler"); 
            break; 
        }
        switch(cod_op){
            case SCH_CPU__PID:{ // ej: al principio, o despues de haber desalojado cuando le asigna otro proceso
                log_debug(logger, "hilo principal: nuevo_pid");
                pthread_mutex_lock(&m_pid_pendiente);
                t_list* lista_paquete = recibir_paquete(conexion_kernel_scheduler);
                int* nuevo_pid = list_get(lista_paquete, 0);
                pid_pendiente = *nuevo_pid;
                reanudar_ejecucion();
                pthread_mutex_unlock(&m_pid_pendiente);
                list_destroy_and_destroy_elements(lista_paquete, free);
                break;
            }
            case SCH_CPU__DETENER_EJECUCION:{ // ej cuando hace un MUTEX_LOCK y se bloquea, o cuando se desaloja
                log_debug(logger, "hilo principal: detener_ejecucion");
                detener_ejecucion();
                break;
            }
            case SCH_CPU__REANUDAR_EJECUCION:{ // cuando hace MUTEX_LOCK y estaba el semaforo disponible, no bloquea el proceso
                log_debug(logger, "hilo principal: Reanudar ejecucion");
                reanudar_ejecucion();
                break;
            }
            case SCH_CPU__NUEVO_STICK: {
                log_info(logger, "Conexion de modulo stick");
                t_list * lista_del_paquete = recibir_paquete(conexion_kernel_scheduler);
                
                char* ip_stick = list_get(lista_del_paquete, 0);
                char* puerto_stick = list_get(lista_del_paquete, 1);
                int* tamanio = list_get(lista_del_paquete, 2); 
                                
                // conectar a memory stick
                int conexion_memory_stick = crear_conexion(ip_stick, puerto_stick);
                exit_si_error_conexion(conexion_memory_stick, logger, "memory stick");
                handshake_cliente(conexion_memory_stick,logger);
                
                log_info(logger, "Datos de stick recibidos, ip: %s, puerto: %s", ip_stick, puerto_stick);

                // guardar datos del stick en lista_sticks
                t_stick* stick = iniciar_stick(ip_stick, puerto_stick, *tamanio, conexion_memory_stick);
                list_add(lista_sticks, stick);
                
                // liberar lista_del_paquete
                list_destroy_and_destroy_elements(lista_del_paquete, free);
                break;
            }
            default:
                log_warning(logger, "Warning: Operacion desconocida, cod_op = %d", cod_op);
                break;
        }
    }
    
    // liberar logger, config y conexiones
    log_destroy(logger);
    config_destroy(config);
    close(conexion_kernel_scheduler);
    close(conexion_kernel_memory);
    return 0;
}

void* ejecutar(){
    t_instruccion_decodificada* prox_instruccion;
    t_pcb* pcb;
    while(1){
        log_debug(logger, "ejecutar: esperando a poder ejecutar");
        esperar_a_poder_ejecutar(); 
        log_debug(logger, "ejecutar: ejecutar vale true");
        pthread_mutex_lock(&m_pid_pendiente);

        if(pid_pendiente >= 0){ // significa que tengo que cambiar de proceso
            // enviar el pcb actual a km para que lo actualice
            // ...
            // reemplazar pcb actual por el pendiente (recibirlo de km)
            // ...
            pcb = pedir_contexto(pid_pendiente);
            log_debug(logger, "ejecutar: pidiendo nuevo pcb de pid %d a km y cargandolo", pid_pendiente);
            pid_pendiente = -1;
        }
        pthread_mutex_unlock(&m_pid_pendiente);
        /*
        log_debug(logger, "ejecutar: buscando prox instruccion");
        prox_instruccion = proxima_instruccion();
        if(prox_instruccion == NULL){ // puede pasar esto?
            log_debug(logger, "en ejecutar: prox_instruccion valia NULL");
            detener_ejecucion();
            continue;
        };
        log_debug(logger, "ejecutar: proxima instruccion de tipo: %d, pc = %d", prox_instruccion->tipo, pc);
        
        pc++; // cuando haya un salto no se deberia aumentar pc
        */
        ciclo_instruccion(pcb);

        
    }
    return NULL;
}

void ciclo_instruccion(t_pcb *pcb){
        //FECH
        char *instruccion = fetch(pcb);
        if(instruccion == NULL){
        log_error(logger, "FETCH devolvio NULL");
        return;
        }
        
        //DECODE
        t_instruccion_decodificada * instruccion_decodificada = malloc(sizeof(t_instruccion_decodificada));
        decode(instruccion, pcb, instruccion_decodificada);
        log_info(logger, "pc %i", pcb->registros.pc);
        log_info( logger, "Instruccion: %s", instruccion);
        int pc_antiguo = pcb->registros.pc;
        //execute
        execute(instruccion_decodificada, pcb);
        
        if(pcb->registros.pc == pc_antiguo)
            pcb->registros.pc++;

        list_destroy(instruccion_decodificada->registros);
        free(instruccion_decodificada);
        free(instruccion);
        //list_destroy_and_destroy_elements(instruccion_decodificada->registros, registro_destroy);
        //free(i_decodificada);
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
    log_debug(logger, "instruccion: %s", instruccion);
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
    else if(string_equals_ignore_case(partes[0], "EXIT")){
        instruccion_decodificada->tipo = I_EXIT;
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
        list_add(instruccion_decodificada->registros, partes[1]); // dir logica
        list_add(instruccion_decodificada->registros, partes[2]); // tamanio
        string_array_destroy(partes);
        return;
    }    
    else if(string_equals_ignore_case(partes[0], "STDOUT")){
        instruccion_decodificada->tipo = I_STDOUT;
        list_add(instruccion_decodificada->registros, partes[1]); // dir logica
        list_add(instruccion_decodificada->registros, partes[2]); // tamanio
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "MUTEX_LOCK")){
        instruccion_decodificada->tipo = I_MUTEX_LOCK;
        log_info(logger, "nombre mutex: %s", partes[1]);
        char* nombre_mutex = strdup(partes[1]);
        list_add(instruccion_decodificada->registros, nombre_mutex); // nombre
        log_debug(logger, "instruccion, param 1: %s, en instruc_decod: %s", partes[1], list_get(instruccion_decodificada->registros, 0));
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
        log_debug(logger, "parm: %s", list_get(instruccion_decodificada->registros, 0));
        return;
    }
    string_array_destroy(partes);
    return;
}
t_pcb* pedir_contexto(int pid){
   // enviar_operacion(conexion_kernel_memory, CPU_KM__PCONTEXTO);
    t_paquete *paquete = crear_paquete(CPU_KM__PCONTEXTO);

    agregar_a_paquete(paquete, &pid, sizeof(int));

    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    
    op_code cod_op = recibir_operacion(conexion_kernel_memory);

    if(cod_op != KM_CPU__RTA_CONTEXTO) {
        printf("ERROR CONTEXTO\\n");
        exit(EXIT_FAILURE);
    }

    t_list* lista_contexto = recibir_paquete(conexion_kernel_memory);

    t_pcb *pcb = malloc(sizeof(t_pcb));
    int i = 0;
    pcb->pid = *(int*) list_get(lista_contexto, i++);
    pcb->ppid = *(int*)list_get(lista_contexto, i++);
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


    list_destroy_and_destroy_elements(
        lista_contexto,
        free
    );

    return pcb;
}

void execute(t_instruccion_decodificada * instruccion, t_pcb *pcb){

    switch (instruccion->tipo){
        case I_NOOP:
            break;
        case I_SET://Asigna al registro el valor pasado como parámetro. SET ax 5
            especificacion_registro* reg =list_get(instruccion->registros,0);
            int *input  =list_get(instruccion->registros,1);
            log_info(logger, "antes de set, valor: %p",reg->ptro_reg);
            escribir_registro(reg, *input);
            log_info(logger, "despues de set, valor: %p",reg->ptro_reg);
            free(reg);
            free(input);
            break;
        case I_SUM:
            log_info(logger, "Ejecutando instruccion JNZ");
            especificacion_registro *destino = list_get(instruccion->registros,0);
            especificacion_registro *origen = list_get(instruccion->registros,1);
            uint32_t suma =leer_registro(destino)+leer_registro(origen);
            escribir_registro(destino, suma);
            free(origen);
            free(destino);
            break;
        case I_SUB:
            log_info(logger, "Ejecutando instruccion SUB");
            especificacion_registro *minuendo = list_get(instruccion->registros,0);
            especificacion_registro *sustraendo = list_get(instruccion->registros,1);
            uint32_t resta =leer_registro(minuendo)-leer_registro(sustraendo);
            escribir_registro(minuendo, resta);
            free(minuendo);
            free(sustraendo);
            log_info(logger, "hice sub");
            break;
        case I_JNZ:
            log_info(logger, "Ejecutando instruccion JNZ");
            especificacion_registro* reg_ = list_get(instruccion->registros,0);
            int* nuevo_pc =list_get(instruccion->registros,1);
            log_info(logger, "pc %i", pcb->registros.pc);
            if(leer_registro(reg_) != 0)
                pcb->registros.pc = *nuevo_pc;
            log_info(logger, "pc %i", pcb->registros.pc);
            free(reg_);
            free(nuevo_pc);
            break;
        case I_SLEEP:
            ejecutar_sleep(instruccion);
            break;
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
            ejecutar_stdin(instruccion);
            break;
        case I_STDOUT:
            ejecutar_stdout(instruccion);
            break;
        case I_EXIT:
            ejecutar_exit(instruccion);
            break;
        case I_INIT_PROC:
            ejecutar_init_proc(instruccion);
            break;
        default:
            log_warning(logger, "Instruccion no implementada, tipo %d", instruccion->tipo);
            break;
    }
    list_destroy_and_destroy_elements(i->registros, free);
}

void esperar_a_poder_ejecutar(){
    pthread_mutex_lock(&m_ejecutar);
    while(!v_ejecutar){
        pthread_cond_wait(&cond_ejecutar, &m_ejecutar);
        log_debug(logger, "espserando a poder v_ejecutar, v_ejecutar = %d", v_ejecutar);
    }
    log_debug(logger, "saliendo del while, v_ejecutar true");
    pthread_mutex_unlock(&m_ejecutar);
}

// syscalls: 
void ejecutar_init_proc(t_instruccion_decodificada* instr){
    log_debug(logger, "Ejecutando INIT_PROC");
    detener_ejecucion();
    char* ruta_archivo_instrucciones = list_get(instr->registros, 0);
    int prioridad = *(int*)list_get(instr->registros, 1);
    t_paquete* paquete = crear_paquete(CPU_SCH__INIT_PROC);
    agregar_string_a_paquete(paquete, ruta_archivo_instrucciones);
    agregar_a_paquete(paquete, &prioridad, sizeof(prioridad));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_exit(t_instruccion_decodificada* instr){
    log_debug(logger, "Ejecutando EXIT");
    detener_ejecucion();
    enviar_operacion(conexion_kernel_scheduler, CPU_SCH__EXIT);
}

void ejecutar_m_unlock(t_instruccion_decodificada* instr){
    detener_ejecucion(); // cpu tiene que esperar a que esta syscall se termine (puede ser instantaneo, o no, depende) para seguir ejecutando
    char* nombre = (char*)list_get(instr->registros, 0);
    log_debug(logger, "Ejecutando MUTEX_UNLOCK %s", nombre);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_UNLOCK);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_m_lock(t_instruccion_decodificada* instr){
    detener_ejecucion(); // cpu tiene que esperar a que esta syscall se termine (puede ser instantaneo, o no, depende) para seguir ejecutando
    char * nombre = list_get(instr->registros, 0);
    //char* nombre = instr->param1;
    log_debug(logger, "Ejecutando MUTEX_LOCK %d", *nombre);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_LOCK);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_m_create(t_instruccion_decodificada* instr){
    detener_ejecucion(); // cpu tiene que esperar a que esta syscall se termine (es instantaneo) para seguir ejecutando
    char * nombre = list_get(instr->registros, 0);
    //char* nombre = instr->param1;
    log_debug(logger, "Ejecutando MUTEX_CREATE %d", *nombre);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_CREATE);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_stdout(t_instruccion_decodificada* instr){
    detener_ejecucion(); // esta es bloqueante, obligatoriamente detiene la ejecucion hasta que scheduler le mande el proximo pid
    int * dir_logica = list_get(instr->registros, 0);
    int * tamanio = list_get(instr->registros, 0);
    //int dir_logica = atoi(instr->param1);
    //int tamanio = atoi(instr->param2);
    t_paquete* paquete = crear_paquete(CPU_SCH__STDOUT);
    agregar_a_paquete(paquete, &dir_logica, sizeof(dir_logica));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_stdin(t_instruccion_decodificada* instr){
    detener_ejecucion(); // esta es bloqueante, obligatoriamente detiene la ejecucion hasta que scheduler le mande el proximo pid
    int dir_logica = *(int*)list_get(instr->registros, 0); // TODO: esto tiene q ser distinto cuando le pida a km las cosas, se deberia poder guardar en un registro, ej "AX"
    int tamanio = *(int*)list_get(instr->registros, 1);
    t_paquete* paquete = crear_paquete(CPU_SCH__STDIN);
    agregar_a_paquete(paquete, &dir_logica, sizeof(dir_logica));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_sleep(t_instruccion_decodificada* instruccion){
    detener_ejecucion(); // esta es bloqueante, obligatoriamente detiene la ejecucion hasta que scheduler le mande el proximo pid
    int tiempo_sleep = *(int*)list_get(instruccion->registros, 0);
    log_debug(logger, "Ejecutando SLEEP %d", tiempo_sleep);
    t_paquete* paquete = crear_paquete(CPU_SCH__SLEEP);
    agregar_a_paquete(paquete, &tiempo_sleep, sizeof(tiempo_sleep));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void detener_ejecucion(){
    pthread_mutex_lock(&m_ejecutar);
    v_ejecutar = false;
    log_debug(logger, "detener");
    pthread_mutex_unlock(&m_ejecutar);
}

void reanudar_ejecucion(){
    pthread_mutex_lock(&m_ejecutar);
    v_ejecutar = true;
    pthread_cond_signal(&cond_ejecutar);
    log_debug(logger, "Reanudar");
    pthread_mutex_unlock(&m_ejecutar);
}

void inicializar_variables(){
    v_ejecutar = false;
    pid_pendiente = -1;
    lista_sticks = list_create();
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