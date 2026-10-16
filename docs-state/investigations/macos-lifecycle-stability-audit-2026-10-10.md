# Auditoría de estabilidad del driver macOS

Fecha: 10 de octubre de 2026. Alcance: carga, apertura USB, inicio y parada de audio, cierre de Traktor, desconexión, IPC de control y MIDI, instalación y descarga.

Continuación: [plan de implementación por tareas](macos-lifecycle-implementation-plan-2026-10-10.md), [especificación de pruebas](macos-lifecycle-test-spec-2026-10-10.md) y [estado de ejecución](macos-lifecycle-implementation-status.json). El plan está diseñado; las correcciones todavía no están implementadas.

**El ciclo de vida necesita correcciones antes de considerarlo robusto para uso prolongado.** Hay tres defectos reproducidos ejecutando el código de transporte en un proceso aislado: limpieza incompleta de una apertura fallida, doble cierre de descriptores y cierre bloqueado por un cliente IPC que no lee. Hay además carencias de recuperación USB, de aislamiento entre sesiones y de validación de la instalación. Son mecanismos compatibles con estados que solo desaparecen al reiniciar servicios o el equipo; no prueban cuál causó los incidentes concretos relatados por el usuario.

## Versión y evidencia

- Repositorio auditado: `46a80ff`, árbol limpio al comenzar. Los archivos de `src/hal` y `src/tools/opena8dj-midid.m` no tienen diferencias respecto a `927d8af`, el commit de audio de la versión pública.
- Instalado: OpenA8DJ **0.5.1, build 151**, en macOS **26.6.2, build 25G83**.
- SHA-256 instalado: `4af4b1207f81846208fd2fcc0b8f5a600c2e11346d523ee5fd8e4f55700b9f66`. Coincide con [la referencia publicada](../current-release-state.md).
- La implementación activa es HAL + IOUSBHost en espacio de usuario. Los contratos de DriverKit no validan directamente este transporte Objective-C.
- En la muestra de procesos, Traktor estaba cerrado; el proceso del driver y el puente MIDI mostraban 0 % de CPU. El proceso HAL llevaba aproximadamente tres horas vivo. Esto por sí solo es normal y no demuestra un bloqueo.
- La consulta acotada del log unificado de las últimas 24 horas encontró activación del dispositivo a las 14:23 y selecciones posteriores como salida preferida. No aporta una traza del fallo de cierre. No aparecieron informes con nombres relacionados con OpenA8DJ, Traktor, Core Audio o panic en los directorios de diagnósticos inspeccionados. No se puede atribuir un reinicio del sistema al driver con esta evidencia.

Evidencia local: `local-analysis/stability-audit-20261010/` y `local-analysis/macos-lifecycle-audit/report.json`. El antiguo `build/hal-build-config.txt` corresponde a **0.3.135** y no se ha usado como manifiesto del binario instalado.

## Qué ocurre al cerrar Traktor

Los valores del Makefile de la línea publicada son `HAL_BACKGROUND_PREOPEN_ON_INIT=1`, `HAL_STOP_ISOC_ON_STOP=1`, `HAL_BACKGROUND_WARM_OPEN=0` y `HAL_STOP_GRACE_USEC=10000000`.

1. `Initialize` lanza una apertura USB en segundo plano. La apertura toma el dispositivo y la interfaz con `DeviceSeize`, configura USB y arranca EP1 e IPC.
2. El primer `StartIO` inicia el streaming. Los demás clientes incrementan un contador global.
3. El último `StopIO` detiene las transferencias isócronas y el reloj, y programa un hilo para diez segundos después.
4. Con preapertura habilitada, ese hilo **conserva USB, EP1 e IPC abiertos**. No llama a `OpenA8DJUSBClose`.
5. Una nueva sesión reutiliza ese motor; un cambio de frecuencia puede cerrarlo y sustituirlo. La descarga de la biblioteca llama a `OpenA8DJUSBClose` desde un destructor.

