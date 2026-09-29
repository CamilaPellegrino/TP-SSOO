#include "ciclo_instruccion.h"

typedef void (*t_decodificador)(char **partes, t_pcb *pcb,
								t_instruccion_decodificada *instruccion);

typedef struct {
	char *nombre;
	t_decodificador decodificar;
} t_decodificador_instruccion;

void agregar_numero(int posicion, int valor, t_list *lista_registros){
	int *numero = malloc(sizeof(*numero));
	if(numero == NULL)
		return;
	*numero = valor;
	list_add_in_index(lista_registros, posicion, numero);
}

void agregar_string(int posicion, char *valor, t_list *lista_registros){
	char *cadena = strdup(valor);
	if(cadena == NULL)
		return;
	list_add_in_index(lista_registros, posicion, cadena);
}

static void decode_set(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_SET;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	int *numero = malloc(sizeof(int));
	*numero = (int)strtol(partes[2], NULL, 10);
	list_add(instruccion->registros, numero);
}

static void decode_noop(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)partes; (void)pcb;
	instruccion->tipo = I_NOOP;
}

static void decode_sum(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_SUM;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	list_add(instruccion->registros, obtener_registro(partes[2], pcb));
}

static void decode_sub(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_SUB;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	list_add(instruccion->registros, obtener_registro(partes[2], pcb));
}

static void decode_jnz(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_JNZ;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	int *numero = malloc(sizeof(int));
	*numero = (int)strtol(partes[2], NULL, 10);
	list_add(instruccion->registros, numero);
}

static void decode_mov_out(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_MOV_OUT;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
}

static void decode_mov_in(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_MOV_IN;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
}
  
static void decode_exit(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)partes; (void)pcb;
	iniciar_instr_exit(instruccion, OK);
}

static void decode_sleep(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_SLEEP;
	int *numero = malloc(sizeof(int));
	*numero = (int)strtol(partes[1], NULL, 10);
	list_add(instruccion->registros, numero);
}

static void decode_stdin(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_STDIN;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	list_add(instruccion->registros, obtener_registro(partes[2], pcb));
}

static void decode_stdout(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_STDOUT;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
	list_add(instruccion->registros, obtener_registro(partes[2], pcb));
}

static void decode_mutex_lock(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_MUTEX_LOCK;
	list_add(instruccion->registros, strdup(partes[1]));
}

static void decode_mutex_unlock(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_MUTEX_UNLOCK;
	list_add(instruccion->registros, strdup(partes[1]));
}

static void decode_mutex_create(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_MUTEX_CREATE;
	list_add(instruccion->registros, strdup(partes[1]));
}

static void decode_init_proc(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_INIT_PROC;
	list_add(instruccion->registros, strdup(partes[1]));
	int *prioridad = malloc(sizeof(int));
	*prioridad = (int)strtol(partes[2], NULL, 10);
	list_add(instruccion->registros, prioridad);
}

static void decode_mem_alloc(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_MEM_ALLOC;
	int *id_segmento = malloc(sizeof(int));
	*id_segmento = (int)strtol(partes[1], NULL, 10);
	int *tamanio = malloc(sizeof(int));
	*tamanio = (int)strtol(partes[2], NULL, 10);
	list_add(instruccion->registros, id_segmento);
	list_add(instruccion->registros, tamanio);
}

static void decode_mem_free(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	(void)pcb;
	instruccion->tipo = I_MEM_FREE;
	int *id_segmento = malloc(sizeof(int));
	*id_segmento = (int)strtol(partes[1], NULL, 10);
	list_add(instruccion->registros, id_segmento);
}

static void decode_copy_mem(char **partes, t_pcb *pcb, t_instruccion_decodificada *instruccion){
	instruccion->tipo = I_COPY_MEM;
	list_add(instruccion->registros, obtener_registro(partes[1], pcb));
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
			decodificadores[i].decodificar(partes, pcb, instruccion_decodificada);
			string_array_destroy(partes);
			return instruccion_decodificada;
		}
	}

	iniciar_instr_exit(instruccion_decodificada, INSTRUCCION_INVALIDA);
	string_array_destroy(partes);
	return instruccion_decodificada;
}
