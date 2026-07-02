#include <utils/hello.h>
#include <utils/utils.h>
#include "base_swap.h"

char *puerto_kernel_memory;
char* ip;
int tam_bloque;
int tam_archivo;
int cant_bloques;
char* ruta;
t_log * logger;
FILE* archivo_swap = NULL;
//declaro funciones 
void atender_pedido_escritura(int cliente_fd, t_list* data);
void atender_pedido_lectura(int cliente_fd, t_list* data);
void inicializar_variables_globales(t_config* config);

int main(int argc, char* argv[]) {
    if(argc < 2){ 
        printf("Se esperaban mas parametros. Ejemplo: ./bin/swap swap.config");
        exit(EXIT_FAILURE);
    }
    char *ruta_config = argv[1];
    
    
    t_config* config = iniciar_config(ruta_config);
    inicializar_variables_globales(config);
    log_info(logger, "archivo= %d, bloque= %d", tam_archivo, tam_bloque);
    cant_bloques = tam_archivo/tam_bloque;
    archivo_swap = fopen(ruta, "w+b");
    if (archivo_swap == NULL) {
        printf("Error al abrir el archivo.\n");
        return 1;
    }
    int fd = fileno(archivo_swap);
    ftruncate(fd, cant_bloques * tam_bloque);
    //crear conexion con kernel memory
    int conexion_kernel_memory = crear_conexion(ip, puerto_kernel_memory); 
    handshake_cliente(conexion_kernel_memory, logger);
    exit_si_error_conexion(conexion_kernel_memory, logger, "kernel_memory");
    t_paquete* conexion = crear_paquete(SWAP_KM__CONEXION);
    agregar_a_paquete(conexion, &tam_bloque, sizeof(tam_bloque));
    agregar_a_paquete(conexion, &cant_bloques, sizeof(cant_bloques));
    enviar_paquete(conexion, conexion_kernel_memory);

    while(1){
        op_code cod_op = recibir_operacion(conexion_kernel_memory);
        if(cod_op == -1){
            log_error(logger, "Desconexion de Kernel Memory");
            exit(EXIT_FAILURE);
        }
        switch(cod_op){
            case KM_SWAP__ESCRITURA:{
                t_list* lista_swap = recibir_paquete(conexion_kernel_memory);
                atender_pedido_escritura(conexion_kernel_memory, lista_swap);
                break;
            }case KM_SWAP__LECTURA:{
                t_list* lista_swap = recibir_paquete(conexion_kernel_memory);
                atender_pedido_lectura(conexion_kernel_memory, lista_swap);
                break;
            }default:{
                log_warning(logger, "Operacion desconocida, cod_op: %d", cod_op);
            }
        }
    }
    fclose(archivo_swap);
    return 0;
}


void atender_pedido_escritura(int cliente_fd, t_list* data){
    int bloque = *(int*)list_get(data, 0);
    void* contenido = list_get(data, 1);
    int tamanio_cont = *(int*)list_get(data, 2);
    if(bloque >= cant_bloques){
        log_error(logger, "Escribiendo fuera de memoria de SWAP");
        return;
    }
    imprimir_bytes(contenido, tamanio_cont);
    int desplazamiento = bloque * tam_bloque;
    fseek(archivo_swap, desplazamiento, SEEK_SET);
    fwrite(contenido, tamanio_cont, 1, archivo_swap);

    log_info(logger, "## Escritura del bloque: <%d>", bloque);
    enviar_operacion(cliente_fd, SWAP_KM__OK);
}

void atender_pedido_lectura(int cliente_fd, t_list* data)
{
    int bloque = *(int*)list_get(data, 0);
    int tamanio_cont = *(int*)list_get(data, 1);

    if (bloque >= cant_bloques) {
        log_error(logger, "Leyendo fuera de memoria de SWAP");
        return;
    }
    void* leido = malloc(tamanio_cont);
    int desplazamiento = bloque * tam_bloque;
    fseek(archivo_swap, desplazamiento, SEEK_SET);
    fread(leido, tamanio_cont, 1, archivo_swap);

    log_info(logger, "## Lectura del bloque: <%d>", bloque);

    t_paquete* paquete_respuesta = crear_paquete(SWAP_KM__OK);
    agregar_a_paquete(paquete_respuesta, leido, tamanio_cont);
    enviar_paquete_y_liberarlo(paquete_respuesta, cliente_fd);

    free(leido);
}

void inicializar_variables_globales(t_config* config){
    puerto_kernel_memory = config_get_string_value (config, "PUERTO_KERNEL_MEMORY");
    ip = config_get_string_value (config, "IP");
    tam_archivo = config_get_int_value(config, "SWAP_FILE_SIZE");
    tam_bloque = config_get_int_value(config, "BLOCK_SIZE");
    ruta = config_get_string_value (config, "SWAP_FILE_PATH");
    t_log_level log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL")); 
    logger = iniciar_logger("swap.log", "ProcesoSWAP", log_level);
}