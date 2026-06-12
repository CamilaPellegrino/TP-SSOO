#include "conexion_memoria.h"
void desconexion_por_bsod();


// STICKS
void* atender_stick(void* arg){
    t_list* lista_evt = lista_eventos_stick;
    while(1){
        sem_wait(&s_lista_eventos_stick);
        log_debug(logger, "Pedido a sticks");
        
        pthread_mutex_lock(&m_lista_eventos_stick);
        if(list_is_empty(lista_evt)){
            pthread_mutex_unlock(&m_lista_eventos_stick);
            continue;
        }

        t_evt* evt = list_remove(lista_evt, 0);
        pthread_mutex_unlock(&m_lista_eventos_stick);

        int pid = evt->pid;
        switch(evt->tipo){
            case SCH_LECTURA: {
                log_debug(logger, "Caso de SCH_lectura");
                t_data_read* data = (t_data_read*)evt->data;
                // ...
                char* datos_leidos = "Si esto anda soy una crack B)";
                t_paquete* paquete = crear_paquete(RTA_READ);
                agregar_a_paquete(paquete, &pid, sizeof(pid));
                agregar_string_a_paquete(paquete, datos_leidos);
                enviar_paquete_y_liberarlo(paquete, sch_fd);
                break;
            }
            case SCH_ESCRITURA: {
                log_debug(logger, "Caso de SCH_escritura");
                t_data_write* data = (t_data_write*)evt->data;
                int base = data->base;
                int tamanio = data->tamanio;
                void* bytes = data->bytes;
                imprimir_bytes(bytes, tamanio);

                t_paquete* paquete = crear_paquete(RTA_WRITE);
                agregar_a_paquete(paquete, &pid, sizeof(pid));
                enviar_paquete_y_liberarlo(paquete, sch_fd);

                break;
            }
            case SCH_MOVER: {
                t_data_mover* data = (t_data_mover*)evt->data;
                
                log_debug(logger, "Caso de SCH_MOVER, stick:%d, base_leer;%d, tamanio:%d, base_escribir:%d", data->stick, data->base_leer, data->tamanio, data->base_escribir);
                usleep(1000);
                // (...)
                sem_post(&s_fin_mover);
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