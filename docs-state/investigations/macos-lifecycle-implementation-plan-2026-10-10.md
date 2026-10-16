# Plan de implementación de estabilidad del driver macOS

Fecha: 10 de octubre de 2026. Estado: **diseñado, sin implementar**.

Continuación del 11 de octubre: usar el [plan de corrección tras los resultados físicos](macos-lifecycle-remediation-plan-2026-10-11.md) y su [estado nuevo](macos-lifecycle-remediation-status-2026-10-11.json) para el orden de ejecución. Este documento conserva el diseño y las tarjetas originales; la implementación parcial y los fallos de la primera campaña figuran en el estado histórico.

Este plan corrige los problemas de la [auditoría de estabilidad](macos-lifecycle-stability-audit-2026-10-10.md). Está escrito para ejecutarse en orden, una tarea por vez, incluso con un modelo que disponga de poco contexto. Cada tarea indica dónde cambiar código, qué comportamiento obtener y qué pruebas deben pasar.

El resultado buscado es que abrir y cerrar Traktor, cambiar frecuencia, desconectar USB e instalar o retirar el driver tengan un resultado definido: éxito o un error recuperable dentro de un plazo. El driver debe conservar memoria y recursos mientras haya operaciones que puedan usarlos. La necesidad de reiniciar macOS rechaza el candidato.

## Cómo usar el plan

1. Leer esta introducción, las decisiones de diseño y la tarjeta de la siguiente tarea pendiente.
2. Consultar únicamente los casos de esa tarjeta en la [especificación de pruebas](macos-lifecycle-test-spec-2026-10-10.md).
3. Hacer el cambio de esa tarjeta. No adelantar tareas dependientes.
4. Ejecutar sus pruebas y las que ya habían pasado en tareas anteriores.
5. Guardar comandos, resultados, hash de fuentes y limitaciones. Actualizar el [estado de ejecución](macos-lifecycle-implementation-status.json).
6. Si una prueba falla, corregir la tarea actual. No declarar completada una tarea por haber escrito el código.

El archivo de estado es una ayuda de continuidad, no evidencia por sí mismo. Una tarea `done` necesita rutas a resultados verificables. Un test no ejecutado es `NOT_RUN`, nunca `PASS`. Las tareas L00–L17 son offline. L18 prepara la herramienta física pero se prueba offline. L19 requiere el equipo y la ventana de uso físico.

**En este documento, todos los nombres de archivos, funciones, opciones y comandos marcados como NUEVO son especificaciones que hay que implementar.** No existen todavía. Los comandos nuevos solo se ejecutan después de completar la tarea que los crea. Las referencias a archivos existentes se buscan por nombre de función; los números de línea de la auditoría son orientativos y cambiarán.

## Punto de partida y alcance

- Base auditada: commit `46a80ff`; HAL y MIDI equivalentes al audio de `927d8af`.
- Instalado de referencia: 0.5.1 build 151, SHA-256 `4af4b1207f81846208fd2fcc0b8f5a600c2e11346d523ee5fd8e4f55700b9f66`.
- Tres defectos reproducidos: doble cierre de descriptor, bloqueo IPC por contrapresión y limpieza incompleta de apertura.
- Resultado histórico CTest: 72/88 pasan, 16 gates fallan; varias pruebas leen otro checkout. No usar ese resultado como aceptación del nuevo driver.
- Preservar el formato de audio, ruteo, ganancias, tamaño de transferencias, latencia del perfil publicado, nombres/UID del dispositivo y protocolo IPC v1. Esta entrega no migra a DriverKit ni modifica Windows/Linux.
- Las pruebas físicas de 44,1 y 48 kHz son obligatorias. No retirar las frecuencias adicionales anunciadas sin una decisión separada; probar su validación y manejo de errores offline.
- Conservar USB y MIDI en `OpenIdle` sigue siendo la política. No activar el keepalive de audio del daemon MIDI para disimular un fallo de reapertura.

## Decisiones de diseño que no debe improvisar el ejecutor

### D1 Estados del motor

La fuente de verdad es un estado explícito. `_running` y `_streaming`, si temporalmente se conservan, son valores derivados; no autorizan saltarse cleanup.

| Estado | Recursos y comportamiento |
| --- | --- |
| `Closed` | No posee dispositivo, interfaz, pipes, peticiones ni clientes IPC del motor |
| `Opening` | Adquisición parcial; todavía no disponible para audio |
| `OpenIdle` | USB/EP1/control/MIDI saludables, sin transferencias ISO de audio |
| `Starting` | Negociación y preparación; audio aún no publicado |
| `Streaming` | Sesión publicada y envíos aceptados |
| `Stopping` | Nuevos envíos prohibidos; cancelando y drenando la sesión |
| `Closing` | Cierre de control/EP1/IPC y liberación tras drenaje |
| `Recovering` | Reapertura limitada de una conexión inválida |
| `Faulted` | Audio despublicado; conserva solo recursos todavía en uso; error y motivo consultables |

Transiciones principales: `Closed → Opening → OpenIdle → Starting → Streaming → Stopping → OpenIdle`. Cierre completo: `OpenIdle → Closing → Closed`. Cualquier fallo relevante puede conducir a `Faulted`; solo se permite `Recovering` cuando las operaciones viejas han terminado y no quedan recursos que impidan una apertura exclusiva.

`devicePresent` indica presencia del servicio físico; `transportReady` indica motor utilizable; `clientDemand` indica clientes que quieren audio; `shutdownRequested` prohíbe reabrir al descargar. No confundir estos cuatro datos. Al perder USB durante audio, conservar la demanda de clientes mientras sigan registrados, entregar silencio y permitir una recuperación limitada. Al retirar el último cliente, la recuperación solo debe intentar `OpenIdle`.

### D2 Colas y propiedad de recursos

| Dueño | Qué hace | Qué no puede hacer |
| --- | --- | --- |
| Cola serial de lifecycle NUEVA | Decide estados, operaciones pendientes, plazos y recuperación | Esperar sincrónicamente a una cola que necesita responderle |
| Cola USB actual `_queue` | Submit/completion ISO y estado de la sesión de audio | Esperar IPC, al host HAL o al coordinador |
| Cola de operaciones de recursos NUEVA | Llamadas SDK síncronas de apertura/configuración/destrucción | Actualizar el estado publicado sin validar el ID de operación |
| Cola serial IPC NUEVA | Registro de conexiones, parser, lectura/escritura no bloqueante y cancelación | Esperar respuestas USB ni ejecutar un bucle de escritura sin presupuesto |
| Camino de audio HAL | Copiar audio usando una referencia estable y corta | Abrir/cerrar USB, esperar lifecycle, hacer IPC, escribir logs o destruir el último dueño del motor |

