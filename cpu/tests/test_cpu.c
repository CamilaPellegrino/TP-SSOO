/* 
 * Tests de modulo cpu
 * Compilacion (desde cpu/tests): make -C .. test
 * Archivo compilado en cpu/bin
*/


#include "../src/main.h"
#include "../src/ciclo_instruccion/decode.h"
#include "../src/ciclo_instruccion/execute.h"
#include <assert.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static int cantidad_fallos = 0;

static void verificar(bool condicion, const char *descripcion){
    if(!condicion){
        fprintf(stderr, "[FALLO]: %s\n", descripcion);
        cantidad_fallos++;
    }else{
        fprintf(stderr, "[OK]: %s\n", descripcion);

    }
}

typedef struct {
    int fd;
    op_code expected_operation;
    uint32_t read_value;
    uint32_t expected_write_value;
} t_mock_km_request;

static void* atender_pedido_mock(void* arg){
    t_mock_km_request* request = arg;
    assert(recibir_operacion(request->fd) == request->expected_operation);
    t_list* payload = recibir_paquete(request->fd);

    if(request->expected_operation == CPU_KM__MOV_OUT){
        uint8_t expected_bytes[sizeof(uint32_t)] = {
            (request->expected_write_value >> 24) & 0xFF,
            (request->expected_write_value >> 16) & 0xFF,
            (request->expected_write_value >> 8) & 0xFF,
            request->expected_write_value & 0xFF
        };
        assert(list_size(payload) == 4);
        assert(memcmp(list_get(payload, 2), expected_bytes, sizeof(expected_bytes)) == 0);
        enviar_operacion(request->fd, KM_CPU__RESPUESTA);
    }else if(request->expected_operation == CPU_KM__COPY_MEM){
        assert(list_size(payload) == 4);
        assert(*(int *)list_get(payload, 0) == 42);
        t_dir_fisica *source = list_get(payload, 1);
        t_dir_fisica *destination = list_get(payload, 2);
        assert(source->id_segmento == 0 && source->offset == 4);
        assert(destination->id_segmento == 0 && destination->offset == 8);
        assert(*(uint32_t *)list_get(payload, 3) == request->expected_write_value);
        enviar_operacion(request->fd, KM_CPU__RESPUESTA);
    }else{
        t_paquete* response = crear_paquete(KM_CPU__RESPUESTA);
        agregar_a_paquete(response, &request->read_value, sizeof(request->read_value));
        enviar_paquete_y_liberarlo(response, request->fd);
    }

    list_destroy_and_destroy_elements(payload, free);
    return NULL;
}

static void preparar_pcb(t_pcb* pcb){
    memset(pcb, 0, sizeof(*pcb));
    pcb->pid = 42;
    pcb->tabla_segmentos = list_create();
    t_segmento* segment = malloc(sizeof(*segment));
    segment->pid = pcb->pid;
    segment->id_segmento = 0;
    segment->base = 100;
    segment->tamanio = 256;
    list_add(pcb->tabla_segmentos, segment);
}

static t_instruccion_decodificada* instruccion_para_registro(void* reg, int tamanio){
    t_instruccion_decodificada* instruction = calloc(1, sizeof(*instruction));
    assert(instruction != NULL);
    instruction->registros = list_create();
    especificacion_registro* register_spec = malloc(sizeof(*register_spec));
    register_spec->ptro_reg = reg;
    register_spec->tamanio = tamanio;
    list_add(instruction->registros, register_spec);
    return instruction;
}

static void test_mov_in_recibe_dato_y_libera_respuesta(void){
    t_pcb pcb;
    preparar_pcb(&pcb);
    seg_max_size_global = 256;
    pcb.registros.si = 0;
    t_instruccion_decodificada* instruction = instruccion_para_registro(&pcb.registros.eax, sizeof(pcb.registros.eax));
    t_mock_km_request request = {
        .fd = -1,
        .expected_operation = CPU_KM__MOV_IN,
        .read_value = 0x12345678
    };

    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    request.fd = sockets[1];
    conexion_kernel_memory = sockets[0];
    pthread_t mock_thread;
    assert(pthread_create(&mock_thread, NULL, atender_pedido_mock, &request) == 0);
    ejecutar_mov_in(instruction, &pcb);
    assert(pthread_join(mock_thread, NULL) == 0);
    assert(pcb.registros.eax == request.read_value);

    close(sockets[0]);
    close(sockets[1]);
    destruir_instruccion(instruction);
    list_destroy_and_destroy_elements(pcb.tabla_segmentos, free);
}

