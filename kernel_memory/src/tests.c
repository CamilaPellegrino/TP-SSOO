#include "tests.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

void print_ok(const char* nombre);
void print_fail(const char* nombre);

void assert_true(bool cond, const char* msg);
void assert_eq_u32(uint32_t a, uint32_t b, const char* msg);

t_proceso* crear_proceso_test(int pid);
void resetear_contexto_tests();
void test_compactacion_sin_fragmentacion();
void test_compactacion_basica();
void test_compactacion_multiple();
void test_compactacion_integral();

void testear(){
    log_info(logger, "==== INICIANDO TESTS ====");
    test_compactacion_sin_fragmentacion();
    test_compactacion_basica();
    test_compactacion_multiple();
    test_compactacion_integral();
    log_info(logger, "==== FIN TESTS ====");
    exit(EXIT_SUCCESS);
}
// HELPERS

void print_ok(const char* nombre){
    printf("[OK] %s\n", nombre);
}

void print_fail(const char* nombre){
    printf("[FAIL] %s\n", nombre);
}

void assert_true(bool cond, const char* msg){
    if(!cond){
        print_fail(msg);
        assert(cond);
    }
}

void assert_eq_u32(uint32_t a, uint32_t b, const char* msg){
    if(a != b){
        printf("Esperado: %u\n", b);
        printf("Recibido: %u\n", a);
        print_fail(msg);
        assert(a == b);
    }
}

// HELPERS DE CREACION
t_proceso* crear_proceso_test(int pid){
    t_proceso* p = malloc(sizeof(t_proceso));
    p->pcb = malloc(sizeof(t_pcb));
    p->pcb->pid = pid;
    p->instrucciones = list_create();
    p->lista_segmentos = list_create();
    return p;
}
void resetear_contexto_tests(){
    while(!list_is_empty(lista_huecos)){
        t_hueco* h = list_remove(lista_huecos, 0);
        free(h);
    }
    while(!list_is_empty(lista_segmentos_global)){
        t_segmento* s = list_remove(lista_segmentos_global, 0);
        free(s);
    }
    crear_hueco(0, 600);
}

void imprimir_estado_error(){
    printf("SEGMENTOS:\n");
    imprimir_segmentos();
    printf("HUECOS:\n");
    imprimir_huecos();
}

// TEST 1

void test_compactacion_sin_fragmentacion(){
    resetear_contexto_tests();
    t_proceso* p1 = crear_proceso_test(1);
    t_segmento* segA = crear_segmento(p1, 1, 100);
    t_segmento* segB = crear_segmento(p1, 2, 100);
    uint32_t baseA_antes = segA->base;
    uint32_t baseB_antes = segB->base;
    compactar_memoria();
    if(segA->base != baseA_antes){
        printf("ERROR test_compactacion_sin_fragmentacion\n");
        printf("segA deberia quedar en %u y quedo en %u\n", baseA_antes, segA->base);
        imprimir_estado_error();
        assert(false);
    }
    if(segB->base != baseB_antes){
        printf("ERROR test_compactacion_sin_fragmentacion\n");
        printf("segB deberia quedar en %u y quedo en %u\n", baseB_antes, segB->base);
        imprimir_estado_error();
        assert(false);
    }
    print_ok("test_compactacion_sin_fragmentacion");
}

// TEST 2

void test_compactacion_basica(){
    resetear_contexto_tests();
    t_proceso* p1 = crear_proceso_test(1);
    t_segmento* segA = crear_segmento(p1, 1, 100);
    t_segmento* segB = crear_segmento(p1, 2, 100);
    t_segmento* segC = crear_segmento(p1, 3, 100);
    eliminar_segmento(segB, p1);
    compactar_memoria();
    if(segA->base != 0){
        printf("ERROR test_compactacion_basica\n");
        printf("segA deberia quedar en 0 y quedo en %u\n", segA->base);
        imprimir_estado_error();
        assert(false);
    }
    if(segC->base != 100){
        printf("ERROR test_compactacion_basica\n");
        printf("segC deberia quedar en 100 y quedo en %u\n", segC->base);
        printf("segA termina en %u\n", segA->base + segA->tamanio - 1);
        imprimir_estado_error();
        assert(false);
    }
    print_ok("test_compactacion_basica");
}

