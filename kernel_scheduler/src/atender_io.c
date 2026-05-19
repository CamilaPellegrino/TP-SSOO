#include "atender_io.h"
// Atender modulo IO: 
void* atender_io(t_io* io){
    t_tipo_io tipo = io->tipo;
    log_info(logger, "** Atendiendo al io de tipo %d", tipo);
    switch(tipo){
        case SLEEP:
            atender_io_sleep(io);
            break;
        case STDIN:
            atender_io_stdin(io);
            break;
        case STDOUT:
        log_warning(logger, "atender_io: falta implementar STDOUT");
            // atender_io_stdout(io);
            break;
    }
    return NULL;
}

void atender_io_stdin(t_io* io){
    int io_fd = io->fd;
    while(1){
        sem_wait(&s_evt_stdin);
        log_debug(logger, "atender_io_stdin hizo sem_wait");

        pthread_mutex_lock(&m_lista_evt_stdin);
        t_evt* evt = list_remove(lista_evt_stdin, 0);
        pthread_mutex_unlock(&m_lista_evt_stdin);

        if(evt == NULL){
            log_warning(logger, "evento stdin NULL");
            continue;
        }
        t_evt_stdin* data_evt = evt->data_evt;
        int tamanio = data_evt->tamanio;
        t_pcb* proceso = evt->proceso;

        // mandar al modulo de io la solic (con todos los datos que haya en t_evt_sleep, en este caso seria el tiempo de sleep)
        t_paquete* paquete_sleep = crear_paquete(SCH_IO__SOLICITUD);
        agregar_a_paquete(paquete_sleep, &(proceso->pid), sizeof(proceso->pid));
        agregar_a_paquete(paquete_sleep, &(tamanio), sizeof(tamanio));
        enviar_paquete_y_liberarlo(paquete_sleep, io_fd);
        
        // esperar que termine y recibir confirmacion
        op_code cod_op = recibir_operacion(io_fd);
        t_list* lista_paquete = recibir_paquete(io_fd);
        char* contenido = list_get(lista_paquete, 0);
        // TODO: enviar contenido a KM para que lo guarde en la dir_logica
        // ...

        pthread_mutex_lock(&evt->mutex);

        evt->syscall_finalizada = true;
        pthread_cond_signal(&evt->cond);
        
        pthread_mutex_unlock(&evt->mutex);
        
        if(proceso->estado == BLOQUEADO){
            log_info(logger, "## (<%d>) finalizó IO y pasa a READY", proceso->pid);
            blocked_a_ready(proceso);
        }else if(proceso->estado == SUSP_BLOQUEADO){
            log_info(logger, "## (<%d>) finalizó IO y pasa a SUSP_READY", proceso->pid);
            susp_blocked_a_susp_ready(proceso);
        }
        pthread_join(evt->hilo_timeout, NULL); //estoy espserando que termine para que el hilo_timeout no intente acceder a evt 
        pthread_mutex_destroy(&evt->mutex);
        pthread_cond_destroy(&evt->cond);
        free(evt->data_evt);
        free(evt);
    }
}

void atender_io_sleep(t_io* io){
    int io_fd = io->fd;
    while(1){
        sem_wait(&s_evt_sleep);
        log_debug(logger, "atender_io_sleep hizo sem_wait");

        pthread_mutex_lock(&m_lista_evt_sleep);
        t_evt* evt = list_remove(lista_evt_sleep, 0);
        pthread_mutex_unlock(&m_lista_evt_sleep);

        if(evt == NULL){
            log_warning(logger, "evento sleep NULL");
            continue;
        }
        t_evt_sleep* data_evt = evt->data_evt;
        int tiempo_sleep = data_evt->tiempo_sleep;
        t_pcb* proceso = evt->proceso;

        // mandar al modulo de io la solic (con todos los datos que haya en t_evt_sleep, en este caso seria el tiempo de sleep)
        t_paquete* paquete_sleep = crear_paquete(SCH_IO__SOLICITUD);
        agregar_a_paquete(paquete_sleep, &(proceso->pid), sizeof(proceso->pid));
        agregar_a_paquete(paquete_sleep, &(tiempo_sleep), sizeof(tiempo_sleep));
        enviar_paquete_y_liberarlo(paquete_sleep, io_fd);
        
        // esperar que termine y recibir confirmacion
        op_code cod_op = recibir_operacion(io_fd);

        pthread_mutex_lock(&evt->mutex);

        evt->syscall_finalizada = true;
        pthread_cond_signal(&evt->cond);
        
        pthread_mutex_unlock(&evt->mutex);
        
        if(proceso->estado == BLOQUEADO){
            log_info(logger, "## (<%d>) finalizó IO y pasa a READY", proceso->pid);
            blocked_a_ready(proceso);
        }else if(proceso->estado == SUSP_BLOQUEADO){
            log_info(logger, "## (<%d>) finalizó IO y pasa a SUSP_READY", proceso->pid);
            susp_blocked_a_susp_ready(proceso);
        }
        pthread_join(evt->hilo_timeout, NULL); //estoy espserando que termine para que el hilo_timeout no intente acceder a evt 
        pthread_mutex_destroy(&evt->mutex);
        pthread_cond_destroy(&evt->cond);
        free(evt->data_evt);
        free(evt);
    }
}