Por tanto, cerrar Traktor, detener el streaming, cerrar USB y descargar el plug-in son operaciones distintas. Conservar el control/MIDI abierto puede ser intencional, pero exige validar la conexión al reutilizarla y disponer de recuperación. Apagar todo al cerrar Traktor tampoco sería una corrección completa: podría romper el uso independiente de MIDI.

Referencias: `Makefile:166`, `src/hal/OpenA8DJHAL.c:1280`, `:1320`, `:2043`, `:2071`; `src/hal/OpenA8DJUSB.m:3351`, `:6998`, `:7160`. La LaunchAgent instalada no activa `OPENA8DJ_MIDI_KEEPALIVE`; no hay base para culpar a ese modo opcional de mantener el audio en marcha.

## Hallazgos prioritarios

### P1 El servidor IPC puede impedir el cierre del motor

**Reproducido.** `broadcastIPCType` mantiene `_ipcClientsMutex` mientras `IPCSend` hace escrituras bloqueantes. Si un cliente conectado deja de leer y se llena su buffer, el hilo queda dentro de `write`. `stopIPCServer` necesita ese mismo mutex para cerrar los clientes. No existe tiempo límite de escritura ni una cola acotada que desacople a cada cliente.

La prueba llena un `socketpair` privado, ejecuta el broadcast real y solicita el cierre. Ambos quedan bloqueados durante las ventanas de observación de 200 ms; terminan al drenar el socket del cliente. No depende de simular un fallo de IOUSBHost. En producción, un envío MIDI recibido por EP1 también pasa por este broadcast: un cliente lento puede detener la lectura de respuestas de control.

**Impacto:** el cierre USB, el cambio de frecuencia o la descarga pueden quedar esperando a una aplicación de control/MIDI. Las funciones públicas de cierre mantienen `gEngineMutex` durante toda la operación, extendiendo el bloqueo a otros accesos al motor.

**Corrección:** escrituras no bloqueantes por conexión, cola con capacidad y política de descarte/desconexión, un único escritor por socket, cancelación que despierte lectores y aceptación, y espera acotada de finalización. Nunca mantener el mutex del registro de clientes durante I/O.

Código: `src/hal/OpenA8DJUSB.m:1945`, `:3511`, `:4704`, `:4903`, `:5009`, `:7088`.

### P1 El cierre de IPC puede cerrar un recurso nuevo por reutilización de descriptor

**Reproducido.** `stopIPCServer` cierra los descriptores y borra las entradas. Cuando un manejador termina, `removeIPCClient` ejecuta otro `close(fd)` aunque ya no encuentre ese descriptor en la lista. Entre ambas operaciones el sistema puede reutilizar el número para otro socket o archivo. El camino de fallo de broadcast presenta el mismo problema. Además, `addIPCClient` cierra una conexión si se supera la capacidad, pero su llamador continúa enviando y lanzando el manejador con ese número.

La prueba registra un socket, ejecuta `stopIPCServer`, reutiliza su descriptor para `/dev/null` y llama al cleanup tardío real. **El archivo nuevo queda cerrado** (`EBADF`).

**Corrección:** cada conexión necesita identidad y propiedad explícitas, cancelación separada de liberación, y exactamente un cierre final. El registro debería contener objetos de conexión con generación, no únicamente enteros susceptibles de reutilización. El alta debe devolver éxito o fallo al llamador.

Código: `src/hal/OpenA8DJUSB.m:3475`, `:3494`, `:3523`, `:4628`, `:4695`, `:4712`.

### P1 La apertura parcial no siempre revierte los recursos adquiridos

**Reproducido con inyección de fallo.** `openUSB` puede adquirir `_device` y `_interface` y fallar antes de establecer `_running=true`. `ensureOpen` llama a `close`, pero `close` retorna inmediatamente si `_running` era falso. En ese caso no llega a `closeUSB` y conserva objetos que ya podían tener el dispositivo tomado en exclusiva.

