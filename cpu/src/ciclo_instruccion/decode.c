#include "ciclo_instruccion.h"
#include <limits.h>

typedef bool (*t_decodificador)(char **partes, t_pcb *pcb,
								t_instruccion_decodificada *instruccion);

typedef struct {
	char *nombre;
	t_decodificador decodificar;
} t_decodificador_instruccion;

typedef bool (*t_funcion_agregar)(int posicion, char *valor, t_pcb *pcb,
								  t_list *lista_registros);

bool agregar_numero(int posicion, char *valor, t_pcb *pcb, t_list *lista_registros) {
    (void)pcb;

    if (valor == NULL || lista_registros == NULL ||
        posicion < 0 || (size_t)posicion > list_size(lista_registros))
        return false;

		char* nro_en_string;
    long numero = strtol(valor, &nro_en_string, 10);

	if (nro_en_string == valor)
        return false;
    int* numero_ptr = malloc(sizeof(*numero_ptr));
    if (numero_ptr == NULL)
        return false;
    *numero_ptr = (int)numero;
    list_add_in_index(lista_registros, posicion, numero_ptr);
    return true;
}

bool agregar_string(int posicion, char *valor, t_pcb *pcb, t_list *lista_registros){
	(void)pcb;
	if(valor == NULL || lista_registros == NULL || posicion < 0 ||
	   (size_t)posicion > list_size(lista_registros))
		return false;
	char *cadena = strdup(valor);
	if(cadena == NULL)
		return false;
	list_add_in_index(lista_registros, posicion, cadena);
	return true;
}

bool agregar_registro(int posicion, char *nombre_registro, t_pcb *pcb,
					  t_list *lista_registros){
	if(nombre_registro == NULL || pcb == NULL || lista_registros == NULL || posicion < 0 ||
	   (size_t)posicion > list_size(lista_registros))
		return false;
	especificacion_registro *registro = obtener_registro(nombre_registro, pcb);
	if(registro != NULL){
		list_add_in_index(lista_registros, posicion, registro);
		return true;
	}
	return false;
}
bool agregar_todos(t_funcion_agregar funciones[], size_t cantidad, t_list *lista_registros,
				   char **partes, t_pcb *pcb){
	if(funciones == NULL || lista_registros == NULL || partes == NULL ||
	   cantidad == 0 || partes[0] == NULL)
		return false;
	for(size_t i = 0; i < cantidad; i++){
		if(funciones[i] == NULL || partes[i + 1] == NULL)
			return false;
	}
	for(size_t i = 0; i < cantidad; i++){
		size_t posicion = list_size(lista_registros);
		if(posicion > INT_MAX)
			return false;
		bool ejecucion_correcta = funciones[i]((int)posicion, partes[i + 1], pcb, lista_registros);
		if(!ejecucion_correcta){
			return false;
		}
	}
	return true;
}


static bool decode_set(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_SET;
	t_funcion_agregar funciones[] = {agregar_registro, agregar_numero};
    return agregar_todos(funciones, sizeof(funciones) / sizeof(funciones[0]),
			  instruccion->registros, partes, pcb);
}

static bool decode_noop(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)partes; (void)pcb;
	instruccion->tipo = I_NOOP;
	return true;
}

static bool decode_sum(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_SUM;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	list_add(instruccion->registros, obtener_registro(partes[2], pcb));
	return true;
}

static bool decode_sub(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_SUB;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	list_add(instruccion->registros, obtener_registro(partes[2], pcb));
	return true;
}

static bool decode_jnz(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_JNZ;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	int *numero = malloc(sizeof(int));
	*numero = (int)strtol(partes[2], NULL, 10);
	list_add(instruccion->registros, numero);
	return true;
}

static bool decode_mov_out(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_MOV_OUT;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	return true;
}

static bool decode_mov_in(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_MOV_IN;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	return true;
}
  
static bool decode_exit(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)partes; (void)pcb;
	iniciar_instr_exit(instruccion, OK);
	return true;
}

static bool decode_sleep(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_SLEEP;
	int *numero = malloc(sizeof(int));
	*numero = (int)strtol(partes[1], NULL, 10);
	list_add(instruccion->registros, numero);
	return true;
}

