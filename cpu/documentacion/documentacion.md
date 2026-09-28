# CPU - Funcionamiento actual

## Funcionamiento general
El modulo tiene dos flujos principales:

1. `main.c` lee la configuracion, inicializa el estado global y establece conexiones con SCH y KM. Informa el ID de la CPU, recibe el tamanio maximo de segmento y las direcciones de los memory sticks. Luego crea un hilo separado para ejecutar instrucciones y el hilo principal queda atendiendo mensajes de SCH.
2. El hilo `ejecutar()` espera que el estado sea `EXEC`. Cuando SCH asigna un PID, solicita a KM el PCB y la tabla de segmentos. Luego ejecuta una instruccion por iteracion mediante `ciclo_instruccion()`.

El ciclo solicita la instruccion a KM usando PID y PC (`fetch`), la separa y decodifica (`decode`), y delega su efecto (`execute`). Las operaciones aritmeticas modifican los registros del PCB local. Las operaciones de memoria calculan segmento y offset, validan el rango con `mmu()` y envian una peticion a KM. Las syscalls se notifican a SCH; las transiciones de estado y las interrupciones determinan si la CPU sigue, espera o entrega el contexto.

Los sticks se conectan por separado y cada uno tiene un hilo que espera su desconexion. La CPU tambien puede pedir a KM que actualice la tabla de segmentos despues de operaciones de memoria.


## Maquina de estados

La transicion se decide en el hilo que recibe mensajes de SCH. `EXEC` conserva el estado mientras se marca una interrupcion; el hilo de ejecucion entrega el contexto al llegar a su punto de espera. La tabla refleja las acciones de `procesar_evento()` y sus funciones de transicion.

| Estado inicial | Evento de SCH | Accion | Estado siguiente |
|---|---|---|---|
| `LIBRE` | `SCH_CPU__PID` | Limpia el pedido de desalojo y despierta al hilo de ejecucion. | `EXEC` |
| `LIBRE` | `SCH_CPU__PEDIDO_DESALOJO` | Envia `CPU_SCH__EJECUCION_DETENIDA`. | `LIBRE` |
| `LIBRE` | `SCH_CPU__ERROR_SYSCALL` | Mantiene la CPU libre. | `LIBRE` |
| `EXEC` | `SCH_CPU__ERROR_SYSCALL`, `SCH_CPU__COMPACTACION` o `SCH_CPU__PEDIDO_DESALOJO` | Marca el desalojo pendiente; el hilo de ejecucion guarda el PCB y confirma la detencion al procesar el pedido. | `EXEC` y luego `LIBRE` |
| `WAIT_SYS` | `SCH_CPU__COMPACTACION` o `SCH_CPU__PEDIDO_DESALOJO` | Registra que debe desalojar al terminar la syscall. | `WAIT_SYS_Y_PROX_DESALOJO` |
| `WAIT_SYS` | `SCH_CPU__FIN_SYSCALL` | Limpia el pedido de desalojo y despierta al hilo de ejecucion. | `EXEC` |
| `WAIT_SYS` | `SCH_CPU__SYS_BLOQUEANTE` | Marca el desalojo pendiente, despierta al hilo y libera la CPU. | `LIBRE` |
| `WAIT_SYS` | `SCH_CPU__ERROR_SYSCALL` | Libera la CPU. | `LIBRE` |
| `WAIT_SYS_Y_PROX_DESALOJO` | `SCH_CPU__SYS_BLOQUEANTE` o `SCH_CPU__FIN_SYSCALL` | Libera la CPU, marca el desalojo pendiente y despierta al hilo para guardar el PCB y confirmar. | `LIBRE` |
| `WAIT_SYS_Y_PROX_DESALOJO` | `SCH_CPU__ERROR_SYSCALL` | Libera la CPU. | `LIBRE` |
| `WAIT_MEM_ALLOC` | `SCH_CPU__PEDIDO_DESALOJO` | Difiere el desalojo hasta la respuesta de asignacion. | `WAIT_MEM_ALLOC_Y_PROX_DESALOJO` |
| `WAIT_MEM_ALLOC` | `SCH_CPU__FIN_SYSCALL` | Despierta al hilo de ejecucion para continuar. | `EXEC` |
| `WAIT_MEM_ALLOC` | `SCH_CPU__COMPACTACION` | Libera la CPU y confirma la detencion. | `LIBRE` |
| `WAIT_MEM_ALLOC` | `SCH_CPU__ERROR_SYSCALL` | Libera la CPU. | `LIBRE` |
| `WAIT_MEM_ALLOC_Y_PROX_DESALOJO` | `SCH_CPU__COMPACTACION` | Libera la CPU y confirma la detencion. | `LIBRE` |
| `WAIT_MEM_ALLOC_Y_PROX_DESALOJO` | `SCH_CPU__FIN_SYSCALL` | Marca el desalojo pendiente y despierta al hilo; este guarda el PCB y confirma la detencion. | `WAIT_MEM_ALLOC_Y_PROX_DESALOJO` y luego `LIBRE` |
| `WAIT_MEM_ALLOC_Y_PROX_DESALOJO` | `SCH_CPU__ERROR_SYSCALL` | Libera la CPU. | `LIBRE` |

Los eventos no enumerados no realizan una transicion en el `default` actual. Las confirmaciones de compactacion y desalojo llaman a `enviar_confirmacion()`, que tambien limpia el indicador de solicitud de segmentos.
| 