La prueba sustituye únicamente la adquisición por un objeto falso que cuenta llamadas a `destroy`. Ejecuta los métodos de producción `ensureOpen` y `close`: la apertura devuelve fallo, el recurso sigue retenido y el contador de destrucciones permanece en cero. No demuestra una fuga perpetua: la sustitución o liberación posterior del objeto podría liberarlo; demuestra que **el intento fallido no deja el estado limpio**, y el motor global puede seguir reteniéndolo.

**Corrección:** apertura transaccional con rollback en cada etapa. La limpieza debe depender de qué recursos existen, no del indicador de funcionamiento. Separar `Opening`, `OpenIdle`, `Streaming`, `Stopping`, `Faulted` y `Closed`; no publicar el motor como utilizable hasta completar la apertura.

Código: `src/hal/OpenA8DJUSB.m:3351`, `:3418`, `:4725`, `:4903`, `:4918`, `:7001`.

### P1 La parada no tiene una condición verificable de finalización

**Confirmado por inspección; bloqueo USB concreto no reproducido.** `stop` y `close` ignoran los errores de `abortWithError`, usan `dispatch_sync` sin límite y esperan al lector EP1, que a su vez usa `DISPATCH_TIME_FOREVER`. La petición EP1 tiene timeout USB de un segundo, pero la espera del software depende de recibir su completion; no es una espera cancelable independiente.

`waitForPlaybackTransfersToDrain` espera hasta 250 iteraciones de un milisegundo y retorna sin comunicar si drenó o agotó el plazo. No hay verificación equivalente de todas las capturas pendientes. `start` pone el contador de playback a cero y reinicializa buffers. Las completions y los reintentos retardados no llevan una generación de sesión que permita descartar trabajo antiguo.