static void test_mov_out_envia_dato_y_libera_buffer(void){
    t_pcb pcb;
    preparar_pcb(&pcb);
    seg_max_size_global = 256;
    pcb.registros.di = 4;
    pcb.registros.eax = 0x12345678;
    t_instruccion_decodificada* instruction = instruccion_para_registro(&pcb.registros.eax, sizeof(pcb.registros.eax));
    t_mock_km_request request = {
        .fd = -1,
        .expected_operation = CPU_KM__MOV_OUT,
        .expected_write_value = pcb.registros.eax
    };

    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    request.fd = sockets[1];
    conexion_kernel_memory = sockets[0];
    pthread_t mock_thread;
    assert(pthread_create(&mock_thread, NULL, atender_pedido_mock, &request) == 0);
    ejecutar_mov_out(instruction, &pcb);
    assert(pthread_join(mock_thread, NULL) == 0);

    close(sockets[0]);
    close(sockets[1]);
    destruir_instruccion(instruction);
    list_destroy_and_destroy_elements(pcb.tabla_segmentos, free);
}

static void test_copy_mem_envia_direcciones_y_tamanio(void){
    t_pcb pcb;
    preparar_pcb(&pcb);
    seg_max_size_global = 256;
    pcb.registros.si = 4;
    pcb.registros.di = 8;
    pcb.registros.eax = 4;
    t_instruccion_decodificada* instruction = decode("COPY_MEM EAX", &pcb);
    assert(instruction != NULL);
    t_mock_km_request request = {
        .fd = -1,
        .expected_operation = CPU_KM__COPY_MEM,
        .expected_write_value = 4
    };

    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    request.fd = sockets[1];
    conexion_kernel_memory = sockets[0];
    pthread_t mock_thread;
    assert(pthread_create(&mock_thread, NULL, atender_pedido_mock, &request) == 0);
    assert(!execute(instruction, &pcb));
    assert(pthread_join(mock_thread, NULL) == 0);

    close(sockets[0]);
    close(sockets[1]);
    destruir_instruccion(instruction);
    list_destroy_and_destroy_elements(pcb.tabla_segmentos, free);
}

static void test_destruir_stick_libera_sus_cadenas(void){
    t_stick* stick = iniciar_stick("127.0.0.1", "9000", 32, -1);
    assert(stick != NULL);
    destruir_stick(stick);
}

static void test_decode_instrucciones_soportadas(void){
    static const struct {
        const char *texto;
        instrucciones tipo;
        int cantidad_operandos;
    } casos[] = {
        {"NOOP", I_NOOP, 0},
        {"SET AX 7", I_SET, 2},
        {"SUM EAX EBX", I_SUM, 2},
        {"SUB AX BX", I_SUB, 2},
        {"JNZ AX 3", I_JNZ, 2},
        {"COPY_MEM EAX", I_COPY_MEM, 1},
        {"MOV_IN EAX", I_MOV_IN, 1},
        {"MOV_OUT AX", I_MOV_OUT, 1},
        {"MUTEX_CREATE candado", I_MUTEX_CREATE, 1},
        {"MUTEX_LOCK candado", I_MUTEX_LOCK, 1},
        {"MUTEX_UNLOCK candado", I_MUTEX_UNLOCK, 1},
        {"MEM_ALLOC 1 64", I_MEM_ALLOC, 2},
        {"MEM_FREE 1", I_MEM_FREE, 1},
        {"SLEEP 2", I_SLEEP, 1},
        {"STDOUT SI EAX", I_STDOUT, 2},
        {"STDIN SI EAX", I_STDIN, 2},
        {"INIT_PROC programa.prc 1", I_INIT_PROC, 2},
        {"EXIT", I_EXIT, 1}
    };
    t_pcb pcb;
    preparar_pcb(&pcb);

    for(size_t i = 0; i < sizeof(casos) / sizeof(casos[0]); i++){
        t_instruccion_decodificada *instruction = decode((char *)casos[i].texto, &pcb);
        assert(instruction != NULL);
        assert(instruction->tipo == casos[i].tipo);
        assert(list_size(instruction->registros) == casos[i].cantidad_operandos);
        if(casos[i].tipo == I_EXIT)
            assert(*(t_status_op *)list_get(instruction->registros, 0) == OK);
        destruir_instruccion(instruction);
    }

    list_destroy_and_destroy_elements(pcb.tabla_segmentos, free);
}

