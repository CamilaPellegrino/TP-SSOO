#include "manejar_segmentos.h"
// privadas
t_hueco* ubicacion_de_proximo_segmento(uint32_t tamanio); 
t_hueco* best_fit(uint32_t tamanio);
t_hueco* worst_fit(uint32_t tamanio);
void agregar_segmento_a_proceso(t_segmento* segmento, t_proceso* proceso); 
bool achicar_hueco(t_hueco* hueco, uint32_t seg_tam);
bool segmento_del_proceso(t_segmento* segmento, t_proceso* proceso);
uint32_t direccion_final(t_hueco* h);
void fusionar_huecos_contiguos();
t_hueco* crear_hueco(int32_t base, uint32_t tamanio);
void insertar_ordenado_por_base(t_list* lista, t_segmento* segmento);
int pos_segmento_que_comienza_en(int base);
bool segmento_adelante_de_dir(t_segmento* s, int dir);

// no thread safe
t_segmento* crear_segmento(t_proceso* proceso, uint32_t id_segmento, uint32_t tamanio){
    printf("Antes: \n");
    imprimir_estado_mem();
    t_hueco* hueco_disp = ubicacion_de_proximo_segmento(tamanio);
    if(hueco_disp == NULL){
        log_error(logger, "Error en crear_segmento: No se encontró hueco disponible");
        if(tamanio_total_libre >= tamanio){
            log_debug(logger, "## Iniciando compactacion de memoria");
        }
        return NULL;
    }
    t_segmento* segmento = malloc(sizeof(t_segmento));
    if(segmento == NULL){
        log_error(logger, "Error en crear_segmento: No se pudo hacer malloc");
        return NULL;
    }

    uint32_t base_hueco = hueco_disp->base;
    bool achicado = achicar_hueco(hueco_disp, tamanio);
    if(!achicado){
        free(segmento);
        return NULL;
    }
    segmento->base = base_hueco;
    segmento->id_segmento = id_segmento;
    segmento->tamanio = tamanio;
    segmento->pid = proceso->pcb->pid;

    agregar_segmento_a_proceso(segmento, proceso);
    printf("Despues: \n");
    imprimir_estado_mem();
    tamanio_total_libre -= segmento->tamanio;
    return segmento;
}

bool eliminar_segmento(t_segmento* segmento, t_proceso* proceso){
    printf("Antes: \n");
    imprimir_estado_mem();
    if(!segmento_del_proceso(segmento, proceso)){
        return false;
    }
    pthread_mutex_lock(&m_lista_segmentos_global);
    bool b = list_remove_element(lista_segmentos_global, segmento);
    pthread_mutex_unlock(&m_lista_segmentos_global);
    if(!b){
        return false;
    }

    bool x = list_remove_element(proceso->lista_segmentos, segmento);
    if(!x){
        pthread_mutex_lock(&m_lista_segmentos_global);
        insertar_ordenado_por_base(lista_segmentos_global, segmento);
        pthread_mutex_unlock(&m_lista_segmentos_global);
        return false;
    }
    crear_hueco(segmento->base, segmento->tamanio);
    tamanio_total_libre += segmento->tamanio;
    free(segmento);
    fusionar_huecos_contiguos();
    printf("Despues: \n");
    imprimir_estado_mem();
    return true;
}

bool hay_espacio_total(uint32_t tamanio){
    return tamanio_total_libre >= tamanio;
}

void agregar_espacio_mem(int bytes){
    crear_hueco(tamanio_total_mem, bytes);
    fusionar_huecos_contiguos();
    imprimir_estado_mem();
}

bool existe_segmento_de_id_de_proc(int id, t_proceso* p){
    for (int i = 0; i < list_size(p->lista_segmentos); i++) {
        t_segmento* s = list_get(p->lista_segmentos, i);
        if (s->id_segmento == id) {
            return true;
        }
    }
    return false;
}

// thread safe
t_segmento* crear_segmento_thread_safe(t_proceso* proceso, uint32_t id_segmento, uint32_t tamanio){
    pthread_mutex_lock(&m_manejar_memoria);
    t_segmento* r = crear_segmento(proceso, id_segmento, tamanio);
    pthread_mutex_unlock(&m_manejar_memoria);
    return r;
}

