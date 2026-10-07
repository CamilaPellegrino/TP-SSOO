#include "../src/main.h"
#include <errno.h>

extern t_bitarray* bitarray;
extern pthread_mutex_t m_bitarray;

static int cantidad_fallos = 0;

static void verificar(bool condicion, const char* descripcion){
	cantidad_fallos += verificar_test(condicion, descripcion);
}

static t_hueco* crear_hueco_prueba(uint32_t base, uint32_t tamanio){
	t_hueco* hueco = malloc(sizeof(*hueco));
	hueco->base = base;
	hueco->tamanio = tamanio;
	return hueco;
}

static void test_configuracion_y_fit(void){
	verificar(allocation_strategy_str_to_enum("BEST") == BEST_FIT, "reconoce BEST_FIT desde configuracion");
	verificar(allocation_strategy_str_to_enum("WORST") == WORST_FIT, "reconoce WORST_FIT desde configuracion");

	t_list* huecos = list_create();
	t_hueco* chico = crear_hueco_prueba(0, 16);
	t_hueco* mediano = crear_hueco_prueba(16, 32);
	t_hueco* grande = crear_hueco_prueba(48, 64);
	list_add(huecos, chico);
	list_add(huecos, mediano);
	list_add(huecos, grande);

	algoritmo_fit = BEST_FIT;
	verificar(ubicacion_de_proximo_segmento(huecos, 20) == mediano, "BEST_FIT elige el menor hueco suficiente");
	algoritmo_fit = WORST_FIT;
	verificar(ubicacion_de_proximo_segmento(huecos, 20) == grande, "WORST_FIT elige el mayor hueco suficiente");
	verificar(ubicacion_de_proximo_segmento(huecos, 65) == NULL, "rechaza una asignacion sin hueco suficiente");

	list_destroy_and_destroy_elements(huecos, free);
}

static void test_asignacion_direccion_y_liberacion(void){
	t_stick* stick = calloc(1, sizeof(*stick));
	stick->tamanio = 100;
	agregar_espacio_mem_thread_safe(100);
	agregar_stick(stick);

	t_pcb* pcb = calloc(1, sizeof(*pcb));
	pcb->pid = 42;
	t_proceso* proceso = iniciar_proceso(pcb, list_create());
	list_add(lista_procesos, proceso);

	t_segmento* primero = crear_segmento_thread_safe(proceso, 0, 20);
	t_segmento* segundo = crear_segmento_thread_safe(proceso, 1, 30);
	t_segmento* tercero = crear_segmento_thread_safe(proceso, 2, 10);
	verificar(primero != NULL && primero->base == 0, "asigna el primer segmento desde la base libre");
	verificar(segundo != NULL && segundo->base == 20 && tercero != NULL && tercero->base == 50,
	          "asigna segmentos contiguos y actualiza sus bases");
	verificar(list_size(lista_segmentos_global) == 3 && list_size(proceso->lista_segmentos) == 3,
	          "registra segmentos en las listas global y del proceso");
	verificar(calc_dir_fisica(42, 2, 4) == 54, "calcula la direccion fisica desde segmento y offset");
	verificar(crear_segmento_thread_safe(proceso, 3, 65) == NULL, "rechaza un segmento mayor al maximo configurado");
	verificar(crear_segmento_thread_safe(proceso, 3, 41) == NULL, "rechaza una asignacion que supera el espacio libre");

	verificar(eliminar_segmento_thread_safe(segundo, proceso), "libera un segmento perteneciente al proceso");
	verificar(list_size(lista_huecos) == 2 && tamanio_total_libre == 70,
	          "recupera el espacio liberado sin fusionar huecos separados");
	verificar(eliminar_segmento_thread_safe(primero, proceso), "libera el segmento inicial");
	verificar(list_size(lista_huecos) == 2 && ((t_hueco*)list_get(lista_huecos, 0))->base == 0 &&
	          ((t_hueco*)list_get(lista_huecos, 0))->tamanio == 50,
	          "fusiona huecos contiguos al liberar segmentos adyacentes");
	verificar(eliminar_segmento_thread_safe(tercero, proceso), "libera el ultimo segmento");
	verificar(list_size(lista_huecos) == 1 && ((t_hueco*)list_get(lista_huecos, 0))->base == 0 &&
	          ((t_hueco*)list_get(lista_huecos, 0))->tamanio == 100 && tamanio_total_libre == 100,
	          "coalesce todos los huecos y restaura la memoria disponible");

	list_remove_element(lista_procesos, proceso);
	list_destroy(proceso->instrucciones);
	list_destroy(proceso->lista_segmentos);
	pthread_mutex_destroy(&proceso->mutex);
	free(proceso->pcb);
	free(proceso);
	list_destroy_and_destroy_elements(lista_huecos, free);
	lista_huecos = list_create();
	list_destroy(lista_sticks);
	lista_sticks = list_create();
	free(stick);
	tamanio_total_mem = 0;
	tamanio_total_libre = 0;
}

static void test_particion_de_accesos_a_sticks(void){
	t_stick primero = {.tamanio = 40};
	t_stick segundo = {.tamanio = 60};
	list_add(lista_sticks, &primero);
	list_add(lista_sticks, &segundo);

	t_list* pedidos = pedidos_a_sticks_para_acceder_a(35, 20);
	verificar(list_size(pedidos) == 2, "divide un acceso que cruza dos sticks");
	if(list_size(pedidos) == 2){
		t_data_pedido_stick* pedido_a = list_get(pedidos, 0);
		t_data_pedido_stick* pedido_b = list_get(pedidos, 1);
		verificar(pedido_a->stick == &primero && pedido_a->base_en_stick == 35 && pedido_a->tamanio == 5,
		          "calcula el tramo local del primer stick");
		verificar(pedido_b->stick == &segundo && pedido_b->base_en_stick == 0 && pedido_b->tamanio == 15,
		          "calcula el tramo local del segundo stick");
	}
	list_destroy_and_destroy_elements(pedidos, free);

	pedidos = pedidos_a_sticks_para_acceder_a(40, 12);
	verificar(list_size(pedidos) == 1 && ((t_data_pedido_stick*)list_get(pedidos, 0))->stick == &segundo,
	          "respeta el limite entre sticks al iniciar una lectura");
	list_destroy_and_destroy_elements(pedidos, free);
	list_clean(lista_sticks);
}

