#include "../src/main.h"
#include <assert.h>
#include <sys/socket.h>

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

static void test_destruir_stick_libera_sus_cadenas(void){
    t_stick* stick = iniciar_stick("127.0.0.1", "9000", 32, -1);
    assert(stick != NULL);
    destruir_stick(stick);
}

int main(void){
    logger = log_create("/tmp/cpu_tests.log", "cpu_tests", false, LOG_LEVEL_ERROR);
    printf("Running CPU tests...\n");
    assert(logger != NULL);

    test_mov_in_recibe_dato_y_libera_respuesta();
    test_mov_out_envia_dato_y_libera_buffer();
    test_destruir_stick_libera_sus_cadenas();

    log_destroy(logger);
    return EXIT_SUCCESS;
}