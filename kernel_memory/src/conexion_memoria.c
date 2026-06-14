#include "conexion_memoria.h"
void desconexion_por_bsod();
void ejecutar_pedidos_lectura(t_list* pedidos, void* ret);
void ejecutar_pedidos_escritura(t_list* pedidos, void* bytes);
typedef void (*t_handler)(t_evt*);

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
    char* datos_leidos = "Si esto anda soy una crack :D";
    t_paquete* paquete = crear_paquete(RTA_READ);
    agregar_a_paquete(paquete, &pid, sizeof(pid));
    agregar_string_a_paquete(paquete, datos_leidos);
    enviar_paquete_y_liberarlo(paquete, sch_fd);
}

void atender_sch_mover(t_evt* evt){
    t_data_mover* data = (t_data_mover*)evt->data;
    int pid = evt->pid;
    int tamanio_total = data->tamanio;
    log_debug(logger, "Caso de SCH_MOVER, base_leer:%d, tamanio:%d, base_escribir:%d", data->base_leer, tamanio_total, data->base_escribir);
    
    t_list* pedidos_para_leer = pedidos_a_sticks_para_acceder_a(data->base_leer, tamanio_total);
    t_list* pedidos_para_escribir = pedidos_a_sticks_para_acceder_a(data->base_escribir, tamanio_total);

    void* bytes = malloc(tamanio_total);

    ejecutar_pedidos_lectura(pedidos_para_leer, bytes);

    ejecutar_pedidos_escritura(pedidos_para_escribir, bytes);

    imprimir_bytes(bytes, tamanio_total);
    
    sem_post(&s_fin_mover);
}

void ejecutar_pedidos_lectura(t_list* pedidos, void* ret){
    log_debug(logger, "mandando pedidos de lectura a sticks");
    int offset = 0;
    for(int i = 0; i<list_size(pedidos); i++){
        t_data_pedido_stick* pedido = list_get(pedidos, i);
        int stick_fd = pedido->stick->fd;
        t_paquete* paquete = crear_paquete(X_STICK__LECTURA);
        agregar_a_paquete(paquete, &pedido->base_en_stick, sizeof(pedido->base_en_stick));
        agregar_a_paquete(paquete, &pedido->tamanio, sizeof(pedido->tamanio));
        enviar_paquete_y_liberarlo(paquete, stick_fd);

        op_code cod_op = recibir_operacion(stick_fd);
        if(cod_op == -1){
            desconexion_por_bsod();
        }
        if(cod_op == STICK_X__ERROR){
            log_error(logger, "Error: Fallo al leer stick, base: %d, tamanio: %d", pedido->base_en_stick, pedido->tamanio);
            exit(EXIT_FAILURE);
        }
        t_list* data = recibir_paquete(stick_fd);
        void* bytes = list_get(data, 0);

        memcpy((char*)ret + offset, bytes, pedido->tamanio);

        offset += pedido->tamanio;
    }
    log_debug(logger, "Lectura finalizada");
}

void ejecutar_pedidos_escritura(t_list* pedidos, void* bytes){
    log_debug(logger, "mandando pedidos de escritura a sticks");
    int offset = 0;
    for(int i = 0; i<list_size(pedidos); i++){
        t_data_pedido_stick* pedido = list_get(pedidos, i);
        int stick_fd = pedido->stick->fd;
        t_paquete* paquete = crear_paquete(X_STICK__ESCRITURA);
        agregar_a_paquete(paquete, &pedido->base_en_stick, sizeof(pedido->base_en_stick));
        agregar_a_paquete(paquete, &pedido->tamanio, sizeof(pedido->tamanio));
        agregar_a_paquete(paquete, (char*)bytes + offset, pedido->tamanio);
        enviar_paquete_y_liberarlo(paquete, stick_fd);

        op_code cod_op = recibir_operacion(stick_fd);
        if(cod_op == -1){
            desconexion_por_bsod();
        }
        if(cod_op == STICK_X__ERROR){
            log_error(logger, "Error: Fallo al leer stick, base: %d, tamanio: %d", pedido->base_en_stick, pedido->tamanio);
            exit(EXIT_FAILURE);
        }

        offset += pedido->tamanio;
    }
    log_debug(logger, "Lectura finalizada");

}

t_list* pedidos_a_sticks_para_acceder_a(int base, int tamanio){
    t_list* pedidos = list_create();

    int fin = base + tamanio;
    int inicio_stick_global = 0;
    printf("\nPEDIDOS PARA ACCEDER A base=%d tam=%d\n", base, tamanio);

    for(int i = 0; i < list_size(lista_sticks); i++){
        t_stick* stick = list_get(lista_sticks, i);
        int fin_stick_global = inicio_stick_global + stick->tamanio;
        int inicio_interseccion = (base > inicio_stick_global) ? base : inicio_stick_global;

        int fin_interseccion = (fin < fin_stick_global) ? fin : fin_stick_global;

        if(inicio_interseccion < fin_interseccion){
            t_data_pedido_stick* pedido = malloc(sizeof(t_data_pedido_stick));
            
            pedido->stick = stick_por_id(i);
            pedido->base_en_stick = inicio_interseccion - inicio_stick_global;
            pedido->tamanio = fin_interseccion - inicio_interseccion;
            list_add(pedidos, pedido);
            printf("    Pedido %d -> Stick %d | base_en_stick=%d | tam=%d | rango_global=[%d,%d]\n", list_size(pedidos) - 1, i, pedido->base_en_stick, pedido->tamanio, inicio_interseccion, fin_interseccion - 1);
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