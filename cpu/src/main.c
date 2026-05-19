#include <utils/hello.h>
#include <utils/utils.h>
#include "base_cpu.h"
#include <semaphore.h>

void* ejecutar();
void detener_ejecucion();
void reanudar_ejecucion();
void inicializar_variables();
t_pcb* inicializar_pcb(int pid);
t_instruccion* crear_instruccion(t_tipo_instruccion tipo, char* param1, char* param2);
t_instruccion* proxima_instruccion();
void ejecutar_sleep(t_instruccion* instr);
void ejecutar_stdin(t_instruccion* instr);
void ejecutar_stdout(t_instruccion* instr);
void ejecutar_m_create(t_instruccion* instr);
void ejecutar_m_lock(t_instruccion* instr);
void ejecutar_m_unlock(t_instruccion* instr);
void esperar_a_poder_ejecutar(); 

// variables globales
t_log* logger;
t_list* lista_instrucciones; // lista hardcodeada de instruciones para testear sch
t_list* lista_sticks;  // lista global para guardar los memory sticks a los que me conecte

// identificadores del proceso
int pid;
int ppid;
// registros de estado
uint32_t pc;
uint8_t ax;
uint8_t bx;
uint8_t cx;
uint8_t dx;
uint32_t eax;
uint32_t ebx;
uint32_t ecx;
uint32_t edx;
uint32_t si;
uint32_t di;

bool execute;
pthread_cond_t cond_ejecutar;
pthread_mutex_t m_ejecutar;

int pid_pendiente;
pthread_mutex_t m_pid_pendiente;

// otros
int conexion_kernel_scheduler;
int conexion_kernel_memory;

void conectarse_a_stick(char* ip, char* puerto);

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
    t_instruccion* prox_instruccion;

    while(1){
        log_debug(logger, "ejecutar: esperando a poder ejecutar");
        esperar_a_poder_ejecutar(); 
        log_debug(logger, "ejecutar: execute vale true");
        pthread_mutex_lock(&m_pid_pendiente);

        if(pid_pendiente >= 0){ // significa que tengo que cambiar de proceso
            // reemplazar pcb actual por el pendiente (recibirlo de km)
            // ...
            log_debug(logger, "ejecutar: pidiendo nuevo pcb de pid %d a km y cargandolo", pid_pendiente);
            pid_pendiente = -1;
        }
        pthread_mutex_unlock(&m_pid_pendiente);

        log_debug(logger, "ejecutar: buscando prox instruccion");
        prox_instruccion = proxima_instruccion();
        if(prox_instruccion == NULL){ // puede pasar esto?
            log_debug(logger, "en ejecutar: prox_instruccion valia NULL");
            detener_ejecucion();
            continue;
        };
        log_debug(logger, "ejecutar: proxima instruccion de tipo: %d, pc = %d", prox_instruccion->tipo, pc);

        pc++; // cuando haya un salto no se deberia aumentar pc

        switch(prox_instruccion->tipo){ // falta implemenetar todas las instrucciones, dejo 2 syscalls hechas para probar sch
            case INST_SLEEP:
                ejecutar_sleep(prox_instruccion);
                break;
            case INST_MUTEX_CREATE:
                ejecutar_m_create(prox_instruccion);
                break;
            case INST_MUTEX_LOCK:
                ejecutar_m_lock(prox_instruccion);
                break;
            case INST_MUTEX_UNLOCK:
                ejecutar_m_unlock(prox_instruccion);
                break;
            case INST_STDIN:
                ejecutar_stdin(prox_instruccion);
                break;
            case INST_STDOUT:
                ejecutar_stdout(prox_instruccion);
                break;
            default:
                log_warning(logger, "Instruccion no implementada, tipo %d", prox_instruccion->tipo);
                break;
        }
    }
    return NULL;
}

void esperar_a_poder_ejecutar(){
    pthread_mutex_lock(&m_ejecutar);
    while(!execute){
        pthread_cond_wait(&cond_ejecutar, &m_ejecutar);
    }
    pthread_mutex_unlock(&m_ejecutar);
}

void ejecutar_m_unlock(t_instruccion* instr){
    char* nombre = instr->param1;
    log_debug(logger, "Ejecutando MUTEX_UNLOCK %s", nombre);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_UNLOCK);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
    detener_ejecucion(); // cpu tiene que esperar a que esta syscall se termine (puede ser instantaneo, o no, depende) para seguir ejecutando
}

