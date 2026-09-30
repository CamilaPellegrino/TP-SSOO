# Trabajo Práctico - Sistemas Operativos

Repositorio correspondiente al Trabajo Práctico de Sistemas Operativos (SSOO) de la Universidad Tecnológica Nacional – Facultad Regional Buenos Aires (UTN FRBA).

## 📋 Consigna

La consigna completa del trabajo se encuentra disponible en el siguiente documento:

🔗 https://docs.google.com/document/d/1T0CglFoVRDHWXKHvNNXl62RryVkhWG9rmGyb7NDilCc/edit?tab=t.0

## ✅ Códigos de prueba

Para facilitar las pruebas y validación de la implementación, se puede utilizar el siguiente repositorio con códigos de prueba:

🔗 https://github.com/sisoputnfrba/plug-n-pray-pruebas/

## Dependencias

Para poder compilar y ejecutar el proyecto, es necesario tener instalada la
biblioteca [so-commons-library] de la cátedra:

```bash
git clone https://github.com/sisoputnfrba/so-commons-library
cd so-commons-library
make debug
make install
```
> Nota: dependiendo de la configuración de permisos de la máquina, puede ser necesario utilizar sudo durante la instalación.

## Compilación y ejecución

Cada módulo del proyecto se compila de forma independiente a través de un
archivo `makefile`. Para compilar un módulo, es necesario ejecutar el comando
`make` desde la carpeta correspondiente.

El ejecutable resultante de la compilación se guardará en la carpeta `bin` del
módulo. Ejemplo:

```sh
cd kernel
make
./bin/kernel
```

## 🧩 Módulos

El proyecto está compuesto por los siguientes módulos:

* **`kernel scheduler`** — Módulo principal encargado de gestionar la planificación de los Procesos.
* **`kernel memory`** — Módulo encargado de gestionar la asignación de memoria a lo largo de los diferentes Memory Sticks y del SWAP.
* **`cpu`** — Módulo encargado de la ejecución de instrucciones y simulación los pasos del ciclo de instrucción de una CPU real simplificada.
* **`memory_stick`** — Módulo encargado representar un espacio de direcciones de memoria, simulando los diferentes chips de memoria de una computadora.
* **`swap`** — Módulo encargado de almacenar la información de los procesos que sean suspendidos por el kernel.
* **`io`** — Módulo encargado de simular las operaciones de entrada y salida.

Cada módulo posee su propio código fuente, configuración, Makefile y ejecutable.

## Instrucciones soportadas

Los archivos de proceso contienen instrucciones con los siguientes nombres y
parámetros. Los nombres de las instrucciones y los registros no distinguen
mayúsculas de minúsculas.

Los registros disponibles son `AX`, `BX`, `CX` y `DX` (8 bits), y `EAX`,
`EBX`, `ECX`, `EDX`, `SI`, `DI` y `PC` (32 bits). En las operaciones que
reciben un registro como destino, el resultado se ajusta al tamaño de ese
registro.

