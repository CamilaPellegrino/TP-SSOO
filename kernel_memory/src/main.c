#include <utils/hello.h>
#include <utils/utils.h>
#include "base_km.h"

void* atender_cliente(void *arg);
void atender_scheduler(int sch_fd);
void atender_stick(int sch_fd, int *tamanio);
void atender_swap(int swap_fd);
void atender_cpu(t_cpu* cpu);
// void enviar_nuevo_stick_a_cpus(t_stick *nuevo_stick);
// void enviar_nuevo_stick_a_cpu(t_stick* nuevo_stick, int cpu_fd);
void enviar_nuevo_stick_a_scheduler(t_stick* nuevo_stick, int sch_fd);
// variables globales

t_log *logger;
t_list *lista_sticks;
t_list *lista_cpus;

int sch_fd; 

/*void inicializar_nueva_cpu(t_list *lista_sticks, int cpu_id){ 
    // crear una nueva estructura para la cpu  // agregarla a la lista de cpus
} hola,  no los escucho xd, se me escucha? nope! q mal
*/
int main(int argc, char* argv[]) { //KERNEL MEMORY
    // ejemplo para ejecutar: ./bin/kernel_memory ./kernel_memory.config
    if(argc < 2){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/kernel_memory ./kernel_memory.config");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];
    char* puerto;

    t_config* config;
    lista_sticks = list_create();
    lista_cpus = list_create();
    
    logger = iniciar_logger("kernel_memory.log", "ProcesoKernelMemory", LOG_LEVEL_INFO );
    config = iniciar_config(ruta_config);
    if(config == NULL){
        printf("No se pudo cargar el config\n");
        exit(EXIT_FAILURE);
    }

    // iniciar servidor
    puerto = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    int kernel_memory_fd = iniciar_servidor(puerto);
    if(kernel_memory_fd == -1){
        log_error(logger, "No se pudo iniciar el servidor");
        exit(EXIT_FAILURE);
    }

    log_info(logger, "esperar cliente");
    // esperar clientes
    while(true){
        int *cliente_fd = esperar_cliente(kernel_memory_fd);
        
        log_info(logger, "Me llego un cliente, %d", *cliente_fd);
        handshake_servidor(*cliente_fd, logger);

        //Creacion del hilo para atender la conexion entrante
        pthread_t thread;
        pthread_create(&thread, NULL, atender_cliente, cliente_fd);
        pthread_detach(thread);
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

            atender_scheduler(cliente_fd);
            free(msg);
            break;
        }case STICK_KM__CONEXION: {
            // recibo el paquete que me mando stick en una t_list
            t_list *lista_paquete = recibir_paquete(cliente_fd);

            // leo cada elemento de la t_list en el orden en que stick los agrego al paquete
            int *tamanio = list_get(lista_paquete, 0);
            char *puerto = list_get(lista_paquete, 1);
            char *ip = list_get(lista_paquete, 2);
            log_info(logger, "Me llego un stick: %d, %s, %s", *tamanio, puerto, ip);
            
            // poner al stick en la lista de sticks
            t_stick* nuevo_stick = iniciar_stick(ip, puerto, *tamanio, cliente_fd);
            
            list_add(lista_sticks, nuevo_stick);
            
            //avisar a las cpus conectadas que llego un stick para que se conecten
            // ...
            enviar_nuevo_stick_a_scheduler(nuevo_stick, sch_fd);
            // atender stick
            atender_stick(cliente_fd, tamanio);
            
            break;
        }case SWAP_KM__CONEXION:{
            log_info(logger, "SWAP");
            atender_swap(cliente_fd);
            break;
        }case CPU_KM__CONEXION:{
            t_list *lista_paquete = recibir_paquete(cliente_fd);
            int *cpu_id = list_get(lista_paquete, 0);
            log_info(logger, "Me llego cpu de id: %d", *cpu_id);

            // agregar CPU a la lista de cpus
            t_cpu* cpu = iniciar_cpu(*cpu_id, cliente_fd);
            list_add(lista_cpus, cpu);

            // mandarle todos los sticks que se conectaron hasta ahora
            
            atender_cpu(cpu);
            list_destroy_and_destroy_elements(lista_paquete, free);
            break;
        }default: 
            log_warning(logger, "Warning: Operacion desconocida en atender_cliente, cod_op = %d", cod_op);
    }
    return NULL;
}

void atender_cpu(t_cpu* cpu){
    int cpu_fd = cpu->fd;
    int cpu_id = cpu->id;
    while(1){
        log_info(logger, "## CPU <%d> Conectada", cpu_id);
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
                }
                case KM_READ:
                case KM_WRITE:{
                    // MOCK: Responder OK sin implementar lógica real [3]
                    enviar_mensaje("OK", cpu_fd, KM_CPU__RESPUESTA);
                    log_info(logger, "Respuesta MOCK: OK");
                    break;
                }
                default:{
                    log_warning(logger,"atender_cpu: op desconocida, op=%d", cod_op);
                }

            }
    }
    close(cpu_fd);
    log_info(logger, "cerrando hilo de CPU");
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
void atender_scheduler(int sch_fd){
    log_info(logger, "## Kernel Scheduler Conectado - FD del socket: %d", sch_fd);
    while(1){
        op_code cod_op = recibir_operacion(sch_fd);
        if(cod_op == -1){
            log_warning(logger, "error, se desconecto scheduler");
            // desconectar todo
            
            exit(EXIT_FAILURE);
        }
    }
    log_info(logger, "cerrando hilo de sch");
    return;
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


/*
void enviar_nuevo_stick_a_cpus(t_stick* nuevo_stick){
    log_info(logger, "size: %d", list_size(lista_cpus));

    for(int i = 0; i < list_size(lista_cpus); i++){
        log_info(logger, "en for");
        t_cpu* cpu = list_get(lista_cpus, i);
        enviar_nuevo_stick_a_cpu(nuevo_stick, cpu->fd);
    }
}

void enviar_nuevo_stick_a_cpu(t_stick* nuevo_stick, int cpu_fd){
    t_paquete* paquete = crear_paquete(KM_CPU__NUEVO_STICK);
    agregar_string_a_paquete(paquete, nuevo_stick->ip);
    agregar_string_a_paquete(paquete, nuevo_stick->puerto);
    enviar_paquete(paquete, cpu_fd);
    log_info(logger, "Enviando stick con IP %s a la CPU por el FD %d",nuevo_stick->ip, cpu_fd);
}
*/