static bool decode_stdin(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_STDIN;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	list_add(instruccion->registros, obtener_registro(partes[2], pcb));
	return true;
}

static bool decode_stdout(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_STDOUT;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	list_add(instruccion->registros, obtener_registro(partes[2], pcb));
	return true;
}

static bool decode_mutex_lock(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_MUTEX_LOCK;
	list_add(instruccion->registros, strdup(partes[1]));
	return true;
}

static bool decode_mutex_unlock(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_MUTEX_UNLOCK;
	list_add(instruccion->registros, strdup(partes[1]));
	return true;
}

static bool decode_mutex_create(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_MUTEX_CREATE;
	list_add(instruccion->registros, strdup(partes[1]));
	return true;
}

static bool decode_init_proc(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_INIT_PROC;
	list_add(instruccion->registros, strdup(partes[1]));
	int *prioridad = malloc(sizeof(int));
	*prioridad = (int)strtol(partes[2], NULL, 10);
	list_add(instruccion->registros, prioridad);
	return true;
}

static bool decode_mem_alloc(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_MEM_ALLOC;
	int *id_segmento = malloc(sizeof(int));
	*id_segmento = (int)strtol(partes[1], NULL, 10);
	int *tamanio = malloc(sizeof(int));
	*tamanio = (int)strtol(partes[2], NULL, 10);
	list_add(instruccion->registros, id_segmento);
	list_add(instruccion->registros, tamanio);
	return true;
}

static bool decode_mem_free(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_MEM_FREE;
	int *id_segmento = malloc(sizeof(int));
	*id_segmento = (int)strtol(partes[1], NULL, 10);
	list_add(instruccion->registros, id_segmento);
	return true;
}

static bool decode_copy_mem(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_COPY_MEM;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	return true;
}

t_instruccion_decodificada *decode(char *instruccion, t_pcb *pcb){
	static t_decodificador_instruccion decodificadores[] = {
		{"SET", decode_set},
		{"NOOP", decode_noop},
		{"SUM", decode_sum},
		{"SUB", decode_sub},
		{"JNZ", decode_jnz},
		{"MOV_OUT", decode_mov_out},
		{"MOV_IN", decode_mov_in},
		{"EXIT", decode_exit},
		{"SLEEP", decode_sleep},
		{"STDIN", decode_stdin},
		{"STDOUT", decode_stdout},
		{"MUTEX_LOCK", decode_mutex_lock},
		{"MUTEX_UNLOCK", decode_mutex_unlock},
		{"MUTEX_CREATE", decode_mutex_create},
		{"INIT_PROC", decode_init_proc},
		{"MEM_ALLOC", decode_mem_alloc},
		{"MEM_FREE", decode_mem_free},
		{"COPY_MEM", decode_copy_mem}
	};
	t_instruccion_decodificada *instruccion_decodificada = calloc(1, sizeof(*instruccion_decodificada));
	if(instruccion_decodificada == NULL)
		return NULL;
	instruccion_decodificada->registros = list_create();
	if(instruccion_decodificada->registros == NULL){
		free(instruccion_decodificada);
		return NULL;
	}

	if(instruccion == NULL){
		log_error(logger, "Instruccion NULL");
		iniciar_instr_exit(instruccion_decodificada, INSTRUCCION_INVALIDA);
		return instruccion_decodificada;
	}

	char **partes = string_split(instruccion, " ");
	if(partes == NULL || partes[0] == NULL){
		iniciar_instr_exit(instruccion_decodificada, INSTRUCCION_INVALIDA);
		string_array_destroy(partes);
		return instruccion_decodificada;
	}

	for(size_t i = 0; i < sizeof(decodificadores) / sizeof(decodificadores[0]); i++){
		if(string_equals_ignore_case(partes[0], decodificadores[i].nombre)){
			bool result = decodificadores[i].decodificar(partes, pcb, instruccion_decodificada);
			string_array_destroy(partes);
			if(!result){
				iniciar_instr_exit(instruccion_decodificada, INSTRUCCION_INVALIDA);
			}
			return instruccion_decodificada;
		}
	}

	iniciar_instr_exit(instruccion_decodificada, INSTRUCCION_INVALIDA);
	string_array_destroy(partes);
	return instruccion_decodificada;
}