void ejecutar_m_lock(t_instruccion* instr){
    char* nombre = instr->param1;
    log_debug(logger, "Ejecutando MUTEX_LOCK %s", nombre);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_LOCK);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
    detener_ejecucion(); // cpu tiene que esperar a que esta syscall se termine (puede ser instantaneo, o no, depende) para seguir ejecutando
}

void ejecutar_m_create(t_instruccion* instr){
    char* nombre = instr->param1;
    log_debug(logger, "Ejecutando MUTEX_CREATE %s", nombre);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_CREATE);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
    detener_ejecucion(); // cpu tiene que esperar a que esta syscall se termine (es instantaneo) para seguir ejecutando
}

void ejecutar_stdout(t_instruccion* instr){
    int dir_logica = atoi(instr->param1);
    int tamanio = atoi(instr->param2);
    t_paquete* paquete = crear_paquete(CPU_SCH__STDOUT);
    agregar_a_paquete(paquete, &dir_logica, sizeof(dir_logica));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
    detener_ejecucion(); // esta es bloqueante, obligatoriamente detiene la ejecucion hasta que scheduler le mande el proximo pid
}

void ejecutar_stdin(t_instruccion* instr){
    int dir_logica = atoi(instr->param1); // TODO: esto tiene q ser distinto cuando le pida a km las cosas, se deberia poder guardar en un registro, ej "AX"
    int tamanio = atoi(instr->param2);
    t_paquete* paquete = crear_paquete(CPU_SCH__STDIN);
    agregar_a_paquete(paquete, &dir_logica, sizeof(dir_logica));
    agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
    detener_ejecucion(); // esta es bloqueante, obligatoriamente detiene la ejecucion hasta que scheduler le mande el proximo pid
}

void ejecutar_sleep(t_instruccion* instr){
    int tiempo_sleep = atoi(instr->param1); // solo puede recibir un numero
    log_debug(logger, "Ejecutando SLEEP %d", tiempo_sleep);
    t_paquete* paquete = crear_paquete(CPU_SCH__SLEEP);
    agregar_a_paquete(paquete, &tiempo_sleep, sizeof(tiempo_sleep));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
    detener_ejecucion(); // esta es bloqueante, obligatoriamente detiene la ejecucion hasta que scheduler le mande el proximo pid
}

void detener_ejecucion(){
    pthread_mutex_lock(&m_ejecutar);
    execute = false;
    pthread_mutex_unlock(&m_ejecutar);
}

void reanudar_ejecucion(){
    pthread_mutex_lock(&m_ejecutar);
    execute = true;
    pthread_cond_signal(&cond_ejecutar);
    pthread_mutex_unlock(&m_ejecutar);
}

void inicializar_variables(){
    execute = false;
    pid_pendiente = -1;
    lista_sticks = list_create();
    lista_instrucciones = list_create();
    // list_add(lista_instrucciones, crear_instruccion(INST_SLEEP, "2000", NULL));
    // list_add(lista_instrucciones, crear_instruccion(INST_SLEEP, "3500", NULL));
    // list_add(lista_instrucciones, crear_instruccion(INST_STDIN, "0", "10")); // El 0 representa la dir logica, deberia ser distinto cuando este bien implementado
    list_add(lista_instrucciones, crear_instruccion(INST_STDOUT, "0", "10")); // El 0 representa la dir logica, deberia ser distinto cuando este bien implementado
}

t_pcb* inicializar_pcb(int pid){
    t_pcb* pcb = malloc(sizeof(t_pcb)); 
    pcb->pid = pid;
    return pcb;
}

t_instruccion* crear_instruccion(t_tipo_instruccion tipo, char* param1, char* param2){
    t_instruccion* inst = malloc(sizeof(t_instruccion));
    inst->tipo = tipo;
    inst->param1 = param1 ? strdup(param1) : NULL;
    inst->param2 = param2 ? strdup(param2) : NULL;
    return inst;
}

t_instruccion* proxima_instruccion(){ // TODO: que esta funcion pida a memoria (fetch) y convierta lo que me mande en un t_instruccion (decode)
    log_debug(logger, "pc=%d size=%d", pc, list_size(lista_instrucciones));
    if(pc >= list_size(lista_instrucciones)){
        return NULL;
    }
    return list_get(lista_instrucciones, pc);
}

void conectarse_a_stick(char* ip, char* puerto){
    int conexion_memory_stick = crear_conexion(ip, puerto);
    exit_si_error_conexion(conexion_memory_stick, logger, "memory stick");
    handshake_cliente(conexion_memory_stick, logger);
}