Las completions USB y los mensajes de interés informan al coordinador por mensajes asíncronos. La documentación de IOUSBHost indica que las notificaciones de interés llegan por una cola interna separada: no asumir que ya están serializadas con el audio.

Mantener un propietario fuerte del motor y de las sesiones hasta su retirada completa. El mutex de publicación solo protege intercambio de referencia y adquisición de lector; no se mantiene durante USB, `dispatch_sync`, IPC o espera de un semáforo. En el callback de audio, usar adquisición sin espera (`trylock` y salida de silencio/descartar salida si falla), contar esa contingencia y exigir cero contenciones durante streaming estable en la validación. Liberar el último propietario únicamente desde control, después de que los lectores activos lleguen a cero.

No introducir un framework genérico de concurrencia. Crear solo los tipos concretos de este driver descritos abajo. No cambiar a `.mm` todo el transporte ni sustituir el empaquetado PCM.

### D3 Generaciones y operaciones pendientes

- `engineGeneration`: aumenta al invalidar/reemplazar una conexión USB.
- `streamGeneration`: aumenta al empezar una sesión y al invalidarla para parar.
- `operationID`: identifica cada operación asíncrona de abrir, arrancar, parar o cerrar.
- `connectionID`: monotónico, identifica un cliente IPC aunque el sistema reutilice su número de fd.

Cada petición USB conserva el contexto de su sesión, token único, dirección, buffer, transacciones y estado terminal. Contar `accepted` antes de permitir una completion; si enqueue falla, retirar la reserva exactamente una vez. Una completion vieja libera y contabiliza su propia petición, pero no decodifica hacia buffers nuevos, no cambia contadores nuevos y no reencola.

Una sesión se puede retirar cuando `pendingCapture=0`, `pendingPlayback=0`, `pendingControl=0` para los recursos que se vayan a cerrar, `callbacksExecuting=0`, `audioReaders=0` y no hay tareas diferidas capaces de usarla. Un plazo vencido no pone artificialmente estos contadores a cero.

Cuando el último token termina, encolar una confirmación asíncrona en la cola serial de completions y desde ella avisar al coordinador: así la retirada no compite con el final del último handler. Auditar ciclos de retención entre motor, pool, bloque y sesión. Los bloques retienen su contexto necesario, pero no deben mantener eternamente al motor a través del pool; retirar las referencias de completion al terminar su petición. Una petición sin completion conserva su contexto intencionalmente y consume el límite D4, sin abrir motores adicionales.

### D4 Cancelación y límites

