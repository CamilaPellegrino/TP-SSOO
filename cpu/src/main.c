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

void* atender_stick(void* arg);
void conectarse_a_stick(char* ip, char* puerto, uint32_t tamanio);
void recibir_sticks_de_km();
void* ejecutar();
void inicializar_variables();
void enviar_pcb_actualizado_a_km(t_pcb* pcb);
void agregar_pcb_al_paquete(t_pcb* pcb, t_paquete* p);

void ejecutar_jnz(t_instruccion_decodificada *instruccion, t_pcb *pcb);
void ejecutar_sub(t_instruccion_decodificada *instruccion, t_pcb *pcb);
void ejecutar_sum(t_instruccion_decodificada *instruccion, t_pcb *pcb);
void ejecutar_set(t_instruccion_decodificada *instruccion, t_pcb *pcb);

void ejecutar_sleep(t_instruccion_decodificada* instr);
void ejecutar_stdin(t_instruccion_decodificada* instr, t_pcb*);
void ejecutar_stdout(t_instruccion_decodificada* instr, t_pcb*);
void ejecutar_m_create(t_instruccion_decodificada* instr);
void ejecutar_m_lock(t_instruccion_decodificada* instr);
void ejecutar_m_unlock(t_instruccion_decodificada* instr);
void ejecutar_exit(t_instruccion_decodificada* instr, t_pcb* pcb);
void esperar_a_poder_ejecutar(t_pcb* pcb); 
void ejecutar_init_proc(t_instruccion_decodificada* instr);
void ejecutar_copy_mem(t_instruccion_decodificada* instruccion, t_pcb* pcb);
void ejecutar_mov_in(t_instruccion_decodificada* instr, t_pcb* pcb);
void ejecutar_mov_out(t_instruccion_decodificada* instr, t_pcb* pcb);
void ejecutar_mem_alloc(t_instruccion_decodificada* instr);
void ejecutar_mem_free(t_instruccion_decodificada* instr);

void destruir_instruccion(t_instruccion_decodificada*);
void manejar_seg_fault(t_pcb* pcb);
int mmu(t_pcb* pcb, uint32_t, uint32_t, uint32_t);
uint32_t calc_id_segmento(int);
uint32_t calc_offset(int);
void iniciar_instr_exit(t_instruccion_decodificada* instr, t_status_op s);
t_dir_fisica iniciar_dir_fisica(int id_segmento, int offset);

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
void *registro_a_bytes(especificacion_registro *reg);
uint32_t leer_registro(especificacion_registro* reg);
// void ejecutar_syscall(t_instruccion_decodificada *syscall, t_pcb* pcb);
//////////////////////////
int id_cpu;
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

void enviar_pcb_actualizado_a_km(t_pcb* pcb){
    if(pcb == NULL){
        return;
    }
    log_debug(logger, "<%d> Enviando contexto actualizado a KM, proximo pc: %d", pcb->pid, pcb->registros.pc);
    t_paquete* p = crear_paquete(CPU_KM__ACTUALIZAR_PCB);
    agregar_pcb_al_paquete(pcb, p);

    enviar_paquete_y_liberarlo(p, conexion_kernel_memory);

    op_code cod_op = recibir_operacion(conexion_kernel_memory);
    log_debug(logger, "PCB cargado en km, op=%d", cod_op);
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
            t_instruccion_decodificada * i = malloc(sizeof(t_instruccion_decodificada));
            i->registros = list_create();
            iniciar_instr_exit(i, INSTRUCCION_INVALIDA);
            ejecutar_exit(i, pcb);
            destruir_instruccion(i);
            return;
        }
        int pid = pcb->pid;
        log_info(logger, "## PID: <%d> - FETCH - Program Counter: <%d>", pid, pcb->registros.pc);
        
        //DECODE
        t_instruccion_decodificada * instruccion_decodificada = malloc(sizeof(t_instruccion_decodificada));
        decode(instruccion, pcb, instruccion_decodificada);
        int pc_antiguo = pcb->registros.pc;
        //execute
        bool actualizar_contexto = execute(instruccion_decodificada, pcb);
        log_info(logger, "## PID: <%d> - Ejecutando: <%s>", pid, instruccion);
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

void decode(char *instruccion, t_pcb *pcb,  t_instruccion_decodificada * instruccion_decodificada){

    instruccion_decodificada->registros =  list_create();
    
    if(instruccion == NULL){
        log_error(logger, "Instruccion NULL");
        return;
    }
    char ** partes = string_split(instruccion, " ");
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
    else if(string_equals_ignore_case(partes[0], "MOV_OUT")){
        instruccion_decodificada->tipo = I_MOV_OUT;
        especificacion_registro* parametro_aux = obtener_registro(partes[1], pcb);
        list_add(instruccion_decodificada->registros, parametro_aux);
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "MOV_IN")){
        instruccion_decodificada->tipo = I_MOV_IN;
        especificacion_registro* parametro_aux = obtener_registro(partes[1], pcb);
        list_add(instruccion_decodificada->registros, parametro_aux);
        string_array_destroy(partes);
        return;
    }
    else if(string_equals_ignore_case(partes[0], "EXIT")){
        iniciar_instr_exit(instruccion_decodificada, OK);
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
    else if(string_equals_ignore_case(partes[0], "COPY_MEM")){
        instruccion_decodificada->tipo = I_COPY_MEM;
        especificacion_registro* reg_tam = obtener_registro(partes[1], pcb);
        list_add(instruccion_decodificada->registros, reg_tam);
        string_array_destroy(partes);
        return;
    }else{
        iniciar_instr_exit(instruccion_decodificada, INSTRUCCION_INVALIDA);
    }
    string_array_destroy(partes);
    return;
}