// TEST 3

void test_compactacion_multiple(){
    resetear_contexto_tests();
    t_proceso* p1 = crear_proceso_test(1);
    t_segmento* A = crear_segmento(p1, 1, 100);
    t_segmento* B = crear_segmento(p1, 2, 100);
    t_segmento* C = crear_segmento(p1, 3, 100);
    t_segmento* D = crear_segmento(p1, 4, 100);
    eliminar_segmento(B, p1);
    eliminar_segmento(D, p1);
    compactar_memoria();
    if(A->base != 0){
        printf("ERROR test_compactacion_multiple\n");
        printf("A deberia quedar en 0 y quedo en %u\n", A->base);
        imprimir_estado_error();
        assert(false);
    }
    if(C->base != 100){
        printf("ERROR test_compactacion_multiple\n");
        printf("C deberia quedar en 100 y quedo en %u\n", C->base);
        imprimir_estado_error();
        assert(false);
    }
    print_ok("test_compactacion_multiple");
}

// TEST 4

void test_compactacion_integral(){
    resetear_contexto_tests();
    t_proceso* p1 = crear_proceso_test(1);
    t_proceso* p2 = crear_proceso_test(2);
    t_segmento* A = crear_segmento(p1, 1, 50);
    t_segmento* B = crear_segmento(p2, 1, 50);
    t_segmento* C = crear_segmento(p1, 2, 50);
    t_segmento* D = crear_segmento(p2, 2, 50);
    t_segmento* E = crear_segmento(p1, 3, 50);
    t_segmento* F = crear_segmento(p2, 3, 50);
    eliminar_segmento(B, p2);
    eliminar_segmento(D, p2);
    eliminar_segmento(F, p2);
    compactar_memoria();
    if(A->base != 0){
        printf("ERROR test_compactacion_integral\n");
        printf("A deberia quedar en 0 y quedo en %u\n", A->base);
        imprimir_estado_error();
        assert(false);
    }
    if(C->base != 50){
        printf("ERROR test_compactacion_integral\n");
        printf("C deberia quedar en 50 y quedo en %u\n", C->base);
        imprimir_estado_error();
        assert(false);
    }
    if(E->base != 100){
        printf("ERROR test_compactacion_integral\n");
        printf("E deberia quedar en 100 y quedo en %u\n", E->base);
        imprimir_estado_error();
        assert(false);
    }
    uint32_t fin_A = A->base + A->tamanio;
    uint32_t fin_C = C->base + C->tamanio;
    if(C->base != fin_A){
        printf("ERROR test_compactacion_integral\n");
        printf("A y C no quedaron contiguos\n");
        imprimir_estado_error();
        assert(false);
    }
    if(E->base != fin_C){
        printf("ERROR test_compactacion_integral\n");
        printf("C y E no quedaron contiguos\n");
        imprimir_estado_error();
        assert(false);
    }
    if(!segmento_del_proceso(A, p1)){
        printf("ERROR test_compactacion_integral\n");
        printf("A ya no pertenece a p1\n");
        assert(false);
    }
    if(!segmento_del_proceso(C, p1)){
        printf("ERROR test_compactacion_integral\n");
        printf("C ya no pertenece a p1\n");
        assert(false);
    }
    if(!segmento_del_proceso(E, p1)){
        printf("ERROR test_compactacion_integral\n");
        printf("E ya no pertenece a p1\n");
        assert(false);
    }
    if(list_size(lista_huecos) != 1){
        printf("ERROR test_compactacion_integral\n");
        printf("Deberia quedar un solo hueco y quedaron %d\n", list_size(lista_huecos));
        imprimir_estado_error();
        assert(false);
    }
    t_hueco* h = list_get(lista_huecos, 0);
    if(h->base != 150){
        printf("ERROR test_compactacion_integral\n");
        printf("Hueco final deberia arrancar en 150 y arranca en %d\n", h->base);
        imprimir_estado_error();
        assert(false);
    }
    print_ok("test_compactacion_integral");
}