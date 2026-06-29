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
            atender_io_stdout(io);
            break;
        default:
            log_warning(logger, "IO de tipo desconcido");
    }
    return NULL;
}

void atender_sys(t_io* io, t_paquete* paquete_envio, char* sys_name){
    int io_fd = io->fd;
    log_debug(logger, "atender_io_%s hizo sem_wait", sys_name);

    enviar_paquete_y_liberarlo(paquete_envio, io_fd);

    /*op_code cod_op = */recibir_operacion(io_fd);
    log_debug(logger, "llego op de IO");
}

void atender_io_stdout(t_io* io){
    while(1){
        sem_wait(&s_evt_stdout);

        pthread_mutex_lock(&m_lista_evt_stdout);
        t_evt* evt = list_remove(lista_evt_stdout, 0);
        pthread_mutex_unlock(&m_lista_evt_stdout);

        if(evt == NULL){
            log_warning(logger, "evento stdout NULL");
            continue;
        }
        t_pcb* proceso = evt->proceso;
        t_evt_std_out* data = evt->data_evt;
        char* datos_leidos = data->datos_leidos;

        t_paquete* paquete_sys = crear_paquete(SCH_IO__SOLICITUD);
        agregar_a_paquete(paquete_sys, &(proceso->pid), sizeof(proceso->pid));
        agregar_string_a_paquete(paquete_sys, datos_leidos);

        atender_sys(io, paquete_sys, "stdout");
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

void atender_io_stdin(t_io* io){
    while(1){
        sem_wait(&s_evt_stdin);

        pthread_mutex_lock(&m_lista_evt_stdin);
        t_evt* evt = list_remove(lista_evt_stdin, 0);
        pthread_mutex_unlock(&m_lista_evt_stdin);

        if(evt == NULL){
            log_warning(logger, "evento stdin NULL");
            continue;
        }
        t_evt_std_in* data_evt = evt->data_evt;
        int tamanio = data_evt->tamanio;
        t_dir_fisica dir_fisica = data_evt->dir_fisica;
        t_pcb* proceso = evt->proceso;

        // mandar al modulo de io la solic (con todos los datos que haya en t_evt_sleep, en este caso seria el tiempo de sleep)
        t_paquete* paquete_sys = crear_paquete(SCH_IO__SOLICITUD);
        agregar_a_paquete(paquete_sys, &(proceso->pid), sizeof(proceso->pid));
        agregar_a_paquete(paquete_sys, &(tamanio), sizeof(tamanio));
        
        atender_sys(io, paquete_sys, "stdin");
        
        t_list* lista_paquete = recibir_paquete(io->fd);

        void* contenido = list_get(lista_paquete, 0);

        t_paquete* paquete = crear_paquete(KM_WRITE);
        agregar_a_paquete(paquete, &dir_fisica, sizeof(dir_fisica));
        agregar_a_paquete(paquete, &tamanio, sizeof(tamanio));
        agregar_a_paquete(paquete, contenido, tamanio);
        agregar_a_paquete(paquete, &proceso->pid, sizeof(proceso->pid));

        enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    }
}

void atender_io_sleep(t_io* io){
    while(1){
        sem_wait(&s_evt_sleep);

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
        
        atender_sys(io, paquete_sleep, "sleep");
        
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