static bool decode_devuelve_instruccion_invalida(char *texto, t_pcb *pcb){
    t_instruccion_decodificada *instruction = decode(texto, pcb);
    if(instruction == NULL)
        return false;
    bool invalid = instruction->tipo == I_EXIT &&
                   list_size(instruction->registros) == 1 &&
                   *(t_status_op *)list_get(instruction->registros, 0) == INSTRUCCION_INVALIDA;
    destruir_instruccion(instruction);
    return invalid;
}

static void test_decode_errores_con_status_correcto(void){
    t_pcb pcb;
    preparar_pcb(&pcb);

    verificar(decode_devuelve_instruccion_invalida("OPCODE_DESCONOCIDO", &pcb),
              "opcode desconocido debe producir INSTRUCCION_INVALIDA");
    verificar(decode_devuelve_instruccion_invalida(NULL, &pcb),
              "instruccion NULL debe producir INSTRUCCION_INVALIDA");
    verificar(decode_devuelve_instruccion_invalida("SUM REGISTRO_FALSO AX", &pcb),
              "registro desconocido debe producir INSTRUCCION_INVALIDA");
    verificar(decode_devuelve_instruccion_invalida("SET AX no_es_numero", &pcb),
              "operando numerico invalido debe producir INSTRUCCION_INVALIDA");

    list_destroy_and_destroy_elements(pcb.tabla_segmentos, free);
}

