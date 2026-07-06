#include "tests.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "conexion_memoria.h"

void testear(){
    log_info(logger, "==== INICIANDO TESTS ====");
    int bytes = 256;
    char* bitmap = calloc(bytes, sizeof(char));
    t_bitarray *bitarray = bitarray_create_with_mode(bitmap, bytes, LSB_FIRST);

    
    log_info(logger, "==== FIN TESTS ====");
    exit(EXIT_SUCCESS);
}