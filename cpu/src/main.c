#include <utils/hello.h>
#include <utils/utils.h>
#include "base_cpu.h"
#include <semaphore.h>


void procesar_evento(op_code cod_op);
void transicion_desde_wait_sys(op_code cod_op);
void transicion_desde_exec(op_code cod_op);
void transicion_desde_wait_sys_y_prox_desalojo(op_code cod_op);
void transicion_desde_libre(op_code cod_op);
void transicion_desde_wait_mem_alloc(op_code cod_op);
void transicion_desde_wait_mem_alloc_y_prox_desalojo(op_code cod_op);
void transicionar(t_estado_cpu estado);
void transicionar_thread_safe(t_estado_cpu estado);
void enviar_confirmacion();
void set_pedir_segmentos(bool r);
bool get_pedir_segmentos();
void set_enviar_contexto_y_desalojar(bool x);
bool get_enviar_contexto_y_desalojar();

void conectarse_a_stick(char* ip, char* puerto, uint32_t tamanio);
void recibir_sticks_de_km();
void* ejecutar();
void inicializar_variables();
void enviar_pcb_actualizado_a_km(t_pcb* pcb);
void agregar_pcb_al_paquete(t_pcb* pcb, t_paquete* p);
void ejecutar_sleep(t_instruccion_decodificada* instr);
void ejecutar_stdin(t_instruccion_decodificada* instr, t_pcb*);
void ejecutar_stdout(t_instruccion_decodificada* instr, t_pcb*);
void ejecutar_m_create(t_instruccion_decodificada* instr);
void ejecutar_m_lock(t_instruccion_decodificada* instr);
void ejecutar_m_unlock(t_instruccion_decodificada* instr);
void ejecutar_exit(t_instruccion_decodificada* instr);
void esperar_a_poder_ejecutar(); 
void ejecutar_init_proc(t_instruccion_decodificada* instr);
void ejecutar_mem_alloc(t_instruccion_decodificada* instr);
void ejecutar_mem_free(instr);

int mmu(t_pcb* pcb, uint32_t dir_logica, uint32_t tamanio, bool* sf);

// variables globales
t_log* logger;
t_list* lista_sticks;  // lista global para guardar los memory sticks a los que me conecte

bool enviar_contexto_y_desalojar;
pthread_mutex_t m_enviar_contexto_y_desalojar;

bool v_pedido_de_desalojo;
pthread_cond_t cond_ejecutar;

bool v_pedir_segmentos;
pthread_mutex_t m_pedir_segmentos;

int pid_pendiente;
pthread_mutex_t m_pid_pendiente;

t_estado_cpu estado_cpu;
pthread_mutex_t m_estado_cpu;

// otros
int conexion_kernel_scheduler;
int conexion_kernel_memory;

int seg_max_size_global;
///////////////////Funciones ALAN///////////////////
void pedir_contexto(int pid, t_pcb* pcb);
void ciclo_instruccion(t_pcb *pcb);
char* fetch(t_pcb *pcb);
void decode(char *instruccion, t_pcb* pcb,  t_instruccion_decodificada * instruccion_decodificada);
bool execute(t_instruccion_decodificada *instruccion, t_pcb *pcb);
especificacion_registro * obtener_registro(char* registro_crudo, t_pcb *pcb);
void escribir_registro(especificacion_registro *reg, int valor);
uint32_t leer_registro(especificacion_registro* reg);
// void ejecutar_syscall(t_instruccion_decodificada *syscall, t_pcb* pcb);
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

    ip = config_get_string_value(config, "IP");
    
    // inicializar variables
    inicializar_variables();

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

    recibir_sticks_de_km();
    
    // hilo que ejecuta instrucciones:
    pthread_t hilo_ejecucion;
    pthread_create(&hilo_ejecucion, NULL, ejecutar, NULL);
    pthread_detach(hilo_ejecucion);

    // queda escuchando mensajes que envie el scheduler
    while (1){
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
        log_debug(logger, "RECIBI OP=%d", cod_op);
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
                log_info(logger, "Conexion de modulo stick");
                t_list * lista_del_paquete = recibir_paquete(conexion_kernel_scheduler);
                
                char* ip_stick = list_get(lista_del_paquete, 0);
                char* puerto_stick = list_get(lista_del_paquete, 1);
                int* tamanio = list_get(lista_del_paquete, 2); 
     
                // conectar a memory stick
                int conexion_memory_stick = crear_conexion(ip_stick, puerto_stick);
                exit_si_error_conexion(conexion_memory_stick, logger, "memory stick");
                handshake_cliente(conexion_memory_stick,logger);
                
                // guardar datos del stick en lista_sticks
                t_stick* stick = iniciar_stick(ip_stick, puerto_stick, *tamanio, conexion_memory_stick);
                list_add(lista_sticks, stick);
                
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
        case SCH_CPU__SYS_BLOQUEANTE:{
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
            set_enviar_contexto_y_desalojar(true);
            pthread_cond_signal(&cond_ejecutar);
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
            set_enviar_contexto_y_desalojar(true);
            pthread_cond_signal(&cond_ejecutar);
            break;
        }
        default:{}
    }
}