static void test_decode_aridad_invalida_no_termina_el_proceso(void){
    pid_t child = fork();
    assert(child >= 0);
    if(child == 0){
        t_pcb pcb;
        memset(&pcb, 0, sizeof(pcb));
        t_instruccion_decodificada *instruction = decode("SET AX", &pcb);
        bool invalid = instruction != NULL && instruction->tipo == I_EXIT &&
                       list_size(instruction->registros) == 1 &&
                       *(t_status_op *)list_get(instruction->registros, 0) == INSTRUCCION_INVALIDA;
        if(instruction != NULL)
            destruir_instruccion(instruction);
        _exit(invalid ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    int status;
    assert(waitpid(child, &status, 0) == child);
    verificar(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS,
              "instruccion truncada debe responder con INSTRUCCION_INVALIDA sin crash");
}

static void test_execute_instrucciones_de_registros(void){
    t_pcb pcb;
    preparar_pcb(&pcb);

    t_instruccion_decodificada *instruction = decode("SET AX 260", &pcb);
    assert(instruction != NULL);
    assert(!execute(instruction, &pcb));
    assert(pcb.registros.ax == 4);
    destruir_instruccion(instruction);

    pcb.registros.eax = 10;
    pcb.registros.ebx = 7;
    instruction = decode("SUM EAX EBX", &pcb);
    assert(instruction != NULL);
    assert(!execute(instruction, &pcb));
    assert(pcb.registros.eax == 17);
    destruir_instruccion(instruction);

    pcb.registros.eax = 2;
    pcb.registros.ebx = 3;
    instruction = decode("SUB EAX EBX", &pcb);
    assert(instruction != NULL);
    assert(!execute(instruction, &pcb));
    assert(pcb.registros.eax == UINT32_MAX);
    destruir_instruccion(instruction);

    pcb.registros.pc = 9;
    pcb.registros.ax = 1;
    instruction = decode("JNZ AX 4", &pcb);
    assert(instruction != NULL);
    assert(!execute(instruction, &pcb));
    assert(pcb.registros.pc == 4);
    destruir_instruccion(instruction);

    pcb.registros.pc = 9;
    pcb.registros.ax = 0;
    instruction = decode("JNZ AX 4", &pcb);
    assert(instruction != NULL);
    assert(!execute(instruction, &pcb));
    assert(pcb.registros.pc == 9);
    destruir_instruccion(instruction);

    instruction = decode("NOOP", &pcb);
    assert(instruction != NULL);
    assert(!execute(instruction, &pcb));
    destruir_instruccion(instruction);

    list_destroy_and_destroy_elements(pcb.tabla_segmentos, free);
}

static void test_execute_rechaza_tipos_sin_ejecutor(void){
    t_instruccion_decodificada instruction = {.tipo = I_SET_PC, .registros = NULL};
    t_pcb pcb;
    memset(&pcb, 0, sizeof(pcb));
    verificar(!execute(&instruction, &pcb), "I_SET_PC no tiene que ejecutar una funcion");
    instruction.tipo = (instrucciones)1000;
    verificar(!execute(&instruction, &pcb), "tipo fuera del enum debe rechazarse");
}

static void test_mmu_limites(void){
    t_pcb pcb;
    preparar_pcb(&pcb);
    seg_max_size_global = 256;

    assert(mmu(&pcb, 1, 0, 0) == 100);
    assert(mmu(&pcb, 4, 0, 252) == 352);
    assert(mmu(&pcb, 1, 0, 256) == -1);
    assert(mmu(&pcb, 5, 0, 252) == -1);
    assert(mmu(&pcb, 1, 1, 0) == -1);
    verificar(mmu(&pcb, UINT32_MAX, 0, 1) == -1, 
        "MMU debe rechazar tamanio que desborda al sumarse al desplazamiento");

    list_destroy_and_destroy_elements(pcb.tabla_segmentos, free);
}

static void test_syscall_scheduler(
    const char *texto, op_code operacion, 
    int cantidad_datos, t_estado_cpu estado_esperado, 
    bool actualiza_contexto, 
    bool pide_segmentos
){
    t_pcb pcb;
    preparar_pcb(&pcb);
    pcb.registros.eax = 4;
    seg_max_size_global = 256;
    set_pedir_segmentos(false);

    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    conexion_kernel_scheduler = sockets[0];

    t_instruccion_decodificada *instruction = decode((char *)texto, &pcb);
    assert(instruction != NULL);
    assert(execute(instruction, &pcb) == actualiza_contexto);
    assert(estado_cpu == estado_esperado);
    assert(get_pedir_segmentos() == pide_segmentos);
    assert(recibir_operacion(sockets[1]) == operacion);
    t_list *payload = recibir_paquete(sockets[1]);
    assert(payload != NULL);
    assert(list_size(payload) == cantidad_datos);

    if(operacion == CPU_SCH__MUTEX_CREATE || operacion == CPU_SCH__MUTEX_LOCK ||
       operacion == CPU_SCH__MUTEX_UNLOCK)
        assert(strcmp(list_get(payload, 0), "mutex") == 0);
    if(operacion == CPU_SCH__INIT_PROC){
        assert(strcmp(list_get(payload, 0), "programa.prc") == 0);
        assert(*(int *)list_get(payload, 1) == 5);
    }
    if(operacion == CPU_SCH__SLEEP)
        assert(*(int *)list_get(payload, 0) == 3);
    if(operacion == CPU_SCH__MEM_ALLOC){
        assert(*(int *)list_get(payload, 0) == 2);
        assert(*(int *)list_get(payload, 1) == 64);
    }
    if(operacion == CPU_SCH__MEM_FREE)
        assert(*(int *)list_get(payload, 0) == 2);

    list_destroy_and_destroy_elements(payload, free);
    destruir_instruccion(instruction);
    close(sockets[0]);
    close(sockets[1]);
    list_destroy_and_destroy_elements(pcb.tabla_segmentos, free);
}

static void test_syscalls_envian_opcode_y_actualizan_estado(void){
    test_syscall_scheduler("MUTEX_CREATE mutex", CPU_SCH__MUTEX_CREATE, 1,
                           WAIT_SYS, false, false);
    test_syscall_scheduler("MUTEX_LOCK mutex", CPU_SCH__MUTEX_LOCK, 1,
                           WAIT_SYS, false, false);
    test_syscall_scheduler("MUTEX_UNLOCK mutex", CPU_SCH__MUTEX_UNLOCK, 1,
                           WAIT_SYS, false, false);
    test_syscall_scheduler("INIT_PROC programa.prc 5", CPU_SCH__INIT_PROC, 2,
                           WAIT_SYS, false, false);
    test_syscall_scheduler("MEM_ALLOC 2 64", CPU_SCH__MEM_ALLOC, 2,
                           WAIT_MEM_ALLOC, false, true);
    test_syscall_scheduler("MEM_FREE 2", CPU_SCH__MEM_FREE, 1,
                           WAIT_SYS, false, true);
    test_syscall_scheduler("SLEEP 3", CPU_SCH__SLEEP, 1,
                           LIBRE, true, false);
    test_syscall_scheduler("STDIN SI EAX", CPU_SCH__STDIN, 2,
                           LIBRE, true, false);
    test_syscall_scheduler("STDOUT SI EAX", CPU_SCH__STDOUT, 2,
                           LIBRE, true, false);
}

static void test_exit_y_seg_fault_envian_status_correcto(const char *texto,
                                                         t_status_op status_esperado){
    t_pcb pcb;
    preparar_pcb(&pcb);
    seg_max_size_global = 256;
    if(strcmp(texto, "MOV_IN EAX") == 0)
        list_clean_and_destroy_elements(pcb.tabla_segmentos, free);

    int km_sockets[2];
    int sch_sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, km_sockets) == 0);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sch_sockets) == 0);
    conexion_kernel_memory = km_sockets[0];
    conexion_kernel_scheduler = sch_sockets[0];

    t_mock_km_request request = {
        .fd = km_sockets[1],
        .expected_operation = CPU_KM__ACTUALIZAR_PCB
    };
    pthread_t mock_thread;
    assert(pthread_create(&mock_thread, NULL, atender_pedido_mock, &request) == 0);

    if(strcmp(texto, "MOV_IN EAX") == 0){
        t_instruccion_decodificada *instruction = decode((char *)texto, &pcb);
        assert(instruction != NULL);
        assert(!execute(instruction, &pcb));
        destruir_instruccion(instruction);
    }else{
        t_instruccion_decodificada *instruction = decode((char *)texto, &pcb);
        assert(instruction != NULL);
        assert(!execute(instruction, &pcb));
        destruir_instruccion(instruction);
    }

    assert(pthread_join(mock_thread, NULL) == 0);
    assert(recibir_operacion(sch_sockets[1]) == CPU_SCH__EXIT);
    t_list *payload = recibir_paquete(sch_sockets[1]);
    assert(payload != NULL && list_size(payload) == 1);
    assert(*(t_status_op *)list_get(payload, 0) == status_esperado);
    list_destroy_and_destroy_elements(payload, free);

    close(km_sockets[0]);
    close(km_sockets[1]);
    close(sch_sockets[0]);
    close(sch_sockets[1]);
    list_destroy_and_destroy_elements(pcb.tabla_segmentos, free);
}

int main(void){
    logger = log_create("/tmp/cpu_tests.log", "cpu_tests", false, LOG_LEVEL_ERROR);
    printf("Running CPU tests...\n");
    assert(logger != NULL);
    inicializar_variables();

    test_mov_in_recibe_dato_y_libera_respuesta();
    test_mov_out_envia_dato_y_libera_buffer();
    test_copy_mem_envia_direcciones_y_tamanio();
    test_destruir_stick_libera_sus_cadenas();
    test_decode_instrucciones_soportadas();
    test_decode_errores_con_status_correcto();
    test_decode_aridad_invalida_no_termina_el_proceso();
    test_execute_instrucciones_de_registros();
    test_execute_rechaza_tipos_sin_ejecutor();
    test_mmu_limites();
    test_syscalls_envian_opcode_y_actualizan_estado();
    test_exit_y_seg_fault_envian_status_correcto("EXIT", OK);
    test_exit_y_seg_fault_envian_status_correcto("MOV_IN EAX", SEG_FAULT);

    if(cantidad_fallos > 0){
        fprintf(stderr, "%d pruebas fallaron\n", cantidad_fallos);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    log_destroy(logger);
    return EXIT_SUCCESS;
}