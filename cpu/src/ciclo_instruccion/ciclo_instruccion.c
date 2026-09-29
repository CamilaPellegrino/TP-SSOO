#include "ciclo_instruccion.h"
#include "decode.h"
#include "execute.h"

/* Funciones para ejecutar el ciclo de instruccion
 * Ciclo de instruccion conformado por 3 pasos:
 * 
 * Etapa   | Accion                                                     | Retorno         |
 * --------+------------------------------------------------------------+-----------------+
 * Fetch   | Solicitar la siguiente instruccion a ejecutar al modulo KM | String          |
 * Decode  | Decodificar la instruccion obtenida                        | t_instruccion*  |
 * Execute | Ejecutar la instruccion                                    | --              |
 * 
 */
void ciclo_instruccion(t_pcb *pcb){
        //FETCH
        char *instruccion = fetch(pcb);
        if(instruccion == NULL){
            log_error(logger, "FETCH devolvio NULL");
            t_instruccion_decodificada * i = malloc(sizeof(t_instruccion_decodificada));
            i->registros = list_create();
            iniciar_instr_exit(i, INSTRUCCION_INVALIDA);
            ejecutar_exit(i, pcb);
            destruir_instruccion(i);
            return;
        }
        int pid = pcb->pid;
        log_info(logger, "## PID: %d - FETCH - Program Counter: %d", pid, pcb->registros.pc);
        
        //DECODE
        t_instruccion_decodificada * instruccion_decodificada = decode(instruccion, pcb);
        if(instruccion_decodificada == NULL){
            log_error(logger, "No se pudo decodificar la instruccion");
            free(instruccion);
            return;
        }
        int pc_antiguo = pcb->registros.pc;
        //execute
        bool actualizar_contexto = execute(instruccion_decodificada, pcb);
        log_info(logger, "## PID: %d - Ejecutando: %s", pid, instruccion);
        if(instruccion_decodificada->tipo == I_MEM_ALLOC){
            enviar_pcb_actualizado_a_km(pcb);
        }
        if(pcb->registros.pc == pc_antiguo)
            pcb->registros.pc++;

        if(actualizar_contexto){
            enviar_pcb_actualizado_a_km(pcb);
        } 
        destruir_instruccion(instruccion_decodificada);
        free(instruccion);
}

char* fetch(t_pcb *pcb){

    t_paquete* paquete = crear_paquete(CPU_KM__FETCH);

    agregar_a_paquete(paquete, &(pcb->pid), sizeof(int));

    agregar_a_paquete(paquete, &(pcb->registros.pc), sizeof(uint32_t));

    enviar_paquete_y_liberarlo(paquete, conexion_kernel_memory);
    op_code cod_op = recibir_operacion(conexion_kernel_memory);

    if(cod_op != KM_CPU__INSTRUCCION){
        log_error(logger, "Error FETCH");
        return NULL;
    }
    t_list* lista = recibir_paquete(conexion_kernel_memory);

    char* instruccion = strdup(list_get(lista, 0));

    list_destroy_and_destroy_elements(lista, free);
    return instruccion;
}