static void test_bitmap_y_bloques_swap(void){
	char contenido[] = "ABCDEFGHIJ";
	verificar(obtener_primer_bloque_libre() == 0, "encuentra el primer bloque disponible");
	int mutex_disponible = pthread_mutex_trylock(&m_bitarray);
	verificar(mutex_disponible == 0, "libera el mutex del bitmap al encontrar un bloque");
	if(mutex_disponible == 0){
		pthread_mutex_unlock(&m_bitarray);
	}

	t_list* bloques = bloques_necesarios_para_escribir_en_swap(contenido, 10);
	verificar(bloques != NULL && list_size(bloques) == 3, "reserva los bloques necesarios para un contenido");
	if(bloques != NULL && list_size(bloques) == 3){
		t_data_escritura_bloque* bloque_a = list_get(bloques, 0);
		t_data_escritura_bloque* bloque_b = list_get(bloques, 1);
		t_data_escritura_bloque* bloque_c = list_get(bloques, 2);
		verificar(bloque_a->num_bloque == 0 && bloque_b->num_bloque == 1 && bloque_c->num_bloque == 2,
		          "asigna los primeros bloques libres en orden");
		verificar(bloque_a->tamanio == 4 && bloque_b->tamanio == 4 && bloque_c->tamanio == 2 &&
		          memcmp(bloque_a->contenido, "ABCD", 4) == 0 &&
		          memcmp(bloque_b->contenido, "EFGH", 4) == 0 &&
		          memcmp(bloque_c->contenido, "IJ", 2) == 0,
		          "divide el contenido y conserva el tamanio del bloque final");
		verificar(cant_bloques_libres() == 5, "actualiza el contador de bloques libres");
	}
	if(bloques != NULL){
		list_destroy_and_destroy_elements(bloques, destruir_bloque);
	}

	char contenido_grande[28] = {0};
	bloques = bloques_necesarios_para_escribir_en_swap(contenido_grande, sizeof(contenido_grande));
	verificar(bloques != NULL && list_size(bloques) == 7, "reserva correctamente una solicitud cercana al limite");
	t_list* insuficientes = bloques_necesarios_para_escribir_en_swap(contenido, sizeof(contenido));
	verificar(insuficientes == NULL && cant_bloques_libres() == 1 && !bloques_libres_suficientes(2),
	          "rechaza una reserva insuficiente sin consumir bloques parcialmente");
	if(bloques != NULL){
		list_destroy_and_destroy_elements(bloques, destruir_bloque);
	}
	verificar(cant_bloques_libres() == 8, "libera los bloques swap al destruir sus reservas");
}

int main(void){
	logger = log_create("/tmp/kernel_memory_tests.log", "kernel_memory_tests", false, LOG_LEVEL_ERROR);
	if(logger == NULL){
		return EXIT_FAILURE;
	}
	printf("Running kernel_memory tests...\n");

	lista_sticks = list_create();
	lista_segmentos_global = list_create();
	lista_huecos = list_create();
	lista_procesos = list_create();
	lista_eventos_stick = list_create();
	pthread_mutex_init(&m_lista_segmentos_global, NULL);
	pthread_mutex_init(&m_lista_procesos, NULL);
	pthread_mutex_init(&m_lista_huecos, NULL);
	pthread_mutex_init(&m_lista_eventos_stick, NULL);
	pthread_mutex_init(&m_manejar_memoria, NULL);
	pthread_mutex_init(&m_bitarray, NULL);
	sem_init(&s_lista_eventos_stick, 0, 0);
	sem_init(&s_fin_mover, 0, 0);
	segment_max_size = 64;
	compaction_delay_ms = 0;
	algoritmo_fit = BEST_FIT;
	tamanio_total_mem = 0;
	tamanio_total_libre = 0;
	tam_bloque = 4;
	cant_bloques = 8;
	int bytes_bitmap = (cant_bloques + 7) / 8;
	bitarray = bitarray_create_with_mode(calloc(bytes_bitmap, sizeof(char)), bytes_bitmap, LSB_FIRST);
	if(bitarray == NULL){
		log_destroy(logger);
		return EXIT_FAILURE;
	}

	test_configuracion_y_fit();
	test_asignacion_direccion_y_liberacion();
	test_particion_de_accesos_a_sticks();
	test_bitmap_y_bloques_swap();

	bitarray_destroy(bitarray);
	sem_destroy(&s_lista_eventos_stick);
	sem_destroy(&s_fin_mover);
	pthread_mutex_destroy(&m_lista_segmentos_global);
	pthread_mutex_destroy(&m_lista_procesos);
	pthread_mutex_destroy(&m_lista_huecos);
	pthread_mutex_destroy(&m_lista_eventos_stick);
	pthread_mutex_destroy(&m_manejar_memoria);
	pthread_mutex_destroy(&m_bitarray);
	list_destroy(lista_sticks);
	list_destroy(lista_segmentos_global);
	list_destroy(lista_huecos);
	list_destroy(lista_procesos);
	list_destroy(lista_eventos_stick);
	log_destroy(logger);
	if(cantidad_fallos > 0){
		fprintf(stderr, "%d pruebas fallaron\n", cantidad_fallos);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
