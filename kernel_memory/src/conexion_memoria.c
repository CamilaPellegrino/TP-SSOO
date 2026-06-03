#include "conexion_memoria.h"
void desconexion_por_bsod();


// STICKS
void* atender_stick(void* arg){
    t_stick* stick = (t_stick*) arg;
    int stick_fd = stick->fd;
    int tamanio = stick->tamanio;
    log_info(logger, "## Memory Stick de %d bytes Conectada", tamanio);
    t_list* lista_evt = stick->lista_evt;

    while(1){
        sem_wait(&stick->s_list_evt);
        log_debug(logger, "Alguien pidio algo a stick de fd %d, tamanio %d", stick_fd, tamanio);
        
        pthread_mutex_lock(&stick->m_lista_evt);
        if(list_is_empty(lista_evt)){
            pthread_mutex_unlock(&stick->m_lista_evt);
            continue;
        }

        t_evt* evt = list_remove(lista_evt, 0);
        pthread_mutex_unlock(&stick->m_lista_evt);

        switch(evt->tipo){
            case SCH_LECTURA: {
                log_warning(logger, "Caso de SCH_lectura no implementado");

                // enviar_paquete(...);
                // recibir_operacion(...);
                
                t_paquete* paquete = crear_paquete(KM_SCH__LECTURA);
                agregar_string_a_paquete(paquete, "Hola mundo");
                enviar_paquete_y_liberarlo(paquete, sch_fd);
                break;
            }
            case SCH_ESCRITURA: {
                log_warning(logger, "Caso de SCH_escritura no implementado");
                break;
            }
            default: {
                log_warning(logger, "Tipo no valido, tipo: %d", evt->tipo);
            }
        }
    };
    log_info(logger, "cerrando hilo de stick");
    return NULL;
}

void desconexion_por_bsod(){
    log_warning(logger, "error, se desconecto stick");
    enviar_operacion(sch_fd, KM_SCH__BSOD);
    exit(EXIT_FAILURE);
}


// SWAP

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