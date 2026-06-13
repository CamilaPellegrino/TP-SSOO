#include "conexion_memoria.h"
void desconexion_por_bsod();

typedef void (*t_handler)(t_evt*);

typedef struct{
    int base_en_stick;
    int tamanio;
    int nro_stick;
}t_data_pedido_stick;

// STICKS
void* atender_stick(void* arg){

    t_handler handlers[] = {
        [SCH_LECTURA]    = atender_sch_lectura,
        [SCH_ESCRITURA]  = atender_sch_escritura,
        [SCH_MOVER]      = atender_sch_mover
    };

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
        handlers[evt->tipo](evt);
    };
    log_info(logger, "cerrando hilo de stick");
    return NULL;
}

void desconexion_por_bsod(){
    log_warning(logger, "error, se desconecto stick");
    enviar_operacion(sch_fd, KM_SCH__BSOD);
    exit(EXIT_FAILURE);
}

void atender_sch_escritura(t_evt* evt){
    log_debug(logger, "Caso de SCH_escritura");
    t_data_write* data = (t_data_write*)evt->data;
    int pid = evt->pid;
    int base = data->base;
    int tamanio = data->tamanio;
    void* bytes = data->bytes;
    imprimir_bytes(bytes, tamanio);

    t_list* pedidos = pedidos_a_sticks_para_acceder_a(base, tamanio);

    t_paquete* paquete = crear_paquete(RTA_WRITE);
    agregar_a_paquete(paquete, &pid, sizeof(pid));
    enviar_paquete_y_liberarlo(paquete, sch_fd);
}

void atender_sch_lectura(t_evt* evt){
    log_debug(logger, "Caso de SCH_lectura");
    t_data_read* data = (t_data_read*)evt->data;
    int pid = evt->pid;
    int base = data->base;
    int tamanio = data->tamanio;
    t_list* pedidos = pedidos_a_sticks_para_acceder_a(base, tamanio);
    char* datos_leidos = "Si esto anda soy una crack B)";
    t_paquete* paquete = crear_paquete(RTA_READ);
    agregar_a_paquete(paquete, &pid, sizeof(pid));
    agregar_string_a_paquete(paquete, datos_leidos);
    enviar_paquete_y_liberarlo(paquete, sch_fd);
}

void atender_sch_mover(t_evt* evt){
    t_data_mover* data = (t_data_mover*)evt->data;
    int pid = evt->pid;
    log_debug(logger, "Caso de SCH_MOVER, base_leer;%d, tamanio:%d, base_escribir:%d", data->base_leer, data->tamanio, data->base_escribir);
    usleep(1000);
    // (...)
    sem_post(&s_fin_mover);

}


t_list* pedidos_a_sticks_para_acceder_a(int base, int tamanio){
    t_list* pedidos = list_create();

    int fin = base + tamanio;
    int inicio_stick_global = 0;

    for(int i = 0; i < list_size(lista_sticks); i++){
        t_stick* stick = list_get(lista_sticks, i);
        int fin_stick_global = inicio_stick_global + stick->tamanio;
        // ¿hay intersección entre [base, fin) y este stick?
        int inicio_interseccion = (base > inicio_stick_global) ? base : inicio_stick_global;

        int fin_interseccion = (fin < fin_stick_global) ? fin : fin_stick_global;

        if(inicio_interseccion < fin_interseccion){
            t_data_pedido_stick* pedido = malloc(sizeof(t_data_pedido_stick));
            pedido->nro_stick = i;
            // offset dentro del stick
            pedido->base_en_stick = inicio_interseccion - inicio_stick_global;
            pedido->tamanio = fin_interseccion - inicio_interseccion;
            list_add(pedidos, pedido);
            log_info(logger, "Stick %d -> leer desde %d, tamanio %d", pedido->nro_stick, pedido->base_en_stick, pedido->tamanio);
        }

        inicio_stick_global = fin_stick_global;

        if(fin <= inicio_stick_global)
            break;
    }

    return pedidos;
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