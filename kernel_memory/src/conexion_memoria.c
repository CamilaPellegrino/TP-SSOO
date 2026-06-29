#include "conexion_memoria.h"
void desconexion_por_bsod();
void* ejecutar_pedidos_lectura(t_list* pedidos, int tamanio_total);
void ejecutar_pedidos_escritura(t_list* pedidos, void* bytes);
typedef void (*t_handler)(t_evt*);

t_list* lista_procesos_suspendidos;
pthread_mutex_t m_list_procesos_suspendidos;
t_bitarray* bitarray;
pthread_mutex_t m_bitarray;

// STICKS
void* atender_stick(void* arg){
    
    lista_procesos_suspendidos = list_create();
    pthread_mutex_init(&m_list_procesos_suspendidos, NULL);
    pthread_mutex_init(&m_bitarray, NULL);
    int bytes = (cant_bloques + 7) / 8;
    char* bitmap = calloc(bytes, sizeof(char));
    bitarray = bitarray_create_with_mode(bitmap, bytes, LSB_FIRST);
    
    t_handler handlers[] = {
        [SCH_LECTURA]    = atender_sch_lectura,
        [SCH_ESCRITURA]  = atender_sch_escritura,
        [SCH_MOVER]      = atender_sch_mover,
        [SUSPENSION]     = atender_suspension,
        [DESUSPENSION]   = atender_desuspension
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

void atender_suspension(t_evt* evt){
    int pid = evt->pid;
    log_info(logger, "Suspendiendo proceso <%d>", pid);
    
    t_proceso* proceso = proceso_de_pid_thread_safe(pid);
    t_list* segmentos = proceso->lista_segmentos;
    
    for(int i = 0; i<list_size(segmentos);i++){
        t_segmento* s = list_get(segmentos, i);
        t_list* pedidos_lectura = pedidos_a_sticks_para_acceder_a(s->base, s->tamanio);
        void* datos = ejecutar_pedidos_lectura(pedidos_lectura, s->tamanio);
        t_list* bloques = bloques_necesarios_para_escribir_en_swap(s->tamanio);
        if(bloques == NULL){
            log_info(logger, "Segmento %d (base=%u, tamaño=%d): no hay bloques libres", i, s->base, s->tamanio);
        }else{
            log_info(logger, "Segmento %d (base=%u, tamaño=%d):", i, s->base, s->tamanio);
            for(int j = 0; j < list_size(bloques); j++){
                int* bloque = list_get(bloques, j);
                log_info(logger, "    -> Bloque SWAP %d", *bloque);
            }

            list_destroy(bloques);
        }
        free(datos);
    }
    sem_post(&evt->s_fin);
}

void atender_desuspension(t_evt* evt){

}


void atender_sch_escritura(t_evt* evt){
    log_debug(logger, "Caso de SCH_escritura");
    t_data_write* data = (t_data_write*)evt->data;
    int pid = evt->pid;
    int base = data->base;
    int tamanio = data->tamanio;
    void* bytes = data->bytes;
    imprimir_bytes(bytes, tamanio);
    log_debug(logger, "pid: %d, base: %d, tam:%d", pid, base, tamanio);

    t_list* pedidos = pedidos_a_sticks_para_acceder_a(base, tamanio);

    ejecutar_pedidos_escritura(pedidos, bytes);

    sem_post(&evt->s_fin);
}

void atender_sch_lectura(t_evt* evt){
    log_debug(logger, "Caso de SCH_lectura");
    t_data_read* data = (t_data_read*)evt->data;
    int pid = evt->pid;
    int base = data->base;
    int tamanio = data->tamanio;
    t_list* pedidos = pedidos_a_sticks_para_acceder_a(base, tamanio);

    void* datos_leidos = ejecutar_pedidos_lectura(pedidos, tamanio);
    printf("Datos: \n");
    imprimir_bytes(datos_leidos, tamanio);
    char* texto = (char*) datos_leidos;
    log_debug(logger, "Datos leidos: %s", texto);
    data->datos_leidos = datos_leidos;
    sem_post(&evt->s_fin);
}

void atender_sch_mover(t_evt* evt){
    t_data_mover* data = (t_data_mover*)evt->data;
    int pid = evt->pid;
    int tamanio_total = data->tamanio;
    log_debug(logger, "Caso de SCH_MOVER, base_leer:%d, tamanio:%d, base_escribir:%d", data->base_leer, tamanio_total, data->base_escribir);
    
    t_list* pedidos_para_leer = pedidos_a_sticks_para_acceder_a(data->base_leer, tamanio_total);
    t_list* pedidos_para_escribir = pedidos_a_sticks_para_acceder_a(data->base_escribir, tamanio_total);

    void* bytes = ejecutar_pedidos_lectura(pedidos_para_leer, tamanio_total);

    ejecutar_pedidos_escritura(pedidos_para_escribir, bytes);

    imprimir_bytes(bytes, tamanio_total);
    
    free(bytes);
    list_destroy_and_destroy_elements(pedidos_para_leer, free);
    list_destroy_and_destroy_elements(pedidos_para_escribir, free);

    sem_post(&evt->s_fin);
}

void* ejecutar_pedidos_lectura(t_list* pedidos, int tamanio_total){
    log_debug(logger, "mandando pedidos de lectura a sticks");
    int offset = 0;
    void* ret = calloc(tamanio_total, 1);

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
        imprimir_bytes(bytes, pedido->tamanio);
        memcpy((char*)ret + offset, bytes, pedido->tamanio);

        offset += pedido->tamanio;

        list_destroy_and_destroy_elements(data, free);
    }
    log_debug(logger, "Lectura finalizada");
    return ret;
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
    log_debug(logger, "Escritura finalizada");

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

t_list* bloques_necesarios_para_escribir_en_swap(int tamanio) {
    int bloques_necesarios = (tamanio + tam_bloque - 1) / tam_bloque;

    t_list* bloques = list_create();

    pthread_mutex_lock(&m_bitarray);

    for (int i = 0; i < cant_bloques && list_size(bloques) < bloques_necesarios; i++) {
        if (!bitarray_test_bit(bitarray, i)) {
            bitarray_set_bit(bitarray, i);

            int* nro_bloque = malloc(sizeof(int));
            *nro_bloque = i;
            list_add(bloques, nro_bloque);
        }
    }

    if (list_size(bloques) < bloques_necesarios) {

        for (int i = 0; i < list_size(bloques); i++) {
            int* bloque = list_get(bloques, i);
            bitarray_clean_bit(bitarray, *bloque);
            free(bloque);
        }

        list_destroy(bloques);
        bloques = NULL;
    }

    pthread_mutex_unlock(&m_bitarray);

    return bloques;
}

// SWAP

int obtener_primer_bloque_libre(int cant_bloques) {
    pthread_mutex_lock(&m_bitarray);
    for (int i = 0; i < cant_bloques; i++) {
        if (!bitarray_test_bit(bitarray, i)) {
            return i;
        }
    }
    pthread_mutex_unlock(&m_bitarray);

    return -1; // No hay bloques libres
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