bool eliminar_segmento_thread_safe(t_segmento* segmento, t_proceso* proceso){
    pthread_mutex_lock(&m_manejar_memoria);
    bool r = eliminar_segmento(segmento, proceso);
    pthread_mutex_unlock(&m_manejar_memoria);
    return r;
}

bool hay_espacio_total_thread_safe(uint32_t tamanio){
    pthread_mutex_lock(&m_manejar_memoria);
    bool r = hay_espacio_total(tamanio);
    pthread_mutex_unlock(&m_manejar_memoria);
    return r;
}

bool compactar_memoria() {
    pthread_mutex_lock(&m_manejar_memoria);
    pthread_mutex_lock(&m_lista_huecos);
    pthread_mutex_lock(&m_lista_segmentos_global);

    if(list_is_empty(lista_huecos)) {
        pthread_mutex_unlock(&m_lista_segmentos_global);
        pthread_mutex_unlock(&m_lista_huecos);
        pthread_mutex_unlock(&m_manejar_memoria);
        return true;
    }

    t_hueco* prox_h = list_get(lista_huecos, 0);
    int prox_base = prox_h->base;
    int ult_dir = 0;

    int s_size = list_size(lista_segmentos_global);

    int i = 0;
    for(; i < s_size; i++) {
        t_segmento* s = list_get(lista_segmentos_global, i);

        if(segmento_adelante_de_dir(s, prox_base))
            break;
    }

    for(; i < s_size; i++) {
        t_segmento* s = list_get(lista_segmentos_global, i);
        t_evt* evt = iniciar_evt_mover(s->pid, s->base, s->tamanio, prox_base);

        agregar_evt_a_stick(evt);
        sem_wait(&evt->s_fin);

        liberar_evt(evt);

        s->base = prox_base;
        prox_base += s->tamanio;
        ult_dir = s->base + s->tamanio;
    }

    int tamanio = 0;

    while(list_size(lista_huecos) > 1) {
        t_hueco* h = list_remove(lista_huecos, 0);
        tamanio += h->tamanio;
        free(h);
    }
    t_hueco* h = list_get(lista_huecos, 0);
    h->base = ult_dir;
    h->tamanio += tamanio;

    pthread_mutex_unlock(&m_lista_segmentos_global);
    pthread_mutex_unlock(&m_lista_huecos);
    pthread_mutex_unlock(&m_manejar_memoria);
    return true;
}

void agregar_espacio_mem_thread_safe(int bytes){
    pthread_mutex_lock(&m_manejar_memoria);
    agregar_espacio_mem(bytes);
    pthread_mutex_unlock(&m_manejar_memoria);
}

bool existe_segmento_de_id_de_proc_thread_safe(int id, t_proceso* p){
    pthread_mutex_lock(&m_manejar_memoria);
    bool r = existe_segmento_de_id_de_proc(id, p);
    pthread_mutex_unlock(&m_manejar_memoria);
    return r;
}
//privadas

t_hueco* ubicacion_de_proximo_segmento(uint32_t tamanio){
    switch(algoritmo_fit){
        case BEST_FIT: {
            return best_fit(tamanio);
        }case WORST_FIT: {
            return worst_fit(tamanio);
        }        
    }
    log_error(logger, "Error: Algoritmo de seleccion de huecos invalido, opciones validas: BEST_FIT, WORST_FIT");
    exit(EXIT_FAILURE);
}

t_hueco* best_fit(uint32_t tamanio){
    t_hueco* hueco = NULL;
    for(int i = 0; i < list_size(lista_huecos); i++){
        t_hueco* hueco_actual = list_get(lista_huecos, i);
        if(hueco_actual->tamanio >= tamanio){
            if(hueco == NULL || hueco->tamanio > hueco_actual->tamanio) {
                hueco = hueco_actual;
            }
        }
    }
    return hueco;
}