void transicionar(t_estado_cpu estado){
    estado_cpu = estado;
    log_debug(logger, "NUEVO ESTADO: %d", estado);
}

void transicionar_thread_safe(t_estado_cpu estado){
    pthread_mutex_lock(&m_estado_cpu);
    transicionar(estado);
    pthread_mutex_unlock(&m_estado_cpu);
}

void enviar_confirmacion(){
    set_pedir_segmentos(false);
    enviar_operacion(conexion_kernel_scheduler, CPU_SCH__EJECUCION_DETENIDA);
}

bool get_pedir_segmentos(){
    pthread_mutex_lock(&m_pedir_segmentos);
    bool r = v_pedir_segmentos;
    pthread_mutex_unlock(&m_pedir_segmentos);
    return r;
}

void set_pedir_segmentos(bool r){
    pthread_mutex_lock(&m_pedir_segmentos);
    v_pedir_segmentos = r;
    pthread_mutex_unlock(&m_pedir_segmentos);
}

bool get_enviar_contexto_y_desalojar(){
    pthread_mutex_lock(&m_enviar_contexto_y_desalojar);
    bool x = enviar_contexto_y_desalojar;
    pthread_mutex_unlock(&m_enviar_contexto_y_desalojar);
    return x;
}
void set_enviar_contexto_y_desalojar(bool x){
    pthread_mutex_lock(&m_enviar_contexto_y_desalojar);
    enviar_contexto_y_desalojar = x;
    pthread_mutex_unlock(&m_enviar_contexto_y_desalojar);
}

