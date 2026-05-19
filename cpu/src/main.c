#include <utils/hello.h>
#include <utils/utils.h>
#include "base_cpu.h"
#include <semaphore.h>

// variables globales
t_log* logger;

t_list* lista_sticks; // lista global para guardar los memory sticks a los que me conecte

bool ejecutar;
pthread_cond_t cond_ejecutar;
pthread_mutex_t m_ejecutar;

int pid_pendiente;
pthread_mutex_t m_pid_pendiente;
//////////////////
//void* ejecutar();
//void detener_ejecucion();
//void reanudar_ejecucion();
//void inicializar_variables();
//t_pcb* inicializar_pcb(int pid);
//t_instruccion* crear_instruccion(t_tipo_instruccion tipo, char* param1, char* param2);
//t_instruccion* proxima_instruccion();
//void ejecutar_sleep(t_instruccion* instr);
//void ejecutar_m_create(t_instruccion* instr);
//void ejecutar_m_lock(t_instruccion* instr);
//void ejecutar_m_unlock(t_instruccion* instr);
//void esperar_a_poder_ejecutar();
////////////////////

void registro_destroy(void* ptr);

void conectarse_a_stick(char* ip, char* puerto);
t_pcb* pedir_contexto(int pid, int conexion_kernel_memory);
void ciclo_instruccion(t_pcb *pcb, int conexion_kernel_memory, int conexion_kernel_scheduler);
char* fetch(t_pcb *pcb, int conexion_kernel_memory);
void decode(char *instruccion, t_pcb* pcb,  t_instruccion_decodificada * instruccion_decodificada);
bool execute(t_instruccion_decodificada *instruccion, t_pcb *pcb, int conexion_kernel_scheduler);
especificacion_registro * obtener_registro(char* registro_crudo, t_pcb *pcb);
void escribir_registro(especificacion_registro *reg, int valor);
uint32_t leer_registro(especificacion_registro* reg);
void ejecutar_syscall(t_syscall syscall, t_pcb* pcb, int conexion_scheduler);


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
    logger = iniciar_logger("cpu.log", "ProcesoCPU", LOG_LEVEL_INFO);

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
    int conexion_kernel_scheduler = crear_conexion(ip, puerto_kernel_scheduler);
    exit_si_error_conexion(conexion_kernel_scheduler, logger, "kernel scheduler");
    handshake_cliente(conexion_kernel_scheduler,logger);
    
    // enviar info propia a sch:
    t_paquete *paquete_conexion_sch = crear_paquete(CPU_SCH__CONEXION);
    agregar_a_paquete(paquete_conexion_sch, &id_cpu, sizeof(id_cpu));
    enviar_paquete_y_liberarlo(paquete_conexion_sch, conexion_kernel_scheduler);

    // conectar a kernel memory
    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    handshake_cliente(conexion_kernel_memory,logger);
        
    //mando informacion propia al km
    t_paquete *paquete_conexion_km = crear_paquete(CPU_KM__CONEXION);
    agregar_a_paquete(paquete_conexion_km, &id_cpu, sizeof(id_cpu));
    enviar_paquete_y_liberarlo(paquete_conexion_km, conexion_kernel_memory);

    // inicializar listas
    lista_sticks = list_create();


    // queda escuchando mensajes que envie el scheduler
    while (1)
    {
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
        if(cod_op == -1){
            log_error(logger, "Error: se desconecto scheduler"); 
            break; 
        }
        switch(cod_op){
            case SCH_CPU__PID:{
                log_info(logger, "me llego un PID");
                t_list *lista = recibir_paquete(conexion_kernel_scheduler);
                int *pid = list_get(lista, 0);
                ejecutar = true;
                t_pcb *pcb = pedir_contexto(*pid, conexion_kernel_memory);
                ciclo_instruccion(pcb, conexion_kernel_memory, conexion_kernel_scheduler);

                list_destroy_and_destroy_elements(lista, free);
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

                /*
                1. A través de variables globales: Cualquier hilo puede acceder a ellas.

                2. Pasando punteros al crearlos: Cuando usas pthread_create, el último parámetro es un void* que te permite 
                pasarle cualquier estructura de datos (como el t_stick de tu ejemplo anterior) al nuevo hilo.
                */
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

void conectarse_a_stick(char* ip, char* puerto){
    int conexion_memory_stick = crear_conexion(ip, puerto);
    exit_si_error_conexion(conexion_memory_stick, logger, "memory stick");
    handshake_cliente(conexion_memory_stick, logger);
    
}

t_pcb* pedir_contexto(int pid, int conexion_kernel_memory){

   // enviar_operacion(conexion_kernel_memory, CPU_KM__PCONTEXTO);
    t_paquete *paquete = crear_paquete(CPU_KM__PCONTEXTO);

    agregar_a_paquete(paquete, &pid, sizeof(int));

    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    
    op_code cod_op = recibir_operacion(conexion_kernel_memory);

    if(cod_op != KM_CPU__CONTEXTO) {
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

void ciclo_instruccion(t_pcb *pcb, int conexion_kernel_memory, int conexion_kernel_scheduler){
    while(1){

        //FECH
        char *instruccion = fetch(pcb, conexion_kernel_memory);
        if(instruccion == NULL){
        log_error(logger, "FETCH devolvio NULL");
        break;
        }
        
        //DECODE
        t_instruccion_decodificada * instruccion_decodificada = malloc(sizeof(t_instruccion_decodificada));
        decode(instruccion, pcb, instruccion_decodificada);
        log_info(logger, "pc %i", pcb->registros.pc);
        log_info( logger, "Instruccion: %s", instruccion);
        int pc_antiguo = pcb->registros.pc;
        //execute
        ejecutar = execute(instruccion_decodificada, pcb, conexion_kernel_scheduler);
        if(!ejecutar)
            break;
        if(pcb->registros.pc == pc_antiguo)
            pcb->registros.pc++;

        list_destroy(instruccion_decodificada->registros);
        free(instruccion_decodificada);
        free(instruccion);
        //list_destroy_and_destroy_elements(instruccion_decodificada->registros, registro_destroy);
        //free(i_decodificada);
        
    }
      
}


char* fetch(t_pcb *pcb, int conexion_kernel_memory){

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
    else if(string_equals_ignore_case(partes[0], "EXIT")){
        instruccion_decodificada->tipo = I_EXIT;
        string_array_destroy(partes);
        return;
    }

    string_array_destroy(partes);
    return;
}

bool execute(t_instruccion_decodificada * instruccion, t_pcb *pcb, int conexion_kernel_scheduler){


    switch (instruccion->tipo){
    case I_NOOP:
    break;
    case I_SET://Asigna al registro el valor pasado como parámetro. SET ax 5

        especificacion_registro* reg =list_get(instruccion->registros,0);
        int *input  =list_get(instruccion->registros,1);
        escribir_registro(reg, *input);
        free(reg);
        free(input);
        break;
    case I_SUM:
        especificacion_registro *destino = list_get(instruccion->registros,0);
        especificacion_registro *origen = list_get(instruccion->registros,1);
        uint32_t suma =leer_registro(destino)+leer_registro(origen);
        escribir_registro(destino, suma);
        free(origen);
        free(destino);
        break;
    case I_SUB:
        especificacion_registro *minuendo = list_get(instruccion->registros,0);
        especificacion_registro *sustraendo = list_get(instruccion->registros,1);
        uint32_t resta =leer_registro(minuendo)-leer_registro(sustraendo);
        escribir_registro(minuendo, resta);
        free(minuendo);
        free(sustraendo);
        log_info(logger, "hice sub");
        break;
    case I_JNZ:
        especificacion_registro* reg_ = list_get(instruccion->registros,0);
        int* nuevo_pc =list_get(instruccion->registros,1);
        log_info(logger, "pc %i", pcb->registros.pc);
        if(leer_registro(reg_) != 0)
            pcb->registros.pc = *nuevo_pc;
        log_info(logger, "pc %i", pcb->registros.pc);
        free(reg_);
        free(nuevo_pc);
        break;
        case I_EXIT:
            ejecutar_syscall(SYSCALL_EXIT, pcb, conexion_kernel_scheduler);
            return false;
        break;
    default:
        break;
    }
    return true;
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

void ejecutar_syscall(t_syscall tipo, t_pcb* pcb, int conexion_scheduler){
    t_paquete* paquete = crear_paquete(CPU_SCH__SYSCALL);
    agregar_a_paquete(paquete, &(pcb->pid), sizeof(int));
    agregar_a_paquete(paquete, &tipo, sizeof(int));
    agregar_a_paquete(paquete,&(pcb->registros),sizeof(t_registros));
    enviar_paquete_y_liberarlo(paquete,conexion_scheduler);
    log_info(logger,"PID %d enviado por syscall",pcb->pid);
}

void registro_destroy(void* ptr) {
    t_instruccion_decodificada * registro = (t_instruccion_decodificada*) ptr;
    //free(registro->registros->elements_count);
    free(registro->registros);
}

//void inicializar_variables(){
//    execute = false;
//    pid_pendiente = -1;
//    lista_sticks = list_create();
//    lista_instrucciones = list_create();
//    list_add(lista_instrucciones, crear_instruccion(INST_SLEEP, "2000", NULL));
//    list_add(lista_instrucciones, crear_instruccion(INST_SLEEP, "3500", NULL));
//}