t_hueco* worst_fit(uint32_t tamanio){
    t_hueco* hueco = NULL;
    for(int i = 0; i < list_size(lista_huecos); i++){
        t_hueco* hueco_actual = list_get(lista_huecos, i);
        if(hueco_actual->tamanio >= tamanio){
            if(hueco == NULL || hueco->tamanio < hueco_actual->tamanio) {
                hueco = hueco_actual;
            }
        }
    }
    return hueco;
}

void agregar_segmento_a_proceso(t_segmento* segmento, t_proceso* proceso){
    pthread_mutex_lock(&m_lista_segmentos_global);
    insertar_ordenado_por_base(lista_segmentos_global, segmento);
    pthread_mutex_unlock(&m_lista_segmentos_global);
    segmento->pid = proceso->pcb->pid;
    insertar_ordenado_por_base(proceso->lista_segmentos, segmento);
}

bool achicar_hueco(t_hueco* hueco, uint32_t seg_tam){
    if(hueco == NULL){
        return false;
    }
    if(seg_tam > hueco->tamanio){
        log_error(logger, "Error en ubicar_segmento_en_hueco: No se pudo ubicar el segmento de tamanio %d en el hueco de tamanio %d", seg_tam, hueco->tamanio);
        return false;
    }   
    if(hueco->tamanio == seg_tam){
        list_remove_element(lista_huecos, hueco);
        free(hueco);
        return true;
    }
    hueco->tamanio -= seg_tam;
    hueco->base += seg_tam;
    return true;
}

bool segmento_del_proceso(t_segmento* segmento, t_proceso* proceso){
    if(segmento == NULL || proceso == NULL || proceso->lista_segmentos == NULL){
        return false;
    }
    for(int i = 0; i < list_size(proceso->lista_segmentos); i++){
        t_segmento* s = list_get(proceso->lista_segmentos, i);
        if(s->id_segmento == segmento->id_segmento){
            return true;
        }
    }
    return false;
}

uint32_t direccion_final(t_hueco* h){
    return h->base + h->tamanio;
}

void fusionar_huecos_contiguos(){
    if(list_size(lista_huecos) < 2){
        return;
    }
    int i = 0;
    t_hueco* actual = list_get(lista_huecos, 0);

    while(i + 1 < list_size(lista_huecos)){
        t_hueco* sgte = list_get(lista_huecos, i+1);
        if(direccion_final(actual) == sgte->base){
            list_remove_element(lista_huecos, sgte);
            actual->tamanio += sgte->tamanio;
            free(sgte);
            continue;
        }
        i++;
        actual = sgte;
    }
}

t_hueco* crear_hueco(int32_t base, uint32_t tamanio){
    if(base < 0 || tamanio == 0){
        return NULL;
    }
    t_hueco* hueco = malloc(sizeof(t_hueco));
    if(hueco == NULL){
        return NULL;
    }
    hueco->base = base;
    hueco->tamanio = tamanio;
    int i = 0;
    while(i < list_size(lista_huecos)){
        t_hueco* actual = list_get(lista_huecos, i);
        if(base < actual->base){
            break;
        }
        i++;
    }
    list_add_in_index(lista_huecos, i, hueco);
    return hueco;
}

void insertar_ordenado_por_base(t_list* lista, t_segmento* segmento){
    int i = 0;
    while(i < list_size(lista)){
        t_segmento* actual = list_get(lista, i);
        if(segmento->base < actual->base){
            break;
        }
        i++;
    }
    list_add_in_index(lista, i, segmento);
}

int pos_segmento_que_comienza_en(int base){
    pthread_mutex_lock(&m_lista_segmentos_global);
    for(int i = 0; i<list_size(lista_segmentos_global); i++){
        t_segmento* s = list_get(lista_segmentos_global, i);
        if(s->base == base){
            pthread_mutex_unlock(&m_lista_segmentos_global);
            return i;
        }
        if(s->base + s->tamanio > base){
            pthread_mutex_unlock(&m_lista_segmentos_global);
            return -1;
        }
    }
    pthread_mutex_unlock(&m_lista_segmentos_global);
    return -1;
}

bool segmento_adelante_de_dir(t_segmento* s, int dir){
    return s->base > dir;
}