void esperar_a_poder_ejecutar(t_pcb* pcb){
    pthread_mutex_lock(&m_estado_cpu);
    if(v_pedido_de_desalojo){
        log_debug(logger, "Pedido de desalojo detectado, pasando a LIBRE");
        v_pedido_de_desalojo = false;
        transicionar(LIBRE);
        enviar_confirmacion();
    }
    while(estado_cpu != EXEC){
        if(v_pedido_de_desalojo){
            log_debug(logger, "Pedido de desalojo detectado, pasando a LIBRE");
            v_pedido_de_desalojo = false;
            transicionar(LIBRE);
            enviar_confirmacion();
        }
        if(get_enviar_contexto_y_desalojar()){
            enviar_pcb_actualizado_a_km(pcb);
            transicionar_thread_safe(LIBRE);
            enviar_confirmacion();
            set_enviar_contexto_y_desalojar(false);
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
    enviar_operacion(conexion_memory_stick, CPU_STICK__CONEXION);
    // agregar a lista local
    list_add(lista_sticks, stick);
}

void recibir_sticks_de_km(){
    op_code cod_op = recibir_operacion(conexion_kernel_memory);
    if(cod_op != KM_CPU__STICKS){
        log_error(logger, "Error: Kernel Memory no envio las sticks");
        exit(EXIT_FAILURE);
    }
    t_list* paquete = recibir_paquete(conexion_kernel_memory);
    int desplazamiento = 0;
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
    log_debug(logger, "recibiendo pcb");
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
        log_debug(logger, "Recibiendo segmento de id: %d", seg_recibido->id_segmento);
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
        log_debug(logger, "Recibiendo segmento de id: %d", seg_recibido->id_segmento);
        list_add(pcb->tabla_segmentos, seg_recibido);
    }
    list_destroy_and_destroy_elements(lista_contexto, free);
}

void enviar_pcb_actualizado_a_km(t_pcb* pcb){
    if(pcb == NULL){
        return;
    }
    log_debug(logger, "<%d> Enviando contexto actualizado a KM, proximo pc: %d", pcb->pid, pcb->registros.pc);
    t_paquete* p = crear_paquete(CPU_KM__ACTUALIZAR_PCB);
    agregar_pcb_al_paquete(pcb, p);

    enviar_paquete_y_liberarlo(p, conexion_kernel_memory);

    recibir_operacion(conexion_kernel_memory);
}

void agregar_pcb_al_paquete(t_pcb* pcb, t_paquete* p){
    // datos del pcb
    agregar_a_paquete(p, &pcb->pid, sizeof(pcb->pid));
    agregar_a_paquete(p, &pcb->ppid, sizeof(pcb->ppid));
    agregar_a_paquete(p, &pcb->priodidad, sizeof(pcb->priodidad));

    // registros
    agregar_a_paquete(p, &pcb->registros.pc, sizeof(pcb->registros.pc));

    agregar_a_paquete(p, &pcb->registros.ax, sizeof(pcb->registros.ax));
    agregar_a_paquete(p, &pcb->registros.bx, sizeof(pcb->registros.bx));
    agregar_a_paquete(p, &pcb->registros.cx, sizeof(pcb->registros.cx));
    agregar_a_paquete(p, &pcb->registros.dx, sizeof(pcb->registros.dx));

    agregar_a_paquete(p, &pcb->registros.eax, sizeof(pcb->registros.eax));
    agregar_a_paquete(p, &pcb->registros.ebx, sizeof(pcb->registros.ebx));
    agregar_a_paquete(p, &pcb->registros.ecx, sizeof(pcb->registros.ecx));
    agregar_a_paquete(p, &pcb->registros.edx, sizeof(pcb->registros.edx));

    agregar_a_paquete(p, &pcb->registros.si, sizeof(pcb->registros.si));
    agregar_a_paquete(p, &pcb->registros.di, sizeof(pcb->registros.di));

}




// ===================== Ciclo de instruccion =====================
void* ejecutar(){
    t_instruccion_decodificada* prox_instruccion;
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
        bool actualizar_contexto = execute(instruccion_decodificada, pcb);
        
        if(pcb->registros.pc == pc_antiguo)
            pcb->registros.pc++;

        if(actualizar_contexto){
            enviar_pcb_actualizado_a_km(pcb);
        } 
        list_destroy_and_destroy_elements(instruccion_decodificada->registros, free);
        free(instruccion_decodificada);
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
    string_array_destroy(partes);
    return;
}

bool execute(t_instruccion_decodificada * instruccion, t_pcb *pcb){
    switch (instruccion->tipo){
        case I_NOOP:
            break;
        case I_SET://Asigna al registro el valor pasado como parámetro. SET ax 5
            especificacion_registro* reg =list_get(instruccion->registros,0);
            int *input  =list_get(instruccion->registros,1);
            log_info(logger, "antes de set, valor: %p",reg->ptro_reg);
            escribir_registro(reg, *input);
            log_info(logger, "despues de set, valor: %p",reg->ptro_reg);
            break;
        case I_SUM:
            log_info(logger, "Ejecutando instruccion JNZ");
            especificacion_registro *destino = list_get(instruccion->registros,0);
            especificacion_registro *origen = list_get(instruccion->registros,1);
            uint32_t suma =leer_registro(destino)+leer_registro(origen);
            escribir_registro(destino, suma);
            break;
        case I_SUB:
            log_info(logger, "Ejecutando instruccion SUB");
            especificacion_registro *minuendo = list_get(instruccion->registros,0);
            especificacion_registro *sustraendo = list_get(instruccion->registros,1);
            uint32_t resta =leer_registro(minuendo)-leer_registro(sustraendo);
            escribir_registro(minuendo, resta);
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
            break;
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
            ejecutar_exit(instruccion);
            return true;
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

// syscalls: 
void ejecutar_mem_free(t_instruccion_decodificada* instr){
    set_pedir_segmentos(true);
    transicionar_thread_safe(WAIT_SYS);
    int id_segmento = *(int*)list_get(instr->registros, 0);
    log_debug(logger, "Ejecutando MEM_FREE %d", id_segmento);
    t_paquete* paquete = crear_paquete(CPU_SCH__MEM_FREE);
    agregar_a_paquete(paquete, &id_segmento, sizeof(id_segmento));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_mem_alloc(t_instruccion_decodificada* instr){
    set_pedir_segmentos(true);
    transicionar_thread_safe(WAIT_MEM_ALLOC);
    int id_segmento = *(int*)list_get(instr->registros, 0);
    int tamanio = *(int*)list_get(instr->registros, 1);
    log_debug(logger, "Ejecutando MEM_ALLOC %d %d", id_segmento, tamanio);
    t_paquete* paquete = crear_paquete(CPU_SCH__MEM_ALLOC);
    agregar_a_paquete(paquete, &id_segmento, sizeof(id_segmento));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_init_proc(t_instruccion_decodificada* instr){
    log_debug(logger, "Ejecutando INIT_PROC");
    transicionar_thread_safe(WAIT_SYS);
    char* ruta_archivo_instrucciones = list_get(instr->registros, 0);
    int prioridad = *(int*)list_get(instr->registros, 1);
    t_paquete* paquete = crear_paquete(CPU_SCH__INIT_PROC);
    agregar_string_a_paquete(paquete, ruta_archivo_instrucciones);
    agregar_a_paquete(paquete, &prioridad, sizeof(prioridad));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_exit(t_instruccion_decodificada* instr){
    log_debug(logger, "Ejecutando EXIT");
    transicionar_thread_safe(LIBRE);
    enviar_operacion(conexion_kernel_scheduler, CPU_SCH__EXIT);
}

void ejecutar_m_unlock(t_instruccion_decodificada* instr){
    transicionar_thread_safe(WAIT_SYS);
    char* nombre = (char*)list_get(instr->registros, 0);
    log_debug(logger, "Ejecutando MUTEX_UNLOCK %s", nombre);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_UNLOCK);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_m_lock(t_instruccion_decodificada* instr){
    transicionar_thread_safe(WAIT_SYS);
    char * nombre = list_get(instr->registros, 0);
    log_debug(logger, "Ejecutando MUTEX_LOCK %s", nombre);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_LOCK);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_m_create(t_instruccion_decodificada* instr){
    transicionar_thread_safe(WAIT_SYS);
    char * nombre = list_get(instr->registros, 0);
    log_debug(logger, "Ejecutando MUTEX_CREATE %d", *nombre);
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
    bool seg_fault = false;
    int base = mmu(pcb, dir_logica , tamanio, &seg_fault); 
    log_info(logger, "Direccion fisica: %d", base);
    t_paquete* paquete = crear_paquete(CPU_SCH__STDOUT);
    agregar_a_paquete(paquete, &base, sizeof(base));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_stdin(t_instruccion_decodificada* instr, t_pcb* pcb){
    transicionar_thread_safe(LIBRE);
    especificacion_registro *param1 = list_get(instr->registros,0);
    especificacion_registro *param2 = list_get(instr->registros,1);
    uint32_t dir_logica = leer_registro(param1);
    uint32_t tamanio = leer_registro(param2);
    bool seg_fault = false;
    int base = mmu(pcb, dir_logica, tamanio, &seg_fault); 
    
    log_info(logger, "Direccion fisica: %d", base);
    t_paquete* paquete = crear_paquete(CPU_SCH__STDIN);
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    agregar_a_paquete(paquete, &base, sizeof(base));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_sleep(t_instruccion_decodificada* instruccion){
    transicionar_thread_safe(LIBRE);
    int tiempo_sleep = *(int*)list_get(instruccion->registros, 0);
    log_debug(logger, "Ejecutando SLEEP %d", tiempo_sleep);
    t_paquete* paquete = crear_paquete(CPU_SCH__SLEEP);
    agregar_a_paquete(paquete, &tiempo_sleep, sizeof(tiempo_sleep));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
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

// ======== MMU ========


//////////////////////////////FUNCIOES NIKI ////////////////////////////////////////////////////////
int mmu(t_pcb* pcb, uint32_t dir_logica, uint32_t tamanio, bool* sf) {
    *sf = false;
    uint32_t num_seg = floor(dir_logica / seg_max_size_global);
    uint32_t desp    = dir_logica % seg_max_size_global;

    t_segmento* seg = NULL;
    for (int i = 0; i < list_size(pcb->tabla_segmentos); i++) {
        t_segmento* s = list_get(pcb->tabla_segmentos, i);
        if ((uint32_t)s->id_segmento == num_seg) { seg = s; break; }
    }
    if (!seg || desp > seg->tamanio) { *sf = true; return -1; }
    log_info(logger, "desp: %d, tamanio segmento: %d", desp, seg->tamanio);

    void*    buffer = malloc(tamanio);
    uint32_t dir_fisica_abs = seg->base + desp;
    uint32_t pendientes = tamanio, offset = dir_fisica_abs;
    uint32_t buf_desp = 0, acum = 0;
    return dir_fisica_abs;
    /*
 
    for (int i = 0; i < list_size(lista_sticks) && pendientes > 0; i++) {
        t_stick* stick = list_get(lista_sticks, i);
        if (offset >= acum + (uint32_t)stick->tamanio) { acum += stick->tamanio; continue; }

        uint32_t dir_en_stick   = offset - acum;
        uint32_t bytes_en_stick = (acum + stick->tamanio) - offset;
        if (bytes_en_stick > pendientes) bytes_en_stick = pendientes;

        t_paquete* p = crear_paquete(X_STICK__LECTURA);
        agregar_a_paquete(p, &dir_en_stick,   sizeof(uint32_t));
        agregar_a_paquete(p, &bytes_en_stick, sizeof(uint32_t));
        enviar_paquete_y_liberarlo(p, stick->fd);

        log_info(logger, "PID: %d - Acción: LEER - Dirección Física: %u - Valor: (%u bytes)",
                 pcb->pid, dir_en_stick, bytes_en_stick);

        recibir_operacion(stick->fd); // STICK_X__OK
        t_list* resp = recibir_paquete(stick->fd);
        memcpy(buffer + buf_desp, list_get(resp, 0), bytes_en_stick);
        list_destroy_and_destroy_elements(resp, free);

        offset    += bytes_en_stick;
        buf_desp  += bytes_en_stick;
        pendientes -= bytes_en_stick;
        acum      += stick->tamanio;
    }
    return buffer;
}

void mmu_escribir(t_pcb* pcb, uint32_t dir_logica, void* datos, uint32_t tamanio, bool* sf) {
    *sf = false;
    uint32_t num_seg = dir_logica / seg_max_size_global;
    uint32_t desp    = dir_logica % seg_max_size_global;

    t_segmento* seg = NULL;
    for (int i = 0; i < list_size(pcb->tabla_segmentos); i++) {
        t_segmento* s = list_get(pcb->tabla_segmentos, i);
        if ((uint32_t)s->id_segmento == num_seg) { seg = s; break; }
    }
    if (!seg || desp + tamanio > seg->limite) { *sf = true; return; }

    uint32_t dir_fisica_abs = seg->base + desp;
    uint32_t pendientes = tamanio, offset = dir_fisica_abs;
    uint32_t buf_desp = 0, acum = 0;

    for (int i = 0; i < list_size(lista_sticks) && pendientes > 0; i++) {
        t_stick* stick = list_get(lista_sticks, i);
        if (offset >= acum + (uint32_t)stick->tamanio) { acum += stick->tamanio; continue; }

        uint32_t dir_en_stick   = offset - acum;
        uint32_t bytes_en_stick = (acum + stick->tamanio) - offset;
        if (bytes_en_stick > pendientes) bytes_en_stick = pendientes;

        t_paquete* p = crear_paquete(X_STICK__ESCRITURA);
        agregar_a_paquete(p, &dir_en_stick,    sizeof(uint32_t));
        agregar_a_paquete(p, datos + buf_desp, bytes_en_stick);
        enviar_paquete_y_liberarlo(p, stick->fd);

        log_info(logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %u - Valor: (%u bytes)",
                 pcb->pid, dir_en_stick, bytes_en_stick);

        recibir_operacion(stick->fd); // STICK_X__OK

        offset    += bytes_en_stick;
        buf_desp  += bytes_en_stick;
        pendientes -= bytes_en_stick;
        acum      += stick->tamanio;
    }  */
}

////////////////////////////////////////////////////////////////////////////



// OTROS

void inicializar_variables(){
    v_pedido_de_desalojo = false;
    pid_pendiente = -1;
    lista_sticks = list_create();
    pthread_cond_init(&cond_ejecutar, NULL);
    pthread_mutex_init(&m_estado_cpu, NULL);
    transicionar(LIBRE);
    seg_max_size_global = 255;
    v_pedir_segmentos = false;
    pthread_mutex_init(&m_pedir_segmentos, NULL);
    enviar_contexto_y_desalojar = false;
    pthread_mutex_init(&m_enviar_contexto_y_desalojar, NULL);

}