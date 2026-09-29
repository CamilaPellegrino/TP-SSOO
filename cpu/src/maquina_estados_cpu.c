#include "maquina_estados_cpu.h"
typedef void (*t_transicionador)(int);

void atender_scheduler(void){
	while(1){
		op_code cod_op = recibir_operacion(conexion_kernel_scheduler);
		if(cod_op == -1){
			log_error(logger, "Error: se desconecto scheduler");
			break;
		}

		if(cod_op == SCH_CPU__PID){
			log_debug(logger, "Recibiendo pid");
			t_list* data = recibir_paquete(conexion_kernel_scheduler);
			pthread_mutex_lock(&m_pid_pendiente);
			pid_pendiente = *(int*)list_get(data, 0);
			pthread_mutex_unlock(&m_pid_pendiente);
			list_destroy_and_destroy_elements(data, free);
		}

		switch(cod_op){
			case SCH_CPU__NUEVO_STICK: {
				log_debug(logger, "Conexion de modulo stick");
				t_list* lista_del_paquete = recibir_paquete(conexion_kernel_scheduler);
				char* ip_stick = list_get(lista_del_paquete, 0);
				char* puerto_stick = list_get(lista_del_paquete, 1);
				int* tamanio = list_get(lista_del_paquete, 2);

				int conexion_memory_stick = crear_conexion(ip_stick, puerto_stick);
				exit_si_error_conexion(conexion_memory_stick, logger, "memory stick");
				handshake_cliente(conexion_memory_stick, logger);
				conectarse_a_stick(ip_stick, puerto_stick, *tamanio);
				list_destroy_and_destroy_elements(lista_del_paquete, free);
				break;
			}
			default:
				procesar_evento(cod_op);
				break;
		}
	}
}

void procesar_evento(op_code cod_op){
	static t_transicionador transicionador[] = {
		[EXEC]                           = transicion_desde_exec,
		[WAIT_SYS]                       = transicion_desde_wait_sys,
		[WAIT_SYS_Y_PROX_DESALOJO]       = transicion_desde_wait_sys_y_prox_desalojo,
		[LIBRE]                          = transicion_desde_libre,
		[WAIT_MEM_ALLOC]                 = transicion_desde_wait_mem_alloc,
		[WAIT_MEM_ALLOC_Y_PROX_DESALOJO] = transicion_desde_wait_mem_alloc_y_prox_desalojo
	};
	pthread_mutex_lock(&m_estado_cpu);
	transicionador[estado_cpu](cod_op);
	pthread_mutex_unlock(&m_estado_cpu);
}

void transicion_desde_exec(op_code cod_op){
	switch(cod_op){
		case SCH_CPU__ERROR_SYSCALL:
		case SCH_CPU__COMPACTACION:
		case SCH_CPU__PEDIDO_DESALOJO:
			v_pedido_de_desalojo = true;
			break;
		default:
			break;
	}
}

void transicion_desde_wait_sys(op_code cod_op){
	switch(cod_op){
		case SCH_CPU__COMPACTACION:
		case SCH_CPU__PEDIDO_DESALOJO:
			transicionar(WAIT_SYS_Y_PROX_DESALOJO);
			break;
		case SCH_CPU__FIN_SYSCALL:
			transicionar(EXEC);
			v_pedido_de_desalojo = false;
			pthread_cond_signal(&cond_ejecutar);
			break;
		case SCH_CPU__SYS_BLOQUEANTE:
			v_pedido_de_desalojo = true;
			pthread_cond_signal(&cond_ejecutar);
			transicionar(LIBRE);
			break;
		case SCH_CPU__ERROR_SYSCALL:
			transicionar(LIBRE);
			break;
		default:
			break;
	}
}

void transicion_desde_wait_sys_y_prox_desalojo(op_code cod_op){
	switch(cod_op){
		case SCH_CPU__SYS_BLOQUEANTE:
		case SCH_CPU__FIN_SYSCALL:
			transicionar(LIBRE);
			v_pedido_de_desalojo = true;
			pthread_cond_signal(&cond_ejecutar);
			break;
		case SCH_CPU__ERROR_SYSCALL:
			transicionar(LIBRE);
			break;
		default:
			break;
	}
}

void transicion_desde_libre(op_code cod_op){
	switch(cod_op){
		case SCH_CPU__PID:
			transicionar(EXEC);
			v_pedido_de_desalojo = false;
			pthread_cond_signal(&cond_ejecutar);
			break;
		case SCH_CPU__PEDIDO_DESALOJO:
			enviar_confirmacion();
			break;
		case SCH_CPU__ERROR_SYSCALL:
			transicionar(LIBRE);
			break;
		default:
			break;
	}
}

void transicion_desde_wait_mem_alloc(op_code cod_op){
	switch(cod_op){
		case SCH_CPU__PEDIDO_DESALOJO:
			transicionar(WAIT_MEM_ALLOC_Y_PROX_DESALOJO);
			break;
		case SCH_CPU__FIN_SYSCALL:
			transicionar(EXEC);
			pthread_cond_signal(&cond_ejecutar);
			break;
		case SCH_CPU__COMPACTACION:
			transicionar(LIBRE);
			enviar_confirmacion();
			break;
		case SCH_CPU__ERROR_SYSCALL:
			transicionar(LIBRE);
			break;
		default:
			break;
	}
}

void transicion_desde_wait_mem_alloc_y_prox_desalojo(op_code cod_op){
	switch(cod_op){
		case SCH_CPU__COMPACTACION:
			transicionar(LIBRE);
			enviar_confirmacion();
			break;
		case SCH_CPU__FIN_SYSCALL:
			v_pedido_de_desalojo = true;
			pthread_cond_signal(&cond_ejecutar);
			break;
		case SCH_CPU__ERROR_SYSCALL:
			transicionar(LIBRE);
			break;
		default:
			break;
	}
}

void enviar_confirmacion(void){
	set_pedir_segmentos(false);
	enviar_operacion(conexion_kernel_scheduler, CPU_SCH__EJECUCION_DETENIDA);
}

void procesar_desalojo_pendiente(t_pcb* pcb){
	log_debug(logger, "Pedido de desalojo detectado, pasando a LIBRE");
	transicionar(LIBRE);
	enviar_pcb_actualizado_a_km(pcb);
	v_pedido_de_desalojo = false;
	enviar_confirmacion();
}

void esperar_a_poder_ejecutar(t_pcb* pcb){
	pthread_mutex_lock(&m_estado_cpu);
	while(1){
		if(v_pedido_de_desalojo){
			log_info(logger, "## Interrupción recibida");
			procesar_desalojo_pendiente(pcb);
		}
		if(estado_cpu == EXEC)
			break;
		pthread_cond_wait(&cond_ejecutar, &m_estado_cpu);
	}
	pthread_mutex_unlock(&m_estado_cpu);
}