Solicitar abort con `abortWithOption:IOUSBHostAbortOptionAsynchronous error:` y comprobar el resultado; luego esperar completions mediante el coordinador. El SDK distingue abort síncrono y asíncrono. La API asíncrona evita esperar deliberadamente a todas las peticiones dentro del abort, pero no permite prometer que cualquier llamada al sistema jamás se atascará. [Referencia Apple](https://developer.apple.com/documentation/iousbhost/iousbhostpipe/abortwithoption:error:).

Los plazos se aplican a la respuesta al llamador y al estado del driver. Si una llamada SDK síncrona excede el plazo, el llamador recibe error y el contexto queda retenido; no se mata un hilo ni se lanza otro intento sobre el mismo dispositivo mientras la llamada anterior siga pendiente. Máximo un contexto de recuperación no drenado: alcanzar ese límite deja el motor `Faulted`, sin acumular motores o hilos.

Los valores iniciales de aceptación están en la especificación de pruebas. Son objetivos de ingeniería propuestos, no medidas de la 0.5.1. No subirlos para convertir un fallo en éxito sin documentar causa, dato físico y revisión.

### D5 IPC con propietario único

Crear `OpenA8DJIPCConnection` y `OpenA8DJIPCServer` en `src/hal`, con interfaz privada. Cada conexión tiene fd, ID, estado, buffer de entrada, cola de frames de salida, offset de envío y sus dispatch sources.

- El servidor admite como máximo **16 conexiones**, igual que hoy. Rechazar la 17 antes de enviar o lanzar un manejador.
- Sockets no bloqueantes y `SO_NOSIGPIPE`; manejar `EINTR`, `EAGAIN`, EOF y escrituras parciales.
- Un frame completo se serializa a bytes antes de entrar en la cola. Un único escritor por conexión preserva cabecera y payload sin intercalarlos.
- Límite de **128 KiB de salida y 128 frames por cliente**, lo primero que se alcance. Al superar el límite, desconectar ese cliente y registrar el motivo; no bloquear USB ni descartar partes de un frame.
- No añadir un límite de 4096 al payload de salida existente: el volcado de ledger puede ser mayor. Conservar los límites reales del protocolo. La cola debe poder albergar al menos un frame máximo legal, incluida su cabecera; los 128 KiB permiten hacerlo.
- El límite de entrada continúa siendo 4096 bytes por frame. Un frame parcial tiene plazo de 2 s desde su primer byte. Una conexión sana y silenciosa puede durar indefinidamente.
- Máximo 16 frames o 64 KiB procesados por handler antes de ceder la cola. Así un cliente no impide cancelar a los demás.
- Cancelar todos los sources que usan el fd; cerrar exactamente una vez después de sus cancel handlers. No cerrar el número mientras un handler pueda usarlo. Para el source de escritura, crear al necesitarlo y cancelar cuando drene; llevar cuenta explícita de todos los sources aún vivos de esa conexión. No usar suspensiones/resumes difíciles de balancear.

Apple exige el cancel handler para cerrar con seguridad un descriptor asociado a dispatch sources. Si hay varios sources, el dueño debe esperar a todos. [Referencia Apple](https://developer.apple.com/documentation/dispatch/dispatch_source_set_cancel_handler_f).

El servidor retira el pathname Unix solo si sigue siendo el socket que creó: comparar identidad capturada al bind; un cleanup viejo no debe borrar el socket de una nueva generación. El path real sigue siendo `/tmp/opena8dj-control.sock`; el constructor permite un path temporal para tests.

### D6 Apertura y EP1

La apertura adquiere recursos localmente y hace commit solo tras completar las etapas necesarias. Un único rollback recorre los recursos realmente adquiridos en orden inverso, aunque nunca se haya llegado a `_running=true`.

Convertir EP1 a lectura asíncrona, una petición pendiente, con continuación al completar. Eliminar el bucle que espera `DISPATCH_TIME_FOREVER`. Las órdenes de control se serializan por una cola con máximo 32 órdenes pendientes; cada una conserva ID, generation y plazo. Una respuesta tardía no puede completar una orden nueva del mismo tipo. Si el protocolo hardware solo distingue el opcode y no permite correlacionar respuestas tardías, **un timeout invalida la conexión y exige drenaje/reapertura antes de reenviar ese opcode**; no inventar un ID en el cable.

El servidor IPC entrega órdenes al control por mensaje asíncrono y recibe la respuesta por otro mensaje. El hilo MIDI del sistema solo encola datos; no escribe directamente a un socket que pueda bloquear.

### D7 Compatibilidad HAL

Mantener registro por `inClientID` con metadatos y `startDepth`, para admitir llamadas Start/Stop balanceadas del mismo cliente. El transporte arranca en la transición total 0→1 y para en 1→0. No incrementar el total si el inicio falla. Stop extra no resta por debajo de cero. Remove retira toda la demanda del cliente; Stop posterior se vuelve inofensivo. Máximo 64 clientes registrados inicialmente; rechazo explícito si se llena.

Los cambios de frecuencia son transacciones: validar, preparar transporte, confirmar y solo entonces publicar frecuencia y reloj. Si falla, conservar el valor anterior si sigue siendo válido; si la conexión queda inválida, publicar no disponible y un error. Nunca afirmar éxito con USB fallido. Una configuración pendiente usa ID propio; Abort solo cancela esa solicitud.

Notificar al host únicamente cambios efectivos de dispositivo vivo, estado de ejecución, frecuencia, formato, buffer y reloj aplicables. Hacerlo fuera de locks y coalescer direcciones repetidas. Los cambios solicitados directamente por el host deben respetar el contrato del SDK para evitar notificar dos veces. El test de reentrada decide este contrato; no quitar simplemente los `return` de las funciones actuales.

### D8 Descarga y recuperación externa

Retener un buffer por timeout no vuelve segura la descarga de su código. Antes de permitir una descarga cooperativa deben haber terminado callbacks, sources, timers, workers y operaciones SDK; el estado de salida es `Closed` y el módulo no tiene trabajo pendiente.

Sustituir los pthread detached por tareas/timers propiedad del coordinador. Preparar una API privada `PrepareForUnload(deadline)` para el host de pruebas y para la retirada gestionada. **Core Audio no garantiza que llame a esa API privada.** El camino real debe comprobarse en L14: el destructor usa el mismo shutdown; si quedan callbacks cuyo fin no puede probarse, no declarar segura una descarga en el mismo proceso. El caso irrecuperable se resuelve terminando desde fuera el proceso aislado que aloja este HAL, después de recoger evidencia; nunca con `exit`, `abort` o `kill` desde el plug-in.

Si el proceso que aloja OpenA8DJ no puede identificarse inequívocamente o aloja también otros drivers, el helper no lo mata por coincidencia de nombre. Devuelve `BLOCKED` con los candidatos. Un reinicio controlado de Core Audio solo pertenece a instalación/recuperación explícita, no al StopIO normal. No reiniciar servicios de medios en cascada.

### D9 Instalación como transacción

El helper de instalación y los hooks PKG comparten el protocolo de estado: `preflight → backup → staged → oldHostStopped → replaced → validated → committed`. Ante error: `rollback → rollbackValidated`; si tampoco se recupera, `FAILED_RECOVERY`, conservando backups y evidencia.

El backup es de **todo lo que se sustituya**: HAL, herramientas, LaunchAgent, aplicación y documentación, más ausencia/presencia original. Staging y backup se mantienen fuera del directorio HAL activo. No renombrar cinco componentes afirmando que sea una única operación atómica: cada paso se registra y la transacción sabe compensarlo.

El lock de scripts es compartido también con Control Center y helpers existentes. La implementación debe verificar dueño e herencia; `OPENA8DJ_HARDWARE_LOCK_HELD=1` por sí solo no prueba pertenencia. Mantener interoperabilidad con lectores existentes del archivo `owner` y agregar un token de herencia que pueda comprobarse. Nunca retirar el lock de un PID vivo ajeno.

Los hooks PKG ejecutan procesos separados. No conservar un lock ligado al PID efímero de `preinstall` y asumir que sigue vigente. En L16 implementar un lease cuyo dueño verificable sea la operación Installer activa, identificado por PID y hora de inicio, con ID de transacción persistente. Si esa pertenencia no se puede comprobar en el entorno real, rechazar el camino PKG antes de quitar archivos; no eliminar la coordinación para hacerlo pasar.

Registrar también el recibo de Installer con `pkgutil --pkg-info` antes y después. Restaurar archivos no garantiza restaurar el recibo. Si quedan desalineados, el estado es `ROLLBACK_FILES_RESTORED_METADATA_PENDING`, sin declarar recuperación completa. No editar `/var/db/receipts`, no ejecutar `pkgutil --forget` para ocultarlo y no lanzar otro Installer recursivo desde un hook. La reparación posterior usa la reinstalación gestionada del paquete baseline firmado y vuelve a comprobar archivos y recibo.

## Secuencia de tareas

Todas las tareas dependen de la anterior. Eso evita que un modelo pequeño tenga que elegir entre ramas de trabajo. Puede dividir una tarjeta en commits locales, manteniendo el mismo objetivo y el estado verificable. No crear un nuevo task de Codex ni delegar salvo petición del usuario.

### L00 Fijar baseline y carpeta de trabajo

**Archivos:** auditoría, estado JSON, `docs-state/current-release-state.md`, manifiestos locales.

1. Ejecutar `git status --short`, `git rev-parse HEAD`, `sw_vers` y SHA-256 del binario instalado. Registrar, no modificar.
2. Preservar los tres archivos de auditoría actualmente sin seguimiento. No usar `reset --hard`, `clean` ni `make clean` sobre evidencia o rollback.
3. Crear una rama `codex/macos-lifecycle-stability` si aún no existe, o continuar la rama de implementación ya elegida. Si se usa otro checkout, trasladar también los archivos de auditoría necesarios.
4. Crear `local-analysis/lifecycle/<run-id>/baseline.json` con commit, diff hash, SDK, compilador, OS y hash instalado. Usar un run-id único; no sobrescribir resultados anteriores.
5. Antes de las correcciones, ejecutar el reproductor actual con timeout: `python3 scripts/audit-macos-lifecycle-offline.py`. Guardar su salida como demostración histórica, no como test verde.

**Aceptación:** baseline reproducible, tres defectos registrados o diferencia explicada, binario instalado intacto. Después de esta tarea ya no es obligatorio ejecutar el reproductor de defectos contra fuentes corregidas.

### L01 Corregir la raíz usada por los tests

**Archivos:** `CMakeLists.txt`, NUEVO `tools/repo_paths.hpp`, todos los consumidores encontrados con `rg -n '/Users/fer/dev/audio8djcpp|root.filename\(\)' tools scripts`.

1. Crear una única función de raíz basada en `OPENA8DJ_SOURCE_ROOT`, definido por CMake con `CMAKE_SOURCE_DIR` canonicalizado. Para herramientas Python/shell conservar la raíz derivada del propio script.
2. Eliminar comprobaciones que exijan el basename `audio8djcpp` y fallbacks absolutos personales. Error explícito si la raíz no contiene los archivos requeridos.
3. Registrar raíz, commit, diff hash y hashes de entradas en cada informe nuevo. Un hash de commit sin estado dirty es insuficiente.
4. Reemplazar los helpers duplicados. No modificar rutas en documentos históricos para fingir que pertenecían a este árbol.
5. Añadir tests `paths_current_checkout` y `paths_decoy_checkout`: compilar/copiar fixtures en una carpeta con otro nombre y colocar un árbol señuelo diferente; la prueba debe usar siempre el configurado.

**Pruebas:** T01–T03. **Aceptación:** ningún consumidor ejecutable mantiene la ruta fija y todos los informes señalan el checkout actual. Se acepta que ahora aparezcan fallos reales antes ocultos; no se suprimen.

### L02 Separar pruebas autocontenidas y evidencia local

**Archivos:** `CMakeLists.txt`, `scripts/run-cpp-offline-gates`, gates de evidencia afectados, NUEVO `tools/test_inventory.json`.

1. Inventariar los 88 tests: tipo, entrada necesaria, si escribe evidencia y si puede tocar hardware. Asignar labels `offline`, `source_contract`, `local_evidence`; los lifecycle nuevos usan además `macos_lifecycle`.
2. Los tests `offline` generan sus fixtures o reciben fixtures versionadas. Un test cuyo propósito sea validar un gate debe probar tanto un expediente válido como uno inválido, no depender de archivos de junio.
3. Los consumidores que verdaderamente requieren medidas locales se registran bajo la opción existente `OPENA8DJCPP_ENABLE_LOCAL_EVIDENCE_TESTS`. No convertir sus fallos en éxitos ni borrarlos.
4. En el runner, generar evidencias derivadas antes de consumirlas, en un directorio por run. Pasar explícitamente el directorio, sin recurrir a “el último JSON” de otra sesión.
5. Ausencia de evidencia física: `NOT_RUN/BLOCKED` y promoción falsa. Con opción local habilitada y evidencia declarada obligatoria pero ausente: fallo.

**Pruebas:** T04–T05. **Aceptación:** suite offline autocontenida; inventario explica el destino de cada uno de los 16 fallos originales. Prohibido resolverlos añadiendo una lista de exclusión sin clasificación.

### L03 Crear el harness de producción sin hardware

**Archivos:** `CMakeLists.txt`, `tools/audit_macos_lifecycle.m`, NUEVOS `tests/macos/`, `src/hal/OpenA8DJUSBPlatform.h/.m`.

1. En CMake, dentro de `if(APPLE)`, habilitar C y Objective-C. Crear ejecutables de tests que compilen el código HAL/USB real una sola vez; no cargar el bundle instalado.
2. Introducir una interfaz privada de plataforma que envuelva búsqueda, adquisición/configuración de device/interface, copia de pipes, submit, abort y destroy. El adaptador real llama a IOUSBHost; el falso proporciona objetos y completions controlables. Inyectarla por constructor del motor, con valor real por defecto en producción.
3. Evitar rehacer toda la clase: conservar métodos de audio y formatos; extraer únicamente los accesos necesarios para inyectar fallos en cada etapa.
4. Añadir un reloj falso y un scheduler de eventos controlado para plazos/reintentos. El reloj real es monotónico. Los tests de carrera usan barreras/semaphores, no `sleep` como sincronización principal.
5. Crear tres tests de regresión con expectativas de salud: recurso liberado, descriptor nuevo intacto y cierre IPC que termina sin ayuda del lector. Deben fallar antes de sus correcciones.
6. Para sockets usar `socketpair` o un path dentro de un directorio temporal corto. El falso falla inmediatamente si alguien intenta el path instalado, USB real, CoreAudio start o un comando privilegiado.
7. Actualizar el reproductor histórico para explicar que su salida cero demostraba defectos y enlazar la nueva suite; no mantenerlo como gate de salud con semántica inversa.

**Pruebas:** T06 y regresiones T10, T15, T30 inicialmente rojas. **Aceptación:** sus fallos son los esperados y no hay efecto sobre el driver instalado. El resto del harness funciona sin USB conectado.

### L04 Añadir eventos y resultados de lifecycle

**Archivos:** NUEVOS `src/hal/OpenA8DJLifecycle.h/.m`; tipos de resultado en `OpenA8DJUSB.h`; tests macOS.

1. Definir enum de estados D1 y resultados `OK`, `NoDevice`, `Busy`, `Timeout`, `Aborted`, `TransportError`, `ShuttingDown`. Mapear a OSStatus existentes en una única función, sin devolver éxito por error.
2. Crear ring fijo de 256 eventos: tiempo monotónico, estado anterior/nuevo, motivo, generaciones, operationID, contadores pendientes y duración. No guardar audio ni payloads MIDI.
3. Escribir eventos en control y emitir resúmenes con `os_log` fuera del callback de audio. Desde audio, solo contadores atómicos. Sin un `fopen` por paquete.
4. Añadir snapshot privado de diagnósticos. Si se expone por IPC, usar un nuevo tipo opcional con size/version; no cambiar las estructuras v1 que consumen herramientas actuales.
5. Implementar pruebas de capacidad, orden, timeout y lectura concurrente del snapshot. No usar estos eventos como sustituto del estado real.

**Pruebas:** T07–T09. **Aceptación:** memoria acotada, errores distinguibles, logs desactivables sin alterar comportamiento.

### L05 Dar propiedad única a conexiones IPC

**Archivos:** NUEVOS `OpenA8DJIPCConnection.h/.m`, `OpenA8DJIPCServer.h/.m`; `OpenA8DJUSB.m`; fuentes del Makefile y targets CMake.

1. Mover el registro de clientes del array de fds a objetos con `connectionID`, fd y `Open/Closing/Closed`. En esta tarea puede mantenerse temporalmente la lectura existente, pero el worker posee su conexión hasta salir.
2. `add` devuelve una conexión aceptada o rechazo. El llamador nunca envía ni crea worker tras rechazo.
3. `remove` recibe la conexión, no un entero. Cancelar varias veces es seguro. Solo el propietario final cierra el fd, después de que no haya lectores/escritores activos.
4. Durante esta transición, `shutdown(fd, SHUT_RDWR)` puede despertar al worker; no sustituirlo por `close` desde otro hilo. Impedir que nuevos escritores adquieran una conexión `Closing`.
5. Eliminar todos los cierres independientes de broadcast, stop y handler. Revisar cada `close(` con `rg` y documentar su dueño.
6. Proteger también la identidad del pathname del listener y la salida de `accept` durante stop.

**Pruebas:** T10–T14. **Aceptación:** ninguna intercalación cierra el recurso reasignado, 17.º cliente rechazado limpiamente y repetir stop/remove es inocuo. T15 puede seguir fallando hasta L06; no declararlo resuelto.

### L06 Hacer no bloqueante y acotado el servidor IPC

**Archivos:** clases IPC nuevas, métodos `broadcastIPCType`, `send*ToClient`, `handleIPCMessageType` de `OpenA8DJUSB.m`.

1. Implementar exactamente D5 con sources de lectura/escritura y cancel handlers, reemplazando workers bloqueantes por conexión.
2. Acumular cabecera y payload parcial; validar magic/version/tamaño antes de reservar memoria adicional. EOF retira la conexión aunque ocurra a mitad de frame.
3. Centralizar todos los envíos, incluidos hello, controles, stats, ledger y MIDI, en el escritor de la conexión.
4. Reservar capacidad antes de encolar un frame. Un broadcast copia la lista o se encola en la cola IPC, nunca espera a escrituras desde EP1/USB.
5. Al parar: prohibir altas, cancelar listener y clientes, esperar sus confirmaciones por notificación asíncrona; cerrar solo tras cancelación completa.
6. Retirar el mutex cuyo propietario podía quedarse en `write`. Añadir métricas de saturación y desconexión por plazo.

**Pruebas:** T15–T23 y repetir T10–T14. **Aceptación:** cliente lento aislado; cliente sano sigue recibiendo frames; el test de contrapresión queda verde sin que el test drene el socket para permitir el cierre.

### L07 Adaptar el daemon MIDI al nuevo cierre

**Archivos:** `src/tools/opena8dj-midid.m`, conexión IPC compartida de L05/L06, targets de compilación del daemon.

1. En el callback MIDI copiar a una cola preasignada y retornar; un worker de transporte envía usando `OpenA8DJIPCConnection`. Mantener esa clase independiente de IOUSBHost para poder enlazarla en ambos binarios. No mantener `gSocketMutex` durante I/O. Aplicar límite 128 frames/128 KiB y, si se desborda, invalidar la conexión y contar el incidente; no silenciar el problema descartando arbitrariamente bytes dentro de mensajes.
2. Usar escritura parcial, `EINTR/EAGAIN`, protección SIGPIPE y un dueño único de socket. Mantener nombres y bytes del protocolo v1.
3. Cuando termina el servidor, invalidar la conexión y reconectar con 100 ms, 250 ms, 500 ms, 1 s y máximo 2 s entre intentos. Una conexión estable reinicia ese backoff. No hacer logging por intento en un bucle infinito rápido.
4. Retener también el endpoint MIDI de destino para poder disponerlo. Implementar salida SIGTERM/SIGINT mediante fuente de señal; nada de llamadas Objective-C desde un signal handler POSIX crudo.
5. No habilitar `OPENA8DJ_MIDI_KEEPALIVE`; si el modo ya se pidió explícitamente, detener y destruir su IOProc durante shutdown.

**Pruebas:** T24–T29. **Aceptación:** cerrar el servidor durante envío no termina el daemon por SIGPIPE; SIGTERM dispone recursos; reconexión no crea endpoints duplicados.

### L08 Corregir rollback de apertura USB

**Archivos:** `OpenA8DJUSB.m`, adaptador de plataforma, tests de apertura.

1. Enumerar las etapas de `openUSB`: service, device, configuración, interface service, interface, alt setting, bulk OUT, bulk IN, EP1, device info/controls, captura, playback e IPC.
2. Introducir un objeto de recursos adquiridos. Limpiarlo por campos existentes; eliminar el guard de `_running` que evita la limpieza de una apertura parcial.
3. No sobrescribir un handle tomado en exclusiva antes de liberar de forma segura el intento anterior. Balancear cada `IOObjectRelease`.
4. Si EP1 ya fue iniciado, primero cancelarlo y drenar antes de destruir pipes/interface/device. Si no puede drenar, devolver error y conservar el contexto; no dar por limpio el estado.
5. Definir qué fallos son fatales: device info, pipes obligatorios y controles necesarios para el perfil. Un listener IPC que no se pudo crear es fallo del producto integrado; no anunciar bridge listo.
6. Añadir cleanup idempotente llamado desde fallo, close y dealloc seguro. La repetición no debe contar como doble liberación de propiedad.

**Pruebas:** T30–T34, todos los puntos de fallo parametrizados. **Aceptación:** cada recurso adquirido se entrega una vez, no quedan handles de un intento fallido y un segundo intento sano funciona.

### L09 Serializar lifecycle y publicar referencias estables

**Archivos:** `OpenA8DJLifecycle.h/.m`, wrappers globales al final de `OpenA8DJUSB.m`, `OpenA8DJUSB.h`.

1. Conectar D1–D3 al motor real. Las funciones públicas solicitan operaciones con ID y reciben un resultado; no ejecutan un cierre bloqueante bajo `gEngineMutex`.
2. Ejecutar llamadas SDK potencialmente bloqueantes en la cola de recursos; completar por mensaje al coordinador. Solo una operación de recursos viva por motor.
3. Un llamador HAL que necesite respuesta espera hasta su deadline sin sostener `gIOMutex`, `gEngineMutex` ni otro lock requerido por completion.
4. Una respuesta tardía con operationID inválido solo limpia su propio resultado; no publica éxito ni sustituye el motor vigente.
5. Implementar adquisición de referencia de audio y contadores de lectores según D2. Despublicar antes de iniciar retirada. Cero destrucciones de motor/buffers desde callback de audio.
6. Establecer prohibiciones comprobables: el coordinador no espera a USB, USB no espera al coordinador, no `dispatch_sync` sobre la cola actual. Usar queue-specific keys y asserts solo en diagnóstico.

**Pruebas:** T35–T39. **Aceptación:** una apertura/destrucción falsa que nunca responde no bloquea snapshots ni callbacks de audio; el API retorna timeout y conserva un solo contexto pendiente.

### L10 Convertir EP1 y órdenes de control en operaciones cancelables

**Archivos:** `ep1ReadLoop`, `sendCommand`, `sendCommandNoReply`, `sendAudioParams*`, `handleEP1Bytes`, integración IPC.

1. Sustituir el bucle EP1 por `submitNextEP1Read` y completion. Cada petición retiene buffer/contexto y generation.
2. Serializar órdenes de control pendientes, con plazo total y resultado cancelable. Rechazar por capacidad sin bloquear al productor.
3. Al parar control, impedir lecturas nuevas y cancelar la petición actual; una completion vieja termina su token y no repone lectura.
4. Eliminar la espera ilimitada de `_pendingActive`; integrar el tratamiento de respuesta tardía definido en D6.
5. Comprobar un timeout de control durante MIDI intenso: no debe quedar una orden pendiente para siempre ni confundirse la respuesta del intento anterior.

**Pruebas:** T40–T45. **Aceptación:** una completion ausente produce timeout controlado, close conserva sus buffers y futuras órdenes no reutilizan el canal inválido.

### L11 Cancelar y drenar cada sesión de audio

**Archivos:** `start`, `stop`, `queueCaptureTransfer`, `queuePlayback*`, `handleCaptureTransfer`, `handlePlaybackTransfer`, pools y adaptador USB.

1. Preparar buffers y petición de sesión antes de publicar `Streaming`. Confirmar que el arranque ha aceptado las peticiones iniciales necesarias; no éxito inmediato antes del worker.
2. Añadir tokens y contadores D3 a captura y playback. Hacer atómico el acuerdo entre submit aceptado y completion, incluyendo completion inmediata en el falso.
3. Stop invalida generación/prohíbe submissions, despublica la sesión, solicita abort asíncrono y espera los tokens. Registrar cada fallo de abort.
4. Cancelar reintentos/timers anteriores. Ningún callback viejo reencola aunque una nueva sesión ya esté `Streaming`.
5. Solo reutilizar pools después del drenaje completo. Quitar `atomic_store(inFlight, 0)` como forma de olvidar trabajo vivo.
6. Al vencer stop, devolver `Timeout` y dejar `Faulted`; no reiniciar el mismo pool. Si posteriormente drena, limpiar y pasar a `OpenIdle` o `Closed` según demanda y shutdown.
7. El path preparado opcional conserva sus handles y solo destruye su bridge tras retirar las peticiones asociadas. Mantenerlo desactivado por defecto y comprobar que compila.

**Pruebas:** T46–T53. **Aceptación:** cancelación completa, error y completion tardía funcionan sin use-after-free ni doble completion. Primera sesión no puede modificar la segunda.

### L12 Reconciliar clientes y propiedades HAL

**Archivos:** `OpenA8DJHAL.c`; callbacks de cliente, configuración, reloj y notificaciones; puente lifecycle.

1. Reemplazar el contador aislado por registro D7, manteniendo un total derivado. Registrar/balancear CFStrings de client info.
2. Reservar una solicitud de Start sin retener el lock durante USB. Al completar, validar cliente/operationID; un Remove concurrente cancela esa demanda y evita revivirla.
3. Stop/Remove concurrentes son idempotentes respecto al transporte. Quitar tareas de cierre originadas por Stop extra sin demanda.
4. Aplicar frecuencia/buffer de forma transaccional. No cambiar el reloj hasta confirmar el commit. Registrar Abort de la solicitud correcta.
5. Implementar notificaciones fuera de locks y por transición. Snapshots de propiedades son coherentes entre hilos; no leer `gRunningClients` o frecuencia con una disciplina distinta de escritura.
6. Al perder el transporte, coherencia de DeviceIsAlive y running físico según el contrato HAL; conservar la demanda internamente sin afirmar streaming sano. Confirmar semántica con header SDK y tests del host falso.

**Pruebas:** T54–T62. **Aceptación:** el host falso puede reentrar desde PropertiesChanged sin deadlock; fallo de USB no devuelve éxito ni publica frecuencia inexistente; no tormenta de eventos.

### L13 Recuperar desconexión y suspensión

**Archivos:** adaptador IOUSBHost, lifecycle, estado visible HAL.

1. Instalar interest handlers para device/interface. Convertir terminación, suspensión, reanudación y petición de cierre en eventos con generation. Copiar solo datos cuya vida esté garantizada durante el callback.
2. Terminar/invalidate una sola vez aunque lleguen eventos duplicados del dispositivo y la interfaz. Mensajes causados por nuestra configuración no deben disparar una reapertura circular.
3. Añadir detección de reaparición del dispositivo mediante notificación de matching; verificar registry ID y VID/PID. No sondear cada milisegundo.
4. Después del drenaje, permitir tres intentos de recuperación a 100 ms, 500 ms y 2 s. Una nueva conexión física o una nueva petición explícita puede reiniciar el presupuesto; un timer repetido no.
5. Antes de reutilizar OpenIdle, validar la conexión por generación/estado y negociación de arranque. Ante pipe/control inválido, recuperar antes de publicar audio.
6. Comprobar si las notificaciones de servicio cubren sleep/wake real. Si no, usar notificación de energía IOKit desde el adaptador, con acknowledgements y cleanup según SDK. No inventar mensajes de energía dentro de interestHandler.
7. Si quedan clientes con demanda y drena bien, reabrir, renegociar y restablecer streaming; sin demanda, quedarse en OpenIdle. Restaurar el último perfil/controles confirmados, no imponer DVS Vinyl al recuperar una sesión con otro perfil. Si se agota presupuesto, publicar error estable y sin spin.

**Pruebas:** T63–T68. **Aceptación:** no reapertura durante shutdown, no dos dueños simultáneos y no reintentos ilimitados. La cobertura real de sleep/wake se confirma en L19.

### L14 Retirar timers y demostrar descarga segura

**Archivos:** `BackgroundPreopenThread`, `DelayedCloseThread`, `Schedule*`, `OpenA8DJUSBDestructor`, lifecycle y host de prueba NUEVO.

1. Reemplazar hilos detached por timers/tareas registrados; cancelar y esperar confirmación de finalización antes de soltar el coordinador.
2. Implementar `PrepareForUnload` con resultado explícito y snapshot de trabajo pendiente. El destructor comparte ese camino, sin crear tareas nuevas ni usar `gHost` después de invalidarlo.
3. Crear un bundle de pruebas con plataforma falsa y un host hijo que lo cargue y descargue 100 veces; probar unload durante preopen, grace period, respuesta EP1 y stop.
4. Añadir prueba de operación SDK/completion que nunca vuelve: el host hijo debe declarar que no es seguro descargar; el padre termina solo a su hijo dentro del plazo. Esto prueba contención, no unload cooperativo exitoso.
5. Verificar el camino real de Core Audio y documentar quién conserva la vida del módulo hasta las cancelaciones. Si no hay garantía demostrable, marcar la descarga in-process en fallo como no soportada y exigir retirada del proceso aislado desde el helper. No afirmar que una referencia al objeto conserva automáticamente el código del dylib.

**Pruebas:** T69–T73. **Aceptación:** cero ejecuciones de código tras unload exitoso; timeout no provoca liberación insegura. No avanzar a candidato físico sin cerrar esta decisión con evidencia.

### L15 Crear la transacción de instalación en un filesystem falso

**Archivos:** NUEVOS `scripts/opena8dj-lifecycle-admin`, `scripts/lib/opena8dj-install-transaction.sh`, `tests/macos/test_install_transaction.py`; `hardware-lock-lib.sh`.

1. Implementar comandos `install`, `uninstall`, `recover`, `status` y `resume-transaction`. En esta tarjeta solo backend falso de comandos y rutas bajo un directorio temporal.
2. Implementar D9 con journal persistente versionado. Escribir estado por temp+rename y verificar hashes después de copy/restore. Registrar el “intento de operación” antes de cada mutación para poder reconciliar un corte.
3. Backup completo, staging, validación de candidato, sustitución, validación de host/enumeración y rollback. Un rollback correcto no transforma una instalación fallida en exit 0.
4. Errores/traps EXIT/INT/TERM intentan rollback idempotente. SIGKILL/corte eléctrico no ejecutan trap: la siguiente invocación inspecciona el journal antes de continuar. No prometer recuperación inmediata tras perder alimentación.
5. Probar lock ocupado, herencia legítima, herencia falsa, PID reciclado, lock stale y falta de privilegios. El cleanup solo libera su token.
6. Rechazar paths vacíos, `/`, enlaces inesperados y candidate dentro del destino activo. No borrar backups al fallar la recuperación.

**Pruebas:** T74–T82. **Aceptación:** todos los puntos de corte dejan estado original íntegro o journal reanudable; ningún test usa sudo, launchctl o `/Library` real.

### L16 Conectar instalador real y hooks PKG

**Archivos:** `Makefile:install-hal`, `resources/pkg/scripts/*`, `scripts/test-hal-candidate-safety`, `audio-stack-guard`, `audio-stack-health`, `find-opena8dj-driver-pid`; helper de L15.

1. Hacer que `make install-hal` y el gate de candidatos llamen al mismo helper transaccional. No copiar/signar de nuevo dentro del destino activo.
2. Detectar el host por identidad verificable y PID/start-time; manejar cero, uno y varios resultados. No seleccionar `head -1`. Confirmar salida del viejo antes de declarar reemplazo cargado.
3. Reiniciar solo lo necesario con timeout, validar enumeración y diagnóstico del motor, y contrastar el hash activo con el candidato. La presencia del bundle no equivale a host listo.
4. Los PKG usan el journal/lease de D9. `preinstall` verifica y respalda antes de retirar archivos; `postinstall` verifica todos los payloads y LaunchAgent, finaliza o restaura. Coordinar paquetes del mismo producto y usuarios de consola distintos.
   Comprobar los recibos además del filesystem y aplicar el estado de recuperación parcial de D9 cuando difieran; la reinstalación reparadora se ejecuta fuera de hooks.
5. Uninstall retira la instancia y herramientas correctas, espera host y verifica ausencia; no elimina drivers ajenos ni backups históricos a ciegas. El daemon se detiene con confirmación.
6. Recuperación recoge logs/snapshot primero. Si el host aislado no responde, permite su terminación gestionada; registra que hubo recuperación. Para campañas de estabilidad normal eso cuenta como fallo, aunque recupere.
7. Corregir documentación que afirma usar locks/gates inexistentes. Mantener firma del artefacto; no re-firmar un candidato de release con firma ad hoc.

**Pruebas:** T83–T88 usando backend falso, además de L15. **Aceptación offline:** dry-run exacto, errores reales propagados y scripts de paquete verificables. La aceptación de instalación real se deja a L19, no se presupone aquí.

### L17 Integrar smoke seguro y puerta offline del candidato

**Archivos:** Makefile, CMake, `src/tools/hal-smoke.c`, `hal-parity-smoke.c`, scripts de construcción de candidatos, CI, NUEVO `scripts/run-macos-lifecycle-offline`.

1. Crear el runner descrito en la especificación de pruebas. Ejecutar Debug, Release, ASan/UBSan y TSan en procesos separados; capturar exit code sin ocultarlo con `tee`.
2. Clasificar los smokes existentes que cargan bundles con preopen como hardware. Para offline usar el bundle falso de L14. Un smoke de producción debe adquirir lock y ejecutarse bajo watchdog de proceso.
3. Integrar todos los `.m/.c` nuevos en Makefile y CMake. Construir también la configuración prepared opcional sin activarla.
4. Crear target NUEVO `hal-lifecycle-candidate` que construye dentro de `build/lifecycle-candidate/` y produce `OpenA8DJ-lifecycle-candidate.driver` y manifiesto completo. No sobrescribir el bundle de rollback.
5. Copiar exactamente los parámetros de audio de la referencia publicada a un perfil declarado; no confiar en un `hal-build-config.txt` antiguo. Identificar candidato con build/version de diagnóstico, no presentarlo como el binario 0.5.1 publicado.
6. Ejecutar 1.000 ciclos de fault injection y seeds publicados según la especificación. No hay supresiones amplias de sanitizers ni tests nuevos deshabilitados.

**Pruebas:** T89–T94 y todos los T01–T88. **Aceptación:** suite offline verde, hash fuente correcto, ninguna violación de sanitizer y bundle completo sin instalación.

### L18 Preparar la campaña física sin ejecutarla todavía

**Archivos:** NUEVOS `scripts/run-macos-lifecycle-physical`, `src/tools/audio-lifecycle-test.c`, `scripts/collect-lifecycle-incident`; documentación de pruebas.

1. El runner implementa `--preflight`, `--scenario`, `--cycles`, `--candidate`, `--baseline`, `--run-dir` y `--dry-run`. Sin escenario explícito no toca audio.
2. El helper C selecciona por UID exacto, nunca por “primer dispositivo”; crea/inicia/para/destruye IOProcs y escribe NDJSON de cada transición. Cambia rate/buffer solo en escenarios que lo requieran.
3. Todas las llamadas potencialmente colgantes se ejecutan en hijo supervisado. Al timeout el padre guarda evidencia y detiene solo su hijo; no intenta recuperarse repitiendo StartIO indefinidamente.
4. Capturar recursos y diagnosticar el proceso HAL correcto antes/después. Guardar baseline y restaurar ruta/rate/defaults modificados por la prueba.
5. Tests de interfaz física humana (Traktor, cables, sleep) usan pasos guiados; el runner no finge que una acción humana ocurrió. El operador confirma y el runner verifica el efecto observable.
6. Escribir `campaign.json` con todas las filas obligatorias de H01–H12, inicialmente `NOT_RUN`. Una fila saltada bloquea la promoción.

**Pruebas:** T95–T98 con procesos y dispositivos falsos. **Aceptación:** dry-run muestra acciones, dueño del lock, artefactos y rollback; no hay cambios físicos para construir o probar el runner.

### L19 Ejecutar campaña física y decidir publicación

**Archivos:** evidencia local de campaña, manifiesto del candidato, docs de instalación/troubleshooting/validación/release.

1. Seguir H01–H12 en el orden de la especificación. Primero baseline y tres ciclos cortos; solo después repeticiones largas.
2. Usar el candidato exacto aprobado offline. Tras cambiar código, recompilar y repetir desde H02 lo afectado; no mezclar hashes dentro de una campaña aceptada.
3. Al primer hang, crash, pérdida funcional o recuperación de servicios no prevista: recoger incidente, restaurar baseline por el helper y detener la campaña. No continuar acumulando “ciclos buenos”.
4. Revisar audio/DVS con el operador y completar el soak de 24 h. Las métricas y la escucha deben pertenecer al mismo candidato.
5. Documentar recuperación acotada, ubicación de logs, significado de OpenIdle y límites pendientes. No recomendar reiniciar macOS como salida normal.
6. Preparar un candidato de release firmado/notarizado con versión nueva elegida según el proceso de release. El cambio de firma/hash exige repetir instalación y smoke del artefacto final; cambios de fuentes/flags exigen la validación correspondiente completa.
7. La publicación es una acción separada y requiere que el usuario la haya pedido. Dejar artefactos y resultado de gates revisables antes de ese paso.

**Aceptación:** todos los H01–H12 en PASS, rollback verificado, ningún reinicio de macOS ni recuperación oculta, y aceptación de sonido/timecode registrada. Si falta hardware/operador, estado `WAITING_PHYSICAL`; no marcar el objetivo de estabilidad como completo.

## Tabla de cobertura de la auditoría

| Hallazgo | Implementación | Pruebas principales |
| --- | --- | --- |
| IPC bloquea el cierre | L05–L07 | T10–T29, H05 |
| Doble cierre y fd reutilizado | L05–L06 | T10–T14, T21 |
| Apertura parcial retiene USB | L08 | T30–T34 |
| Stop sin drenaje fiable | L09–L11 | T35–T53, H03–H04 |
| USB inválido y retry storm | L13 | T63–T68, H06–H07 |
| Cliente eliminado y contador stale | L12 | T54–T57, H04 |
| Propiedades y frecuencia incoherentes | L12 | T58–T62, H08 |
| Detached threads y unload | L14 | T69–T73, H02, H09 |
| Installer sin rollback/lock/health | L15–L16 | T74–T88, H09 |
| Tests contra otro checkout | L01–L02 | T01–T05 |
| Smoke offline que toca USB | L03, L17 | T06, T89–T94 |
| Logs insuficientes | L04, L18 | T07–T09, T95–T98 |
| Regresión de audio o latencia | L17, L19 | H10–H12 y gates de audio existentes |

## Cuándo debe detenerse el ejecutor

- Un test vuelve a leer otro checkout, el driver instalado cambia durante una tarea offline o el falso llama al hardware: detener y corregir infraestructura antes de continuar.
- No se puede explicar quién posee un fd, buffer, callback o módulo durante timeout: no implementar “free al vencer”. Anotar el caso y resolverlo antes del siguiente gate.
- La versión del SDK no soporta una API del diseño: registrar el símbolo exacto y la limitación; no sustituirla silenciosamente por una espera ilimitada.
- Los datos físicos no cumplen un umbral: conservar el fallo. Solo repetir cuando se haya identificado y corregido una causa o documentado una invalidación del ensayo.
- Lock ocupado por un dueño vivo: terminar el intento como `BUSY` sin acciones sobre audio. No matar el dueño.
- Falta una ventana física o la acción necesaria del operador: continuar tareas offline independientes si quedan; registrar `WAITING_PHYSICAL` para lo restante.

## Prompt de continuación para un modelo pequeño

```text
Trabaja en la estabilidad macOS de OpenA8DJ siguiendo estos archivos del repo:
docs-state/investigations/macos-lifecycle-implementation-plan-2026-10-10.md
docs-state/investigations/macos-lifecycle-test-spec-2026-10-10.md
docs-state/investigations/macos-lifecycle-implementation-status.json

Lee las decisiones D1–D9 y ejecuta SOLO la siguiente tarea pendiente Lxx.
Antes de editar, comprueba rama, git status y hashes de baseline. Preserva cambios
ajenos. No vuelvas a hacer la auditoría completa ni cambies el diseño de audio.
Implementa los pasos de la tarjeta y sus casos Txx. Los nombres NUEVOS son
entregables por crear, no herramientas ya disponibles. Un resultado no ejecutado
es NOT_RUN. Revisa la salida y el exit code de cada comando.

No instales, reinicies servicios ni pruebes hardware en L00–L18. Esas tareas
usan mocks, procesos hijos y carpetas temporales. L19 sigue el lock común,
las condiciones de la campaña y la autorización física disponible del usuario.
No publiques una release sin petición de publicación.

No declares resuelto un fallo solo por añadir un timeout. Prueba que recursos
y callbacks siguen siendo válidos, y que no pueden contaminar una sesión nueva.
No ocultes fallos quitando tests, cambiando thresholds o usando otro checkout.

Al terminar actualiza status.json con archivos, comandos, evidencia, resultados
y siguiente tarea. Si algo impide completar la tarjeta, describe la condición
concreta y deja su estado incompleto. Resume en español lo cambiado, cómo se
probó y qué queda. No marques el plan entero completo por acabar una tarjeta.
```
