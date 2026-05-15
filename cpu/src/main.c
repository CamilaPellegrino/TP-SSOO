#include <utils/hello.h>
#include <utils/utils.h>
#include "base_cpu.h"

void recibir_pid(int conexion_kernel_scheduler);
void inicializar_variables();
t_instruccion* crear_instruccion(t_tipo_instruccion tipo, char* param1, char* param2);
t_instruccion* proxima_instruccion();
void ejecutar_sleep(t_instruccion* instr);
void ejecutar_m_create(t_instruccion* instr);

// variables globales
t_log* logger;
t_list* instrucciones; // lista hardcodeada de instruciones para testear sch
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
    
    // queda escuchando mensajes que envie el scheduler
    while (1){
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
        if(cod_op == -1){
            log_error(logger, "Error: se desconecto scheduler"); 
            break; 
        }
        switch(cod_op){
            case SCH_CPU__PID:{
                recibir_pid(conexion_kernel_scheduler);
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

void recibir_pid(int conexion_kernel_scheduler){
    t_list* lista_paquete = recibir_paquete(conexion_kernel_scheduler);
    int* nuevo_pid = list_get(lista_paquete, 0);
    pid = *nuevo_pid;
    
    log_info(logger, "me llego un PID, nuevo PID: %d", pid);
    // ahora la cpu deberia pedir el contexto, instrucciones y ejecutarlas una a una. Simulo eso en un sleep(2);

    sleep(1);
    t_instruccion* prox_instruccion;
    bool bloqueante = false;
    while((prox_instruccion = proxima_instruccion()) != NULL){
        log_debug(logger, "ejecutando instruccion nro %d", pc);
        pc++;
        switch(prox_instruccion->tipo){
        case INST_SLEEP:
            ejecutar_sleep(prox_instruccion);
            break;
        case INST_MUTEX_CREATE:
            ejecutar_m_create(prox_instruccion);
            break;
        default:
            log_warning(logger, "Instruccion no implementada, tipo %d", prox_instruccion->tipo);
            break;
        }
    }
}

void ejecutar_m_create(t_instruccion* instr){
    char* nombre = instr->param1;
    log_debug(logger, "Ejecutando MUTEX_CREATE %s", nombre);
    t_paquete* paquete = crear_paquete(CPU_SCH__MUTEX_CREATE);
    agregar_string_a_paquete(paquete, nombre);
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
}

void ejecutar_sleep(t_instruccion* instr){
    int tiempo_sleep = atoi(instr->param1); // solo puede recibir un numero
    log_debug(logger, "Ejecutando SLEEP %d", tiempo_sleep);

    // syscall SLEEP
    t_paquete* paquete = crear_paquete(CPU_SCH__SLEEP);
    agregar_a_paquete(paquete, &tiempo_sleep, sizeof(tiempo_sleep));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_scheduler);
    log_debug(logger, "io completada"); // falso: En realidad lo desaloja, porque el proc pasa a blocked, este log esta mal
}

void inicializar_variables(){
    pc = 0;
    lista_sticks = list_create();
    instrucciones = list_create();
    list_add(instrucciones, crear_instruccion(INST_MUTEX_CREATE, "MUTEX_1", NULL));
    // list_add(instrucciones, crear_instruccion(INST_SLEEP, "2000", NULL)); // TODO: falta recibir desalojos | confirmacion para poder seguir ejecutando mas de 1 instr, x ahora probar de a una
}

t_instruccion* crear_instruccion(t_tipo_instruccion tipo, char* param1, char* param2){
    t_instruccion* inst = malloc(sizeof(t_instruccion));
    inst->tipo = tipo;
    inst->param1 = param1 ? strdup(param1) : NULL;
    inst->param2 = param2 ? strdup(param2) : NULL;
    return inst;
}
t_instruccion* proxima_instruccion()
{// TODO: que esta funcion pida a memoria (fetch) y convierta lo que me mande en un t_instruccion (decode)
    log_debug(logger, "pc=%d size=%d", pc, list_size(instrucciones));
    if(pc >= list_size(instrucciones)){
        return NULL;
    }
    return list_get(instrucciones, pc);
}