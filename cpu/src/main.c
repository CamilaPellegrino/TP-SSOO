#include <utils/hello.h>
#include <utils/utils.h>

// variables globale
t_log* logger;

t_list* lista_sticks; // lista global para guardar los memory sticks a los que me conecte

int pid_global; // variable global para guardar el PID que me mande el scheduler

// void* atender_kernel_memory(int* conexion_kernel_memory);
void conectarse_a_stick(char* ip, char* puerto);

int main(int argc, char* argv[]){
    // ejemplo para ejecutar: ./bin/cpu "./cpu.config" 0
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
    log_info(logger, "ip: %s, puerto_kernel_memory: %s", ip, puerto_kernel_memory);


    // conectar a kernel scheduler
    int conexion_kernel_scheduler = crear_conexion(ip, puerto_kernel_scheduler);
    exit_si_error_conexion(conexion_kernel_scheduler, logger, "kernel scheduler");
    handshake_cliente(conexion_kernel_scheduler,logger);
    
    // enviar mensaje a sch:
    enviar_mensaje("Soy CPU :)", conexion_kernel_scheduler, CPU_SCH__CONEXION);

    // conectar a kernel memory
    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory);
    log_info(logger, "con fd: %d", conexion_kernel_memory);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    handshake_cliente(conexion_kernel_memory,logger);
        
    //mando informacion propia al km
    t_paquete *paquete = crear_paquete(CPU_KM__CONEXION);
    agregar_a_paquete(paquete, &id_cpu, sizeof(id_cpu));
    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);

    // inicializar listas
    lista_sticks = list_create();


    // queda escuchando mensajes que envie el scheduler
    while (1)
    {
        op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
        log_info(logger, "codop: %d",cod_op);
        if(cod_op == -1){
            log_info(logger, "se desconecto scheduler"); 
            break; 
        }
        switch(cod_op){
            case SCH_CPU__PID:{
                log_info(logger, "me llego un PID");
                
                break;
            }
            case SCH_CPU__NUEVO_STICK: {
                log_info(logger, "recibiendo cosas de stick");
                t_list * lista_del_paquete = recibir_paquete(conexion_kernel_scheduler);
                
                char* ip_stick = list_get(lista_del_paquete, 0);
                char* puerto_stick = list_get(lista_del_paquete, 1);
                int* tamanio = list_get(lista_del_paquete, 2); 
                
                log_info(logger, "%s, %s", ip_stick, puerto_stick);
                
                // conectar a memory stick
                int conexion_memory_stick = crear_conexion(ip_stick, puerto_stick);
                exit_si_error_conexion(conexion_memory_stick, logger, "memory stick");
                handshake_cliente(conexion_memory_stick,logger);
                
                log_info(logger, "cosas de stick: ip: %s, puerto: %s", ip_stick, puerto_stick);
                //STAND BY
                // guardar datos del stick en lista_sticks
                t_stick* stick = iniciar_stick(ip_stick, puerto_stick, tamanio, conexion_memory_stick);
                list_add(lista_sticks, stick);
                
                // liberar lista_del_paquete
                list_destroy(lista_del_paquete);

                /*
                1. A través de variables globales: Cualquier hilo puede acceder a ellas.

                2. Pasando punteros al crearlos: Cuando usas pthread_create, el último parámetro es un void* que te permite 
                pasarle cualquier estructura de datos (como el t_stick de tu ejemplo anterior) al nuevo hilo.
                */
                log_info(logger, "me llego un nuevo memory stick, ip: %s, puerto: %s", ip_stick, puerto_stick);
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
    // close(conexion_memory_stick);
    return 0;
}

void conectarse_a_stick(char* ip, char* puerto){
    int conexion_memory_stick = crear_conexion(ip, puerto);
    exit_si_error_conexion(conexion_memory_stick, logger, "error al conectar a memory stick");
    handshake_cliente(conexion_memory_stick, logger);
    
}