| Instrucción | Parámetros | Qué hace |
| --- | --- | --- |
| `NOOP` | Ninguno | No realiza ninguna operación. |
| `SET registro valor` | `registro`: registro destino. `valor`: número entero decimal. | Guarda el valor en el registro. |
| `SUM destino origen` | `destino`, `origen`: registros. | Suma ambos valores y guarda el resultado en `destino`. |
| `SUB destino origen` | `destino`, `origen`: registros. | Resta `origen` a `destino` y guarda el resultado en `destino`. |
| `JNZ registro pc` | `registro`: registro a comprobar. `pc`: índice de instrucción de destino. | Si el registro no es cero, continúa desde ese PC (salto absoluto). |
| `MOV_IN registro` | `registro`: registro donde guardar lo leído. | Lee desde la dirección lógica indicada por `SI`; la cantidad de bytes leídos corresponde al tamaño del registro. |
| `MOV_OUT registro` | `registro`: registro cuyo contenido se escribe. | Escribe en la dirección lógica indicada por `DI`; la cantidad de bytes escritos corresponde al tamaño del registro. |
| `COPY_MEM registro_tamanio` | `registro_tamanio`: registro que contiene la cantidad de bytes. | Copia memoria desde la dirección lógica de `SI` hacia la de `DI`. |
| `SLEEP milisegundos` | `milisegundos`: duración de la espera. | Bloquea el proceso durante el tiempo indicado. |
| `STDIN registro_direccion registro_tamanio` | `registro_direccion`: registro con la dirección lógica de destino. `registro_tamanio`: registro con la cantidad de bytes a leer. | Solicita datos al dispositivo de entrada y los escribe en memoria. |
| `STDOUT registro_direccion registro_tamanio` | `registro_direccion`: registro con la dirección lógica de origen. `registro_tamanio`: registro con la cantidad de bytes a mostrar. | Lee memoria y envía los datos al dispositivo de salida. |
| `MUTEX_CREATE nombre` | `nombre`: identificador del mutex. | Crea un mutex con ese nombre. |
| `MUTEX_LOCK nombre` | `nombre`: identificador del mutex. | Solicita bloquear el mutex; el proceso espera si ya está ocupado. |
| `MUTEX_UNLOCK nombre` | `nombre`: identificador del mutex. | Libera el mutex. |
| `INIT_PROC ruta prioridad` | `ruta`: archivo del nuevo proceso. `prioridad`: prioridad asignada. | Solicita la creación de un proceso a partir del archivo indicado. |
| `MEM_ALLOC id_segmento tamanio` | `id_segmento`: identificador numérico del segmento. `tamanio`: cantidad de bytes solicitada. | Solicita reservar un segmento de memoria. |
| `MEM_FREE id_segmento` | `id_segmento`: identificador numérico del segmento. | Solicita liberar el segmento. |
| `EXIT` | Ninguno | Finaliza el proceso con estado de salida normal. |

## Importar desde Visual Studio Code

Para importar el workspace, debemos abrir el archivo `tp.code-workspace` desde
la interfaz o ejecutando el siguiente comando desde la carpeta raíz del
repositorio:

```bash
code tp.code-workspace
```

## Checkpoint

Para cada checkpoint de control obligatorio, se debe crear un tag en el
repositorio con el siguiente formato:

```
checkpoint-{número}
```

Donde `{número}` es el número del checkpoint, ejemplo: `checkpoint-1`.

Para crear un tag y subirlo al repositorio, podemos utilizar los siguientes
comandos:

```bash
git tag -a checkpoint-{número} -m "Checkpoint {número}"
git push origin checkpoint-{número}
```

> [!WARNING]
> Asegúrense de que el código compila y cumple con los requisitos del checkpoint
> antes de subir el tag.

## Entrega

Para desplegar el proyecto en una máquina Ubuntu Server, podemos utilizar el
script [so-deploy] de la cátedra:

```bash
git clone https://github.com/sisoputnfrba/so-deploy.git
cd so-deploy
./deploy.sh -r=release -p=utils -p=kernel_scheduler -p=kernel_memory -p=cpu -p=memory_stick -p=swap -p=io "tp-2026-1c-RedBugBusters"
```

El mismo se encargará de instalar las Commons, clonar el repositorio del grupo
y compilar el proyecto en la máquina remota.

> [!NOTE]
> Ante cualquier duda, pueden consultar la documentación en el repositorio de
> [so-deploy], o utilizar el comando `./deploy.sh --help`.

## Guías útiles

- [Cómo interpretar errores de compilación](https://docs.utnso.com.ar/primeros-pasos/primer-proyecto-c#errores-de-compilacion)
- [Cómo utilizar el debugger](https://docs.utnso.com.ar/guias/herramientas/debugger)
- [Cómo configuramos Visual Studio Code](https://docs.utnso.com.ar/guias/herramientas/code)
- **[Guía de despliegue de TP](https://docs.utnso.com.ar/guías/herramientas/deploy)**

[so-commons-library]: https://github.com/sisoputnfrba/so-commons-library
[so-deploy]: https://github.com/sisoputnfrba/so-deploy