bool execute(t_instruccion_decodificada * instruccion, t_pcb *pcb){
    switch (instruccion->tipo){
        case I_NOOP:
            break;
        case I_SET://Asigna al registro el valor pasado como parámetro. SET ax 5
            ejecutar_set(instruccion, pcb);
            break;
        case I_SUM:
            ejecutar_sum(instruccion, pcb);
            break;
        case I_SUB:
            ejecutar_sub(instruccion, pcb);
            break;
        case I_JNZ:
            ejecutar_jnz(instruccion, pcb);
            break;
        case I_COPY_MEM:
            ejecutar_copy_mem(instruccion, pcb);
            break;
        case I_MOV_OUT:{
            ejecutar_mov_out(instruccion, pcb);
            break;
        }
        case I_MOV_IN:{
            ejecutar_mov_in(instruccion, pcb);
            break;
        }
///////////////////////////syscalls//////////////////
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
            ejecutar_exit(instruccion, pcb);
            break;
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
    uint8_t valor = *(uint8_t*)list_get(data, 0);
    log_info(logger, "PID: <%d> - Acción: <LEER> - Dirección Física: <%d> - Valor: <%d>", pcb->pid, dir_fisica_stick, valor);
    escribir_registro(r_datos, valor);
}

void ejecutar_mov_out(t_instruccion_decodificada* instr, t_pcb* pcb){
    especificacion_registro* r_datos = list_get(instr->registros, 0);
    void* datos = registro_a_bytes(r_datos);
    uint32_t dir_logica = pcb->registros.di;
    int id_segmento = calc_id_segmento(dir_logica);
    int offset = calc_offset(dir_logica);
    int tamanio = r_datos->tamanio;
    int dir_fisica_stick = mmu(pcb, tamanio, id_segmento, offset);
    if(dir_fisica_stick == -1){
        manejar_seg_fault(pcb);
        return;
    }
    t_dir_fisica dir_fisica = iniciar_dir_fisica(id_segmento, offset);

    log_info(logger, "PID: <%d> - Acción: <ESCRIBIR> - Dirección Física: <%d> - Valor: <%s>", pcb->pid, dir_fisica_stick, (char*)datos);
    t_paquete* paquete =  crear_paquete(CPU_KM__MOV_OUT);
    agregar_a_paquete(paquete, &dir_fisica, sizeof(dir_fisica));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    agregar_a_paquete(paquete, datos, tamanio);
    agregar_a_paquete(paquete, &pcb->pid, sizeof(pcb->pid));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    recibir_operacion(conexion_kernel_memory);
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
    log_info(logger, "<%d> EXIT[%s]", pcb->pid, status_op_a_string(status));
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
void *registro_a_bytes(especificacion_registro *reg){
    uint8_t *bytes = malloc(reg->tamanio);
    if (bytes == NULL)
        return NULL;

    if (reg->tamanio == sizeof(uint8_t)) {
        bytes[0] = *(uint8_t *)reg->ptro_reg;
    } else if (reg->tamanio == sizeof(uint32_t)) {
        uint32_t valor = *(uint32_t *)reg->ptro_reg;

        bytes[0] = (valor >> 24) & 0xFF;
        bytes[1] = (valor >> 16) & 0xFF;
        bytes[2] = (valor >> 8)  & 0xFF;
        bytes[3] =  valor        & 0xFF;
    }

    return bytes;
}
// ======== MMU ========

uint32_t calc_id_segmento(int dir_logica){
    return floor(dir_logica / seg_max_size_global);
}
uint32_t calc_offset(int dir_logica){
    return floor(dir_logica % seg_max_size_global);
}

int mmu(t_pcb* pcb, uint32_t tamanio, uint32_t num_seg, uint32_t desp) {
    t_segmento* seg = NULL;

    for (int i = 0; i < list_size(pcb->tabla_segmentos); i++) {
        t_segmento* s = list_get(pcb->tabla_segmentos, i);
        if ((uint32_t)s->id_segmento == num_seg) {
            seg = s;
            break;
        }
    }

    if (seg == NULL ||
        desp > seg->tamanio ||
        desp + tamanio > seg->tamanio) {
        return -1;
    }

    return (int32_t)(seg->base + desp);
}

// ======== OTROS ========

void manejar_seg_fault(t_pcb* pcb){
    log_debug(logger, "<%d> ERROR: seg_fault", pcb->pid);
    t_instruccion_decodificada* i = malloc(sizeof(t_instruccion_decodificada));
    i->registros = list_create();
    iniciar_instr_exit(i, SEG_FAULT);
    ejecutar_exit(i, pcb);
    destruir_instruccion(i);
}

void iniciar_instr_exit(t_instruccion_decodificada* instr, t_status_op s){
    log_debug(logger, "En exit");
    instr->tipo = I_EXIT;
    t_status_op* status = malloc(sizeof(t_status_op)); 
    *status = s;
    list_add(instr->registros, status);
}

t_dir_fisica iniciar_dir_fisica(int id_segmento, int offset){
    t_dir_fisica dir;
    dir.id_segmento = id_segmento;
    dir.offset = offset;
    return dir;

}

void destruir_instruccion(t_instruccion_decodificada*i){
    list_destroy_and_destroy_elements(i->registros, free);
    free(i);
}
void inicializar_variables(){
    v_pedido_de_desalojo = false;
    pid_pendiente = -1;
    lista_sticks = list_create();
    pthread_cond_init(&cond_ejecutar, NULL);
    pthread_mutex_init(&m_estado_cpu, NULL);
    transicionar(LIBRE);
    seg_max_size_global = 256;
    v_pedir_segmentos = false;
    pthread_mutex_init(&m_pedir_segmentos, NULL);
    enviar_contexto_y_desalojar = false;
    pthread_mutex_init(&m_enviar_contexto_y_desalojar, NULL);

}