Apple documenta `abortWithError` como abort síncrono. **No se afirma que todo abort exitoso deje callbacks pendientes.** El defecto de robustez está en ignorar el fallo del abort, no comprobar el drenaje y permitir reutilización sin una garantía propia. Referencia: [IOUSBHostPipe abortWithOption](https://developer.apple.com/documentation/iousbhost/iousbhostpipe/abortwithoption:error:), contrastada con los headers del SDK local.

**Corrección:** invalidar la generación antes de detener envíos, comprobar aborts, contar captura y reproducción pendientes, y finalizar solamente cuando todos los propietarios hayan cedido sus recursos. Un timeout debe pasar a estado de error recuperable, conservando buffers aún usados; nunca liberar memoria porque se agotó un temporizador. Separar la publicación del motor para callbacks de audio de los locks que pueden esperar USB o IPC.

Código: `src/hal/OpenA8DJUSB.m:4738`, `:4802`, `:4870`, `:4880`, `:4903`, `:5033`, `:5620`, `:6041`, `:6998`, `:7124`.

### P1 Falta recuperación explícita de una conexión USB inválida

**Confirmado por inspección; efecto físico pendiente de validar.** Dispositivo e interfaz se crean con `interestHandler:nil`. `ensureOpen` interpreta `_running=true` como prueba suficiente de validez. Si `setAudioParams` falla al arrancar, `start` retorna sin cerrar ni invalidar ese motor. Con la política de mantener USB abierto, una sesión posterior puede volver al mismo estado averiado.

Tras fallos de envío de captura se programa un reintento cada milisegundo por cadena activa, sin presupuesto de reintentos ni clasificación de desconexión. Las completions de captura vuelven a encolar mientras `_streaming` siga activo. `start` devuelve éxito antes de que el worker haya demostrado que puede enviar transferencias. El sistema operativo podría descargar el proceso por las condiciones del plist cuando desaparece el dispositivo; eso no sustituye una recuperación propia ni garantiza el caso de suspensión o fallo sin retirada del servicio.

**Corrección:** tratar terminación, reset y suspensión/reanudación; invalidar pipes y estado al fallar la conexión; limitar reintentos con espera creciente; realizar una reapertura controlada; devolver un error útil si no puede arrancar. Mantener MIDI/control en reposo debe ser un estado explícito y recuperable.

Código: `src/hal/OpenA8DJUSB.m:3359`, `:3383`, `:4725`, `:4746`, `:4855`, `:5608`, `:6042`; `resources/OpenA8DJ.driver/Contents/Info.plist`.

### P1 La instalación habitual omite la validación que detectaría un stack atascado

**Confirmado por inspección.** `make install-hal` elimina el bundle activo, copia el nuevo, firma y ejecuta `killall coreaudiod || true`. No toma el lock compartido, conserva una copia de reversión ni espera enumeración saludable. Esto contradice la afirmación de `docs/SHARED_HARDWARE_COORDINATION.md` de que esa ruta usa el cargador seguro.

Los scripts PKG también eliminan el bundle antes de sustituirlo, ignoran errores de reinicio/bootstrap y no validan la salud final. Un reinicio solicitado con `killall` no demuestra que el host anterior haya terminado ni que el nuevo dispositivo responda. Una instalación interrumpida tras `preinstall` puede dejar el producto sin sus archivos anteriores.

Existe `scripts/test-hal-candidate-safety`, que mide CPU y enumeración con timeout, pero no está conectado a la ruta normal. Tampoco es una solución completa: mueve el driver anterior y no lo restaura automáticamente, y su trap solo libera el lock. Una señal o fallo a mitad puede dejar el candidato o el dispositivo ausente. Los scripts de recuperación de audio revisados tampoco implementan el lock que la documentación les atribuye.

**Corrección:** unificar instalación de desarrollo y validación con staging fuera del directorio activo, coordinación, verificación de firma, backup, sustitución controlada, comprobación del host y enumeración con plazo, y rollback verificable. Devolver fallo real cuando reiniciar o validar falle. Restringir la recuperación al componente averiado antes de recurrir a reinicios amplios de servicios.

Código: `Makefile:924`; `resources/pkg/scripts/preinstall`, `postinstall`, `uninstall-opena8dj.sh`; `scripts/test-hal-candidate-safety:119`, `:175`; `scripts/audio-stack-health:149`; `scripts/audio-stack-guard`.

## Otros riesgos de robustez

- **P2 Estado visible incoherente.** `gDevicePresent` cambia sin notificar `DeviceIsAlive`; las funciones de notificación de frecuencia y buffer contienen un `return` incondicional. `PerformDeviceConfigurationChange` aplica la frecuencia lógica antes de intentar cambiar USB e ignora el resultado. Un fallo puede dejar al host y al transporte con versiones diferentes del estado. Corregir con transiciones atómicas y notificaciones solo cuando cambie algo; no reintroducir la tormenta de notificaciones del incidente de junio. Referencias: `OpenA8DJHAL.c:1357`, `:1382`, `:1400`, `:1517`, `:1788`.
- **P2 Clientes y trabajo diferido sin reconciliación.** Se cuenta `StartIO`/`StopIO`, pero se ignora la identidad y `RemoveDeviceClient` no actúa. No se ha probado que Core Audio omita `StopIO` al morir Traktor; debe verificarse antes de presentarlo como causa. Los hilos detached de preapertura/cierre no se unen al descargar. La generación evita cierres tardíos durante otra sesión, pero no demuestra que un hilo no ejecute código de un bundle descargado. Referencias: `OpenA8DJHAL.c:1280`, `:1340`, `:1501`, `:2043`.
- **P2 IPC necesita endurecimiento adicional.** Faltan `SO_NOSIGPIPE` o protección equivalente propia, timeout de lectura, tratamiento de `EINTR` y serialización de todos los envíos por conexión. Algunos mensajes se escriben en dos operaciones y respuestas/broadcasts pueden intercalarse. La disposición de SIGPIPE del host puede mitigar la terminación: no se ha comprobado en el proceso instalado. Referencias: `OpenA8DJUSB.m:1931`, `:1945`, `:1960`, `:3534`; `src/tools/opena8dj-midid.m:61`, `:78`.
- **P2 Observabilidad insuficiente.** El perfil por defecto desactiva trace y estadísticas calientes; no hay historial pequeño y persistente de apertura, parada, abort, generación, timeout y recuperación. Añadir eventos de ciclo de vida fuera del camino de tiempo real, con motivo y duración, permitiría diagnosticar el próximo incidente sin un log por paquete.

## Pruebas y límites de cobertura

La nueva prueba se ejecuta con:

```sh
python3 scripts/audit-macos-lifecycle-offline.py
```

Incluye `src/hal/OpenA8DJUSB.m` real, usa sockets privados y un objeto USB falso para el fallo de apertura. La única llamada `unlink` se sustituye para no tocar el socket instalado. No inicia audio, no abre USB ni se conecta al servidor real. Los métodos afectados por los tres defectos no dependen de los flags de optimización del perfil publicado.

Resultado observado:

```text
partial_open: opened=0 resource_retained=1 destroy_calls=0
ipc_double_close: unrelated_reused_descriptor_closed=1
ipc_backpressure: writer_blocked=1 stop_blocked=1
REPRODUCED: 3 lifecycle defects; no hardware or live IPC accessed
```

**El código de salida cero de este reproductor significa que se han demostrado los fallos. No certifica estabilidad.** Tras corregirlos deben convertirse estos escenarios en tests con expectativas inversas, añadiendo concurrencia, fallos de abort y callbacks tardíos.

Los smoke tests existentes consultan propiedades pero no ejercitan `StartIO`/`StopIO`. Además, llaman a `Initialize`: con preapertura habilitada pueden intentar adquirir el USB físico aun sin reproducción. No son sustitutos de un mock seguro de ciclo de vida. El gate de carga existente usa por defecto un ciclo de instalación/enumeración; eso no cubre semanas de aperturas de Traktor.

**P1 La validación de fuentes puede pasar contra otro checkout.** Varios tests C++ inspeccionan cadenas de texto o un modelo futuro. Hay 42 coincidencias de rutas fijas `/Users/fer/dev/audio8djcpp` en `tools`, incluyendo detectores de raíz que rechazan este checkout llamado `opena8dj`. Se ha confirmado que la ruta alternativa existe, apunta a `e5e1c00` y contiene fuentes diferentes. `hal_prepared_runtime_source_contract` pasa en esta ejecución, pero su resolución de raíz selecciona aquel árbol. Lo mismo afecta a otros contratos de fuentes y a consumidores de evidencia. No basta con que CTest aparezca verde: debe registrar y verificar el commit y el árbol realmente inspeccionados.

SHA-256 de `OpenA8DJUSB.m` auditado: `eabf94672b26b74a6708db5f5dec71a178e8b20782c7666c20ad12895b171a00`; en el checkout alternativo: `3b86e4fc2d43f5faa8ade623881a80d9002b83a0a0a0610eb838248f8febbba2`. La reproducción nueva incluye directamente la fuente de este repositorio y registra su hash. Referencias: `tools/hal_prepared_runtime_source_contract.cpp:19`, `tools/hal_input_spsc_ring_contract.cpp:19`, `tools/hal_runtime_geometry_observability_contract.cpp:19`, `tools/timecode_readiness_gate.cpp:12`.

La compilación completa de CMake en Debug terminó correctamente. CTest ejecutó **88 tests: 72 pasaron y 16 fallaron**, sin timeouts, en 43,29 segundos. Los contratos de core y los modelos de lifecycle/shutdown de DriverKit pasaron; eso no contradice los tres defectos del transporte HAL reproducidos aparte.

Los 16 fallos pertenecen a gates de evidencia, preparación o validación de rutas: `timecode_readiness`, `prepared_transport_migration`, `transport_budget`, `audiophile_tone`, `audiophile_precision_claim`, `human_test_rc`, `physical_evidence_frontier`, `physical_capture_forensics`, `direct_usb_path_attribution`, `physical_window_readiness`, `capture_readiness`, `physical_route_matrix`, `diagnostic_pass_semantics`, `product_quality_claim`, `hal_candidate_safety` y `hal_transport_runtime`. Las salidas muestran entradas de evidencia ausentes, un WAV histórico inexistente, prerequisitos de empaquetado/validación no satisfechos y dependencias de otro checkout. **No son 16 bloqueos físicos observados**, ni justifican declarar sano el driver a partir de los otros 72 resultados.

Comandos ejecutados:

```sh
cmake -S . -B build/stability-audit-20261010 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/stability-audit-20261010 -j 3
ctest --test-dir build/stability-audit-20261010 --output-on-failure --timeout 15 -j 2
```

Logs: `local-analysis/stability-audit-20261010/cmake-configure.txt`, `cmake-build.txt` y `ctest.txt`. No se han instalado candidatos ni reiniciado Core Audio, USB o macOS para obtener estos resultados.

## Orden de corrección y aceptación

1. **Cerrar recursos correctamente.** Corregir propiedad de conexiones IPC, I/O bloqueante y rollback de apertura. Añadir tests para cliente lento, socket cerrado, límite de clientes y reutilización de descriptores.
2. **Definir el ciclo de vida del transporte.** Un coordinador serial de estados y generaciones, stop idempotente, cancelación y drenaje comprobados, y referencias estables para callbacks. Una política explícita de reposo conserva MIDI sin confundir conexión retenida con conexión saludable.
3. **Recuperar errores.** Reapertura limitada tras fallo, hotplug y suspensión; coherencia de propiedades; trazas de transiciones. Un fallo debe producir error y posibilidad de reintento, sin bloquear indefinidamente al host.
4. **Unificar carga, descarga y validación.** Validación con timeout y rollback, identificación del proceso que aloja el HAL, rutas y firmas exactas, eliminación de rutas fijas a otro checkout y corrección de las garantías documentadas que hoy no cumple el instalador.
5. **Validar el candidato corregido con el dispositivo físico.** El driver instalado no se ha cambiado durante esta auditoría. La prueba física debe usar el lock común y registrar el hash exacto.

Matriz mínima propuesta para aceptar ese candidato:

| Escenario | Repeticiones propuestas | Criterio |
| --- | --- | --- |
| Inicio y parada de audio, uno y varios clientes | 1.000 offline con fallos inyectados | Sin deadlocks, recursos pendientes ni trabajo de una sesión anterior |
| Abrir y cerrar Traktor normalmente | 100 | Stop termina, nueva sesión reproduce/captura, sin reinicio de servicios |
| Cierre forzado de Traktor | 50 | Contabilidad de clientes reconciliada y nueva sesión funcional |
| Cambiar 44,1/48 kHz y reabrir | 100 | Frecuencia USB/HAL coherente, sin crecimiento de sockets o threads |
| Desconectar y reconectar, activo y en reposo | 30 | Recuperación o error claro, sin bucle intensivo de reintentos |
| Suspender y reanudar con USB conectado | 20 | Dispositivo utilizable, sin necesidad de reiniciar macOS |
| Cliente de control lento, abort fallido, completion tardía | Cada transición offline | Plazos cumplidos, memoria válida y cierre exactamente una vez |
| Instalar, cargar, descargar y volver a instalar | 20 bajo lock | Enumeración y host saludables; rollback probado ante fallo |
| Sesión larga con audio, DVS y MIDI | 8–24 horas | Sin pérdida funcional ni tendencia creciente de memoria, fds o threads |

Medir mediana, p95 y máximo de apertura/parada, peticiones pendientes, generaciones, número de descriptores, threads, memoria y errores. Establecer presupuestos de tiempo antes de ejecutar la matriz; cualquier necesidad de reiniciar el sistema rechaza el candidato. Repetir después la validación física de sonido y timecode de la versión publicada para evitar que una mejora de estabilidad altere la calidad o la latencia.

La prioridad es reparar la implementación actual y demostrar su recuperación. Migrar a DriverKit no elimina por sí solo los problemas de propiedad, cancelación e IPC identificados aquí.
