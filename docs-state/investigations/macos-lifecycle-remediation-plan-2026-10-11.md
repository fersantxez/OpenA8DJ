# Plan para corregir y validar la estabilidad del driver macOS

Fecha: 11 de octubre de 2026. Estado: **plan de implementación, no implementación terminada**.

La siguiente instalación debe esperar a que podamos medir correctamente los fallos, recuperar una sustitución interrumpida y demostrar que ningún trabajo pendiente usa recursos retirados. El candidato del 10 de octubre pasó una prueba corta de arranque y parada, pero no superó la retirada y recarga repetidas. No hay evidencia suficiente para atribuir al driver el posterior reinicio del Mac.

Este documento dirige la siguiente ejecución. Reordena y concreta el [plan del 10 de octubre](macos-lifecycle-implementation-plan-2026-10-10.md), conserva sus requisitos de diseño D1–D9 y reutiliza sus [pruebas T01–T98 y H01–H12](macos-lifecycle-test-spec-2026-10-10.md). Las precisiones de medición, recuperación y orden de este documento prevalecen. El [estado nuevo](macos-lifecycle-remediation-status-2026-10-11.json) comienza con todas sus tareas pendientes; el estado anterior conserva la historia.

## Lo que demuestran los resultados

Las rutas de evidencia de esta sección parten de `local-analysis/lifecycle/20261010T173942/`. Las horas son del 10 de octubre en America/New_York.

| Momento y evidencia | Resultado comprobado | Límite de la conclusión |
| --- | --- | --- |
| `physical/start-stop-stress.stderr` | El primer intento leyó 48 kHz justo después de solicitar 44,1 kHz; hubo cero arranques | El test asumía un cambio síncrono; ese fallo no demuestra un fallo USB |
| 17:49–17:50, `physical-retry1/start-stop-stress.json` | 201 llamadas exitosas a AudioDeviceStart y 201 a AudioDeviceStop; 889 callbacks IOProc en total; 100 ciclos por frecuencia y una reapertura tras 11 s | No son contadores internos de StartIO/StopIO del HAL. No se exige progreso en cada ciclo, ni se mide señal física o calidad |
| 17:51:33, `unload-reload/cycle-1/reloaded/summary.txt` | Enumeración PASS; salud FAIL; coreaudiod 97,5 % según el muestreo existente | Una lectura puntual no caracteriza duración ni distingue pico de arranque y bucle persistente |
| 17:52:38, `unload-reload-retry/cycle-1/result.txt` | Una retirada y recarga completa pasó | No equivale a completar las tres repeticiones pedidas |
| 17:52:52, `unload-reload-retry/cycle-2/unloaded/summary.txt` | Plugin ausente según carpeta/proceso; enumeración PASS; coreaudiod 89,5 % y salud FAIL | El fallo también aparece con el proceso OpenA8DJ ausente; no exonera efectos anteriores del plugin |
| 17:54:03, `unload-reload-final/cycle-1/unloaded/summary.txt` | Salud FAIL; coreaudiod 110,4 %; enumeración registrada como FAIL con código 0 | El clasificador pierde el código real. No distingue aquí error de enumeración y timeout |
| 17:55:17, `unload-reload-final/recovery/summary.txt` | La recuperación también falla; coreaudiod 131,7 %; idle global 0,18 % | Restituir una carpeta y relanzar un servicio no bastó para recuperar la salud |
| 17:55:48, `unload-reload-final/coreaudiod.sample` | Hay pilas de notificación y reconciliación de objetos en Core Audio, con esperas Mach/mutex | La muestra es posterior a una recarga, no simultánea al estado sin plugin de las 17:54. Muestreo de pilas no prueba por sí solo consumo activo ni quién origina eventos. La ausencia de símbolos OpenA8DJ en otro proceso no lo exonera |
| 17:58, `recovery-to-baseline/manifest.txt` | Se registra el hash del bundle base restaurado | Falta una validación completa de recuperación anterior al reinicio |
| 18:02, `shutdown-stall-report.txt` e historial de arranques recogido | Reinicio observado; el informe muestrea un shutdown stall de 1 s tras 2 s de espera | No prueba kernel panic, reinicio automático, quién solicitó el apagado ni causalidad del driver |
| 18:14:24, `recovery-final-guard/summary.txt` | Enumeración y salud PASS con el bundle base después del arranque nuevo | No demuestra recuperación sin reiniciar el OS |
| 18:15–18:16, `traktor-launch-guard/` y `after-traktor-baseline-close/` | Traktor abrió y cerró con el base; enumeración PASS; salud de procesos de audio PASS al cerrar | No se confirmó Audio 8 seleccionado y transportando audio en Traktor. El guard global falla por idle 4,47 %, aunque procesos de audio suman 0,4 % |

El registro CTest `ctest-debug.txt` muestra 74 pruebas aprobadas de 90 y 16 fallidas. El inventario actual separa 74 offline y 16 dependientes de evidencia local. Esa clasificación no convierte los 16 fallos en aprobados ni convierte los contratos de fuentes en pruebas de concurrencia del HAL. Revisar individualmente cuáles bloquean esta versión.

Identidades históricas:

- Base restaurado: `4af4b1207f81846208fd2fcc0b8f5a600c2e11346d523ee5fd8e4f55700b9f66`.
- Candidato rechazado: `4b4cbba820373fc5da1be266d158bf0beda7a43dd62cf1b9a5f1b319173878b9`.
- El archivo actual `build/hal-build-config.txt` declara versión 0.3.135: no sirve como manifiesto del candidato. Registrar flags efectivos y hash en cada build nuevo.
- El Makefile actual solicita preapertura al inicializar y gracia de 10 s. Con ese perfil, el último Stop conserva USB/EP1/IPC. No deducir el perfil instalado de los defaults internos del archivo C, que son distintos.

## Defectos y dudas que guían el trabajo

| Prioridad | Hallazgo en el código actual | Acción |
| --- | --- | --- |
| P0 | `audio-stack-guard:enumeration_check` toma `$?` después del `if`; se pierde el error | R01. Reproducido el 11 de octubre sin hardware usando la función extraída: salida inyectada 7 o 124 produce `FAIL, return_code=0`; la función sí sale con 1 |
| P0 | Instalador y prueba de unload solo liberan el lock en EXIT; rollback no cubre todas las salidas. Recuperaciones contienen `|| true` y no exigen hash y salud recuperados | R02–R03. Journal persistente y recuperación comprobada, incluyendo interrupción y fallo de la propia recuperación |
| P0 | `_ipcClientsMutex` sigue retenido durante envíos; el timeout de 50 ms se aplica por escritura y no limita toda una operación. Envíos de respuestas no comparten necesariamente un único escritor | R05–R06. Propietario por conexión, escritor no bloqueante, colas y plazos totales |
| P0 | Stop espera playback hasta 250 ms y continúa sin resultado de drenaje; Close espera EP1 con `dispatch_sync`; EP1 espera una completion indefinidamente | R07–R08. Generaciones, tokens y cierre que devuelve error conservando recursos aún vivos |
| P0 | Preopen, warm-open y grace usan pthread detached; un destructor que llama Close no demuestra que esos hilos hayan terminado | R11 después del drenaje. Descartar una simple sustitución de `usleep` por `dispatch_after` |
| P1 | Start/Stop/Remove mantienen `gIOMutex` durante llamadas al motor; wrappers mantienen `gEngineMutex` durante start/close | R07–R09. Sacar esperas de esos locks y proteger publicación/retirada de sesiones |
| P1 | La configuración publica frecuencia/reloj antes de comprobar USB y descarta el resultado de `OpenA8DJUSBSetSampleRate` | R09. Commit transaccional y error explícito |
| P1 | Los callbacks de notificación de frecuencia y buffer empiezan con `return` | R09. No afirmar que esas llamadas concretas causan hoy una tormenta; tampoco habilitarlas sin prueba de reentrada |
| P1 | IOUSBHost se construye con `interestHandler:nil`; no se demuestra recuperación de desconexión/suspensión | R10. Invalidación y recuperación acotadas |
| P1 | El test físico acepta callbacks positivos solo en el total, ignora el resultado de DestroyIOProc y puede salir tras cambiar buffer sin restaurarlo | R12. Métricas por ciclo y cleanup en todos los caminos |
| P1 | El harness usa `assert` con operaciones y espera al escritor antes de solicitar stop en el camino aprobado de contrapresión | R04–R05. Checks activos en Release y barreras que fuercen la carrera real |

Las referencias fuertes Objective-C bajo ARC pueden conservar un objeto después de soltar un mutex; eso no garantiza que sus pipes o buffers no se cierren concurrentemente ni mantiene cargado el código del módulo. El plan debe probar esas tres vidas por separado.

Hipótesis que siguen abiertas: reconciliación del host disparada por sustituciones rápidas; estado incoherente publicado por OpenA8DJ; interacción con otros dispositivos o clientes de audio; carga general del sistema; origen independiente del apagado. La comparación de R13 sirve para distinguirlas. Un fallo con el plugin ausente después de haberlo usado no es un control limpio sin OpenA8DJ.

## Reglas de ejecución y presupuestos

Trabajar en la rama actual preservando los cambios existentes. Completar una tarjeta por vez. Cada tarjeta produce código, pruebas, comando exacto, resultado y artefactos; actualizar su estado solo después de comprobarlos. `PASS`, `FAIL`, `BLOCKED`, `NOT_RUN` y `ENVIRONMENT_BUSY` son resultados distintos. Una dependencia pendiente impide ejecutar la etapa física, pero no impide desarrollar pruebas offline independientes.

R00–R12 son offline: filesystem temporal, sockets privados y backend USB falso. Compilar no debe instalar ni preabrir el dispositivo. R13–R15 son la campaña física posterior. Este encargo del 11 de octubre produce el plan; no ejecuta esa campaña.

Conservar los límites de producción de la especificación anterior: Start 3 s totales, Stop 1 s con p95 normal ≤500 ms, Close 2 s; IPC cliente/servidor 250/500 ms en prueba nativa; 16 conexiones, 128 frames y 128 KiB por conexión, 2 s sin progreso; EP1 32 órdenes pendientes y 2 s por orden subordinados al deadline padre; un único contexto no drenado. Son objetivos que deben verificarse, no garantías medidas del candidato anterior.

Precisar la política del supervisor en un solo `tests/macos/lifecycle_test_policy.json` NUEVO:

| Medida | Regla propuesta que deben aplicar tests y herramientas |
| --- | --- |
| Identidad temporal | Timestamp civil con zona, reloj monotónico y boot UUID en cada evento; un boot nuevo interrumpe la ejecución anterior |
| CPU por proceso | Diferencia de tiempo CPU acumulado durante intervalos de 1 s; 100 % equivale a un núcleo. No mezclar esa escala con idle global |
| Preflight | Tres ventanas de 10 s con idle global medio ≥20 %, coreaudiod y host OpenA8DJ cada uno ≤25 % medio, suma de procesos vigilados sin duplicados ≤120 %, y enumeración correcta al final de cada ventana |
| Entorno ocupado antes de mutar | `ENVIRONMENT_BUSY`; cero modificaciones del HAL. Registrar los procesos que explican la carga; no matar aplicaciones ajenas |
| Tras carga/retirada | Deadline de estabilización 20 s: primeros 5 s de asentamiento y tres ventanas consecutivas de 5 s saludables con enumeración. La observación continúa durante los probes; no sumar esperas ocultas |
| Enumeración | Proceso hijo con deadline 5 s, stdout/stderr, duración, exit real y señal. Falta de ejecutable es error de infraestructura, nunca éxito |
| Alarma durante la prueba | Timeout de enumeración, nuevo boot, crash de host inesperado o >100 % de CPU de coreaudiod/host durante 10 s continuos: fallo de campaña, captura acotada y detención de nuevas mutaciones |
| Carga global durante la prueba | Conserva la observación como inconclusa por entorno si solo falla idle; sigue bloqueando promoción. No ocultar a la vez un fallo real de enumeración/driver |
| Transacción | 90 s para el camino normal, incluyendo preflight y validación; rollback independiente con 60 s máximos. Supervisor externo termina solo sus hijos atascados y conserva journal |
| Tras fallo | Hasta 60 s de diagnóstico sin reiniciar más servicios; no transforma el fallo original en PASS |

No subir límites ni ampliar el número de reinicios para obtener verde. Si un presupuesto resulta inviable, conservar el fallo, explicar la medición y cambiar política y tests en una revisión separada. Sleep/wake planificado se registra explícitamente; el reloj de observación de ese caso debe contabilizar correctamente suspensión sin aplicar por accidente un timeout de una sesión normal.

## Tarjetas de implementación

Los nombres de prueba `lifecycle_Rxx_*` de abajo son NUEVOS y se deben registrar en CMake al implementar su tarjeta. No son comandos ya disponibles. Para una tarjeta offline implementada, configurar Debug y Release en directorios separados, construir su target y ejecutar:

```sh
ctest --test-dir build/lifecycle-debug -R '^lifecycle_R01_' --output-on-failure --timeout 15
ctest --test-dir build/lifecycle-release -R '^lifecycle_R01_' --output-on-failure --timeout 15
```

Cambiar R01 por la tarjeta correspondiente; los tests de carga repetida R11 tienen supervisor propio de 60 s y límite CTest de 90 s. Registrar explícitamente excepciones por duración. Ningún test físico queda en el CTest por defecto. No poner una llamada con efectos dentro de `assert`.

### R00 Congelar evidencia y hacer trazable cada candidato

**Entrada:** árbol actual, informes del 10 de octubre. **Archivos:** manifiestos nuevos bajo `local-analysis/lifecycle/<run-id>/`, `tools/test_inventory.json`, estado nuevo. **Reutiliza:** L00–L02.

1. Guardar commit, diff de tracked y listado/hash de untracked que participan en el build. No usar únicamente HEAD con un árbol modificado.
2. Construir una tabla de cronología como la anterior con enlaces a los archivos originales. Preservar el informe de shutdown y las muestras; no reescribir evidencia histórica.
3. Separar `bundle_on_disk`, `host_pid_and_birth`, `module_loaded`, `device_enumerated`, `stream_progress`, `host_healthy`. Una carpeta existente solo prueba el primero.
4. Auditar los 16 gates fallidos: causa literal, artefacto esperado, relevancia para lifecycle/seguridad/audio y dependencia que aportará la evidencia. No desactivarlos para obtener 100 %.
5. Definir el manifiesto nuevo: fuente completa, compilador/SDK, flags reales, Info.plist, firma y hash del bundle final firmado, dSYM/UUID, configuración de prueba, versión/hash de herramientas y boot UUID. El informe debe validar identidades coincidentes.

**Pruebas:** `lifecycle_R00_manifest_identity` rechaza fuente, flags o binario distintos; `lifecycle_R00_evidence_inventory` detecta artefactos ausentes y el manifiesto antiguo 0.3.135.

**Salida aceptable:** ninguna prueba se atribuye a un binario por una suposición. Los 16 fallos quedan clasificados individualmente. No afirmar estado actual del laptop usando solamente datos del día anterior.

### R01 Reparar el guard y su supervisor

**Depende de:** R00. **Archivos:** `scripts/audio-stack-guard`, `scripts/audio-stack-health`, NUEVOS módulos de medición y `tests/macos/test_audio_stack_guard.py`.

1. Extraer evaluación a funciones importables/probables sin ejecutar acciones al cargarlas. Capturar el código en la rama `else` o inmediatamente con `rc=0; comando || rc=$?`; nunca después de `if` ni después de `! comando`.
2. Eliminar el éxito cuando falta `audio-list`; registrar `INFRA_ERROR`. Conservar exit, señal, timeout y errores de permisos como campos distintos.
3. Sustituir una lectura aislada de `ps pcpu` y el `top` anterior por muestreo alineado con timestamps. Detectar desaparición y reutilización de PID por PID+hora de nacimiento+boot UUID; no sumar la misma identidad dos veces.
4. Aplicar la política de ventanas anterior; devolver JSON estructurado además de texto compatible. Obtener una sola instantánea de procesos por muestra. Un fallo de medición es `INFRA_ERROR`, no cero CPU.
5. Añadir watchdog al conjunto de medición y a cada hijo. Conservar evidencia parcial aun si `top`, enumerador o consulta de procesos se atasca. Escalar TERM/KILL únicamente a hijos propios, con identidad verificada.
6. Hacer el guard observador. Las opciones antiguas `--recover`, `--reset` y `--force-unload-opena8dj` deben delegar en el administrador de R03 o fallar antes de mutar hasta que esté listo. Retirar reinicios en cascada.

**Pruebas:** `lifecycle_R01_exit_codes` para 0, 7, 124, señal y binario ausente; `lifecycle_R01_health_windows` para pico corto, CPU alta persistente y carga ajena; `lifecycle_R01_probe_timeout` mata solo el hijo propio; `lifecycle_R01_pid_reuse` no mezcla procesos; `lifecycle_R01_no_mutation` comprueba cero reinicios y escrituras reales.

**Salida aceptable:** timeout se clasifica como timeout con 124, fallo ordinario conserva 7, ningún caso desconocido es PASS. Un guard posterior sano no borra un fallo anterior de la campaña.

### R02 Coordinar todos los actores con un lock real

**Depende de:** R01. **Archivos:** `scripts/hardware-lock-lib.sh`, todos sus llamadores encontrados con `rg`, helpers/Control Center y NUEVO supervisor de lock. **Reutiliza:** parte de L15–L16.

1. Usar un supervisor que mantenga un lock de fichero del sistema durante la operación y sus hijos; en macOS puede implementarse con `fcntl.flock` en Python estándar. Los hijos que pueden mutar heredan el descriptor bloqueado explícitamente y lo conservan mientras trabajen: matar al supervisor no puede dejar a un hijo mutando sin lock. Mantener estable el inode del fichero de lock; no borrarlo para desbloquear.
2. Conservar metadatos legibles `owner` para herramientas existentes, agregando token de operación, PID, hora de nacimiento y boot UUID. El fichero de metadatos no sustituye el lock efectivo.
3. Hacer que los puntos de entrada se ejecuten bajo el supervisor. La librería shell verifica la herencia real del supervisor; una variable `OPENA8DJ_HARDWARE_LOCK_HELD=1` y un PID vivo ajeno no bastan.
4. Migrar conjuntamente los escritores del lock: ninguna herramienta antigua debe seguir borrando el directorio de un dueño nuevo. Limpiar solo metadatos del token propio y después de reconciliar un journal pendiente.
5. Definir lease de la operación Installer para hooks separados, validando PID y nacimiento de Installer. Si no se puede comprobar, rechazar el camino PKG antes de tocar archivos; no eludir el lock.

**Pruebas:** `lifecycle_R02_lock_contention` con dos procesos; `lifecycle_R02_fake_inheritance`; `lifecycle_R02_pid_boot_reuse`; `lifecycle_R02_old_owner_cleanup`; `lifecycle_R02_installer_lease`; `lifecycle_R02_supervisor_dies_with_child`. Al morir el supervisor, el segundo actor sigue excluido mientras el hijo pueda mutar; una vez retirado ese hijo, la siguiente operación primero exige reconciliar la transacción incompleta.

**Salida aceptable:** dos actores nunca entran en mutación simultánea y un cleanup viejo nunca retira la propiedad actual. Cero comandos privilegiados en estos tests.

### R03 Sustituir los scripts de instalación por una transacción recuperable

**Depende de:** R02. **Archivos:** `scripts/install-hal-lifecycle-transaction`, `scripts/test-hal-unload-reload-lifecycle`, NUEVOS `scripts/opena8dj-lifecycle-admin`, `scripts/lib/opena8dj_lifecycle_admin.py`, tests; `Makefile:install-hal` y hooks de paquete. **Reutiliza:** L15–L16, T74–T88.

1. Implementar una única máquina de transacción en Python estándar, con backend real y falso explícitos. Esto concreta la implementación propuesta antes como librería shell; los entrypoints existentes pasan a wrappers. No duplicar rollback en cada script.
2. Journal versionado y durable por temp+fsync+rename, con intento escrito antes de cada mutación. Estados: preflight, backup, staged, oldHostStopped, replaced, validated, committed; rollback, rollbackValidated, FAILED_RECOVERY y metadataPending cuando corresponda. Validar esquema y rutas al reanudar: un journal corrupto bloquea mutaciones y conserva backups, no se trata como instalación nueva.
3. Backup completo de cada componente afectado y de su ausencia/presencia original; verificar manifiesto y firma. Preparar y firmar el candidato fuera del HAL activo. Instalar exactamente ese bundle firmado; no firmar otro contenido ya activo para después comparar un hash anterior.
4. Identificar el host viejo por PID/nacimiento y por evidencia de que aloja exclusivamente OpenA8DJ. Carpeta movida no significa módulo descargado. No matar por búsqueda textual `ps | grep`, que también puede coincidir con herramientas de observación.
5. Capturar EXIT/error/INT/TERM en el supervisor y finalizar el journal. Tras SIGKILL o boot nuevo, la siguiente invocación `resume-transaction` inspecciona contenido real y reconcilia antes de continuar. Recuperación tras un corte no es instantánea por definición.
6. Rollback restaura el respaldo base validado, no reinstala automáticamente el candidato que acaba de fallar. Si la prueba era retirada de un baseline sano, su respaldo es el destino. Verificar archivos, firma, identidad cargada y salud; mantener fallida la instalación original aunque el rollback pase.
7. Mantener `install`, `uninstall`, `status`, `recover`, `resume-transaction` y `--dry-run`. Hasta integrar pruebas, los wrappers fallan antes de mutar en lugar de caer al script antiguo.
8. Aplicar el mismo protocolo a herramientas, LaunchAgent y hooks PKG si se sustituyen. Registrar recibos antes/después; restaurar archivos con recibo desalineado es `metadataPending`, no éxito completo. Sin borrar recibos ni lanzar Installer recursivo desde hooks.

**Pruebas:** `lifecycle_R03_fault_every_phase` inyecta fallo antes y después de cada operación; `lifecycle_R03_signals_resume` INT/TERM/KILL; `lifecycle_R03_backup_integrity`; `lifecycle_R03_failed_rollback`; `lifecycle_R03_invalid_paths` symlinks, raíz, staging dentro del activo; `lifecycle_R03_loaded_identity`; `lifecycle_R03_pkg_receipt`; `lifecycle_R03_restart_budget` prohíbe cascadas y loops de reinicio.

**Salida aceptable:** cada corte deja baseline íntegro o journal reconciliable; un rollback no validado nunca imprime PASS. Para recuperar hay como máximo un intento administrado, dentro de su deadline, no un bucle que vuelva a reiniciar Core Audio.

### R04 Hacer que las pruebas ejecuten producción de forma aislada

**Depende de:** R00; ejecutar tras R03 para seguir este orden. **Archivos:** CMake, `tools/audit_macos_lifecycle.m`, su runner, NUEVOS `tests/macos/`, `OpenA8DJUSBPlatform.h/.m` y eventos lifecycle. **Reutiliza:** L03–L04, T06–T09.

1. Compilar las unidades reales HAL/USB contra un adaptador de plataforma inyectable. El falso cubre device/interface/pipe, submit/abort/destroy y completions; falla si se intenta acceder al USB, socket vivo o audio del sistema.
2. Inyectar reloj monotónico y scheduler; controlar carreras con barreras. Prohibir usar esperas arbitrarias para hacer aparecer una intercalación.
3. Sustituir asserts por CHECK siempre activo, retirar side effects de asserts, cerrar todos los fds de fixtures y comprobar los contadores de recursos al terminar. El destructor del test tampoco puede acceder al sistema real.
4. Dividir las tres regresiones existentes en casos verificables. No etiquetar como concurrencia el caso que espera al escritor antes de arrancar stop. El adaptador debe observar el cleanup real, no probar una copia del algoritmo.
5. Crear ring acotado de 256 eventos y snapshot size/version: estado, motivo, engine/stream generation, operationID, pendientes, lectores y duración. No escribir payloads/audio ni logs desde cada callback.
6. Registrar builds Debug/Release y ASan/UBSan; ejecutar TSan en una configuración separada si el toolchain lo soporta. Una herramienta no soportada queda indicada y no se convierte en PASS.

**Pruebas:** `lifecycle_R04_offline_isolation`; `lifecycle_R04_release_checks`; `lifecycle_R04_fixture_cleanup`; `lifecycle_R04_event_ring`; `lifecycle_R04_snapshot_concurrency`.

**Salida aceptable:** se puede probar con USB desconectado y sin cargar el bundle del sistema; los checks se mantienen en Release y el runner no sobrescribe evidencia de una ejecución anterior.

### R05 Terminar la propiedad y el cierre no bloqueante de IPC

**Depende de:** R04. **Archivos:** NUEVOS `OpenA8DJIPCConnection.h/.m`, `OpenA8DJIPCServer.h/.m`, integración de `OpenA8DJUSB.m`. **Reutiliza:** L05–L06, T10–T23.

1. Hacer primero el objeto conexión: ID monotónico, fd, Open/Closing/Closed y propietario único. El cleanup recibe la identidad, no busca solo el entero del fd. Admitir 16; rechazar 17 antes de cualquier envío.
2. Introducir sockets no bloqueantes, sources de lectura/escritura, parser incremental y un solo escritor que serialice TODOS los frames: hello, control, stats, ledger y MIDI. Conservar el protocolo v1.
3. Reservar capacidad antes de encolar; aplicar límites de frames, bytes y plazo total sin progreso. Manejar escritura parcial, EINTR/EAGAIN y SIGPIPE. Un peer que lee un byte ocasional no puede extender indefinidamente un cierre.
4. Sacar I/O del mutex del registro y del productor EP1/audio. Un cliente lento se desconecta sin detener a uno sano. Cerrar fd únicamente cuando ningún source/worker pueda usarlo.
5. Detener aceptación antes de retirar clientes. Resolver accept simultáneo con stop. Guardar identidad de la entrada del socket y coordinación del dueño para que un servidor viejo no borre el pathname del nuevo; no basta `_ipcListenFd >= 0`.

**Pruebas:** `lifecycle_R05_fd_aba` reutiliza el fd como una NUEVA conexión y entrega cleanup viejo; `lifecycle_R05_accept_stop`; `lifecycle_R05_capacity17`; `lifecycle_R05_slow_peers16` y un consumidor sano; `lifecycle_R05_partial_frames`; `lifecycle_R05_writer_serialization`; `lifecycle_R05_socket_owner`; `lifecycle_R05_bounded_shutdown` fuerza escritor y stop solapados, sin drenar el peer desde el test.

**Salida aceptable:** exactamente un close por conexión; ningún frame intercalado; cero callbacks tardíos al declarar cerrado; deadlines de IPC cumplidos con 16 peers lentos, no solo con uno.

### R06 Llevar esas garantías al daemon MIDI

**Depende de:** R05. **Archivos:** `src/tools/opena8dj-midid.m`, conexión compartida y targets del daemon. **Reutiliza:** L07, T24–T29.

1. Callback MIDI copia a cola preasignada; transporte envía fuera del callback. No mantener el mutex del socket durante I/O.
2. Usar el mismo ownership, límites y escritor de R05; overflow invalida la conexión y cuenta el error, no corta mensajes arbitrariamente.
3. Reconectar con backoff 100/250/500/1.000/2.000 ms; no duplicar endpoints. Resetear backoff solo tras conexión estable.
4. Señales SIGTERM/SIGINT mediante source segura; disponer origen, destino, cliente MIDI, IOProc opcional y conexiones. Mantener keepalive opcional desactivado por defecto.

**Pruebas:** `lifecycle_R06_server_dies_during_send`, `lifecycle_R06_slow_server`, `lifecycle_R06_reconnect_endpoints`, `lifecycle_R06_sigterm_cleanup`, `lifecycle_R06_callback_budget`.

**Salida aceptable:** servidor muerto no provoca SIGPIPE fatal, callback no bloquea y SIGTERM no deja endpoints ni workers vivos.

### R07 Serializar apertura y control con resultados y deadlines

**Depende de:** R04–R06. **Archivos:** NUEVO coordinador lifecycle; `OpenA8DJUSB.h/.m`, adaptador real/falso. **Reutiliza:** L08–L10, T30–T45.

1. Implementar los estados D1 y separar presencia, disponibilidad, demanda y shutdown. Resultados explícitos OK/NoDevice/Busy/Timeout/Aborted/TransportError/ShuttingDown.
2. Cola de lifecycle decide; cola de recursos ejecuta llamadas SDK; cada solicitud tiene operationID y deadline absoluto. No esperar a USB reteniendo `gEngineMutex` ni `gIOMutex`. Un SDK colgado no debe impedir snapshots o respuestas de timeout.
3. Registrar adquisiciones por etapa de open y devolverlas una vez en orden inverso. Listener, pipes obligatorios y negociación necesaria deben dar error si faltan. Una apertura tardía ya invalidada limpia sus propios recursos y no se publica.
4. Convertir EP1 a submit/completion, con buffer y token propios. Quitar `DISPATCH_TIME_FOREVER` y espera ilimitada de `_pendingActive`. Cancelar impide reponer lecturas y despierta esperas de control con un resultado.
5. Si el protocolo solo correlaciona por opcode, timeout invalida el canal antes de repetir el mismo opcode. Respuesta vieja no confirma una orden nueva.
6. Si una llamada SDK o completion no termina, conservar un contexto no drenado y devolver error; no iniciar otro motor sobre los mismos recursos. Confirmar semántica de abort y callbacks en headers del SDK usado y registrar esa decisión.

**Pruebas:** `lifecycle_R07_open_stage_failures` parametrizada para todas las adquisiciones; `lifecycle_R07_late_open`; `lifecycle_R07_sdk_never_returns`; `lifecycle_R07_ep1_no_completion`; `lifecycle_R07_late_opcode`; `lifecycle_R07_reentrant_snapshot`.

**Salida aceptable:** cleanup idempotente; llamada pública responde dentro del límite sin inventar éxito; snapshot accesible mientras una operación falsa se atasca. Todavía no permitir unload del contexto retenido.

### R08 Cancelar y drenar sesiones antes de reutilizar buffers

**Depende de:** R07. **Archivos:** start/stop, submit/completion de captura/playback, pools, bridge preparado. **Reutiliza:** L11, T46–T53.

1. Session context por generación con tokens únicos de captura, playback y control; reservar antes de submit, revertir la reserva si submit falla y soportar completion inmediata.
2. Publicar Streaming solo tras aceptar los envíos iniciales obligatorios. El callback HAL adquiere un lector breve sin espera; ninguna última liberación/destrucción sucede desde audio.
3. Stop invalida generación y despublica, bloquea submissions nuevos, solicita abort y espera confirmaciones asíncronas. Registrar cada resultado de abort. No sostener locks que esas confirmaciones necesitan.
4. Una completion vieja termina SU token sin tocar contadores/buffers de la nueva sesión ni reencolar. Cancelar timers de reintento.
5. No resetear `inFlight=0` para olvidar trabajo vivo. Retirar pool solo con pendientes, callbacks ejecutándose, lectores y tareas diferidas a cero; confirmar en otra vuelta de la cola tras terminar el último handler.
6. Timeout deja Faulted y retiene recursos necesarios; una completion posterior puede terminar cleanup, pero no revivir streaming sin demanda válida. Verificar también compilación y cancelación del path preparado opcional, manteniendo su default.

**Pruebas:** `lifecycle_R08_submit_inline_completion`, `lifecycle_R08_abort_failure`, `lifecycle_R08_capture_and_playback_drain`, `lifecycle_R08_late_old_session`, `lifecycle_R08_missing_completion`, `lifecycle_R08_audio_reader_retirement`, `lifecycle_R08_prepared_runtime`.

**Salida aceptable:** cero uso después de liberar y cero doble completion; sesión nueva nunca comparte pool con trabajo viejo. Un timeout no se anuncia como Stop/Close satisfactorio.

### R09 Hacer coherentes clientes, configuración y notificaciones HAL

**Depende de:** R08. **Archivos:** `OpenA8DJHAL.c`, puente lifecycle y host falso. **Reutiliza:** L12, T54–T62.

1. Conservar registro por clientID y startDepth, añadiendo reserva de Start pendiente. Start reserva bajo lock, espera fuera y al completar valida que el cliente y la operación siguen vivos; Remove concurrente cancela esa reserva.
2. Total derivado y comprobable: iniciar transporte solo en 0→1; parar en 1→0; Stop extra y Remove repetido son inocuos. Overflow/capacidad falla sin alterar otro cliente. Balancear metadatos CF retenidos.
3. Configuración por ID: validar → preparar transporte → confirmar → publicar frecuencia/buffer/reloj. Fallo USB devuelve error; conservar configuración anterior si sigue válida, o publicar indisponible. Abort solo cancela su operación.
4. Snapshots de propiedades con disciplina de sincronización uniforme. Notificar fuera de locks y solo transiciones efectivas. Verificar qué cambios notificará ya el host según contrato SDK; no quitar simplemente los `return` existentes.
5. Registrar por selector objetos/cantidad de eventos y solicitudes de configuración, sin logging en cada callback de audio. El host falso debe reentrar en getters desde PropertiesChanged.

**Pruebas:** `lifecycle_R09_remove_during_start`, `lifecycle_R09_two_clients`, `lifecycle_R09_unbalanced_stop`, `lifecycle_R09_rate_commit_failure`, `lifecycle_R09_stale_config_abort`, `lifecycle_R09_reentrant_properties`, `lifecycle_R09_noop_no_notifications`.

**Salida aceptable:** cliente eliminado no revive, fallo de USB no devuelve éxito de configuración, no hay deadlock ni bucle de notificaciones ante valores sin cambio.

### R10 Recuperar desconexión y suspensión sin acumular motores

**Depende de:** R09. **Archivos:** adaptador IOUSBHost/energía y coordinador. **Reutiliza:** L13, T63–T68.

1. Registrar eventos de interés y matching con identidad de dispositivo/generación; duplicados de device/interface invalidan una sola vez. Verificar contrato SDK para energía, sin inventar mensajes en interestHandler.
2. Prohibir reopen durante shutdown y mientras exista contexto no drenado. Tres reintentos 100/500/2.000 ms por evento válido; no rearmar presupuesto desde el propio timer.
3. Reabrir tras nueva conexión solo cuando se haya retirado la vieja; validar identidad y negociación antes de publicar sesión. Mantener controles/perfil confirmados.
4. Demanda restante vuelve a Streaming si recupera; sin demanda queda OpenIdle; presupuesto agotado queda Faulted sin spin. Verificar salud del control también al reutilizar OpenIdle.

**Pruebas:** `lifecycle_R10_duplicate_disconnect`, `lifecycle_R10_disconnect_during_open`, `lifecycle_R10_disconnect_during_stop`, `lifecycle_R10_retry_budget`, `lifecycle_R10_suspend_resume`, `lifecycle_R10_shutdown_forbids_reopen`.

**Salida aceptable:** una conexión exclusiva como máximo, reintentos acotados y configuración preservada; la simulación de sleep no sustituye R14 físico.

### R11 Retirar todo el trabajo y comprobar la descarga

**Depende de:** R10. **Archivos:** temporizadores/hilos de HAL, destructor USB, coordinador y NUEVO host hijo de pruebas. **Reutiliza:** L14, T69–T73.

1. Sustituir todos los pthread detached por tareas/timers registrados en el coordinador. Cancelar también preopen, warm-open, grace, keepalive, reintentos, accept y conexiones. Cancelar un timer requiere confirmar que su handler ya no puede ejecutarse.
2. Añadir PrepareForUnload(deadline) con resultado y listado de pendientes. Marcar shutdown y despublicar antes de retirar; invalidar gHost antes de permitir que desaparezca. Reutilizar la secuencia de drenaje, no agregar un cierre paralelo en el destructor.
3. Host hijo carga un bundle con backend falso y repite 100 ciclos de carga/descarga. Inyectar descarga durante cada fase y registrar cero código/callbacks ejecutados tras unload aprobado.
4. Con SDK/completion ausente, PrepareForUnload devuelve NoSafeUnload/Faulted, conserva módulo y recursos y NO llama dlclose. El padre termina exclusivamente ese hijo al deadline; clasificar como contención correcta, no descarga cooperativa exitosa.
5. Verificar por contrato y experimento cuál es el mecanismo real de Core Audio. No asumir que invocará el método privado ni que conservar un objeto conserva su dylib. Si no se puede demostrar una barrera de unload del host, soportar retirada administrada del proceso aislado y documentar que dlclose en fallo no está soportado.
6. No usar `exit`, `abort` ni auto-kill desde el plugin. R03 solo termina un host externo identificado y exclusivo; si es compartido o ambiguo, devuelve BLOCKED antes de matar.

**Pruebas:** `lifecycle_R11_unload100`, `lifecycle_R11_cancel_each_timer`, `lifecycle_R11_no_safe_unload`, `lifecycle_R11_no_host_after_shutdown`, `lifecycle_R11_real_host_contract` (contrato documentado; su confirmación física se realiza en R13).

**Salida aceptable:** no hay trabajo capaz de volver al módulo cuando se declara descargable; el camino irrecuperable está contenido y nunca se confunde con unload exitoso. Mantener el perfil de preapertura/control-MIDI salvo cambio justificado: cerrar todo inmediatamente en StopIO no es el arreglo por defecto.

### R12 Fortalecer el test de cliente y cerrar la puerta offline

**Depende de:** R01–R11. **Archivos:** `tools/audio-lifecycle-test.c`, CMake/Makefile, runner de campaña y manifiestos. **Reutiliza:** L17–L18, T89–T98.

1. Contadores atómicos por ciclo: inicio, primer callback, delta callbacks/frames, último callback, retorno Stop, actividad posterior, tiempos y OSStatus. Exigir progreso en CADA ciclo y tras idle/reopen. No tolerar dos callbacks sin explicar cuál estaba en curso; exigir cero entradas nuevas después de que Stop haya completado según contrato.
2. Añadir lectura de contadores físicos de submit/completion/generación desde snapshot del driver. Separar éxito de API Core Audio, progreso USB y calidad de señal.
3. Esperar configuración asíncrona de frecuencia Y buffer con deadline; validar valores realmente aplicados. Ruta única de cleanup para fallo de creación, Start, Stop, señal y restauración; comprobar DestroyIOProc. Estado original persistido antes de mutar para recuperación del padre tras crash.
4. Watchdog externo por operación y por hijo: un getter de Core Audio colgado no queda acotado simplemente por un bucle de 50 lecturas. No prometer matar un hilo dentro del test para recuperarlo.
5. Incorporar modo cliente que muere durante streaming y dos clientes en procesos distintos. Sus mocks se prueban ahora; la ejecución real corresponde a R14.
6. Construir Debug, Release y bundle candidato firmado; compilar MIDI y variantes opcionales afectadas. Ejecutar T01–T98 implementados y nuevos R00–R12; revisar sanitizers y clasificación de gates locales. No cargar el bundle de producción como smoke offline si preabre USB.
7. Registrar un manifiesto offline aprobado ligado al hash final. R03/R13 deben rechazar un candidato distinto o una aprobación incompleta. El runner físico produce dry-run, checks y rollback explícitos sin ejecutarlos en esta tarjeta.

**Pruebas:** `lifecycle_R12_zero_callbacks_one_cycle` debe fallar aunque haya callbacks en otros; `lifecycle_R12_cleanup_every_failure`; `lifecycle_R12_async_config`; `lifecycle_R12_hung_api_child`; `lifecycle_R12_stale_evidence`; `lifecycle_R12_physical_dry_run`; `lifecycle_R12_offline_gate`.

**Salida aceptable:** no hay PASS con ciclo vacío, cleanup fallido, test offline faltante, binario distinto o comprobación offline de seguridad pendiente. Los gates que requieren evidencia física nueva permanecen pendientes o fallidos hasta R13–R14: no se exige esa evidencia antes de poder producirla, pero sí se exige antes de promover en R15. Esta puerta habilita solamente la campaña supervisada con rollback. Los 74 tests anteriores por sí solos nunca abren la puerta física.

## Campaña física posterior

### R13 Distinguir fallo del host y fallo del candidato

**Depende de:** R12 aprobado y mecanismo de recuperación R03 verificado offline. **Reutiliza:** H01, H02, H09. No se ejecuta al redactar este plan.

1. Leer estado actual del laptop, aplicaciones, demanda de audio, boot UUID y journal. Verificar firma/hash base y backup. Preflight de 30 s sin alterar configuración; si está ocupado, conservar evidencia y posponer la mutación.
2. Capturar inventario de dispositivos/HAL y procesos de audio. Otros drivers, Bluetooth, Continuity, MIDI y clientes se mantienen constantes inicialmente; cualquier cambio posterior es una variable explícita de otra ejecución.
3. Hacer una prueba de referencia con el base: cierre normal de cliente, salud durante 60 s y recuperación de otra sesión. Esto no requiere reiniciar coreaudiod. Confirmar audio/control usados, no solo ventana abierta.
4. Separar tres experimentos: cliente Start/Stop; retirada administrada del host aislado; reinicio global de Core Audio. No llamar a los tres “unload”. El reinicio global es un caso explícito de instalación/fallback, nunca un requisito de cada cierre de Traktor.
5. Si la retirada aislada es demostrablemente soportada, hacer una sola secuencia baseline → ausente → baseline. Si necesita un restart global, declararlo en el manifiesto y tratarlo como experimento del host. Cada transición pasa estabilización antes de otra. Fallo detiene campaña y activa una recuperación administrada como máximo.
6. Solo tras una referencia sana, instalar candidato aprobado y repetir la misma secuencia una vez. Si ambas pasan, ejecutar tres ciclos separados por 60 s de observación estable; no encadenar reintentos después de un rojo. Registrar todas las mutaciones y PIDs esperados.
7. Si aparece CPU alto: guardar a la vez métricas, enumeración, eventos del driver y samples acotados de coreaudiod y host OpenA8DJ cuando exista. La muestra de otro proceso no basta. No repetir samples si el sistema está sobrecargado.
8. Si solo falla tras cambios de otros dispositivos/clientes, realizar comparación modificando una variable por ejecución. No desinstalar otros HAL, desactivar seguridad ni matar servicios en cascada para forzar una conclusión.

**Decisión causal:** si baseline también falla en el mismo experimento, investigar host/herramienta/entorno antes de promover. Si solo candidato falla, acotar por cambios pequeños en copias aisladas, con test que reproduzca la diferencia. Si falla incluso sin plugin tras haberlo usado, clasificar como estado residual/host no resuelto. No proclamar una causa a partir de correlación.

**Evidencia obligatoria:** `baseline_client`, `baseline_retirement`, `candidate_retirement`, cada transición con identidad/hash, métricas por segundo y decisión. Un boot nuevo es `RUN_INTERRUPTED_BY_REBOOT`: no resumir comprobaciones posteriores como recuperación sin reinicio.

### R14 Validar uso real de Traktor y de audio

**Depende de:** R13 completo. **Reutiliza:** H03–H08, H10–H12. Ejecutar las filas en este orden y detenerse ante fallo.

| Caso | Procedimiento | Aceptación y evidencia |
| --- | --- | --- |
| Cliente Core Audio | 100 ciclos a 44,1 y 100 a 48 kHz con buffer 512; luego 20 por cada buffer adicional anunciado entre 128/256/1024 | Delta positivo en cada ciclo, progreso USB, límites Start/Stop cumplidos, sin callbacks nuevos tras Stop, configuración restaurada |
| Varios clientes | Dos procesos; cerrar uno mientras el otro sigue; matar solo un hijo de prueba durante streaming; repetir 20 veces | El superviviente continúa; al desaparecer el último cliente demanda y transferencias de audio llegan a cero; no reiniciar Core Audio |
| Gracia y reapertura | Parar y reabrir a 1 s, justo antes y después de la gracia real del manifiesto y a 60 s; 10 por punto | Sin sesión vieja reencolando; política OpenIdle/control-MIDI respetada; progreso en cada nueva sesión |
| Traktor | Guardar preferencias; confirmar desde Audio Setup Audio 8 DJ seleccionado, frecuencia/buffer y rutas. Reproducir señal de prueba conocida a nivel seguro. Realizar 10 cierres normales tras 60 s y 3 tras 10 min | Salida medida y entrada/captura cuando corresponda; guardar colección y salir sin colgarse; salud 60 s después de cada cierre; próxima sesión funciona. Restaurar preferencias |
| IPC/MIDI | Consumidor sano, consumidores que no leen, retirada del servidor y reinicio del daemon; 10 repeticiones | Sin pérdida de framing/endpoints, cierre dentro de plazo, MIDI/control útiles después; audio no queda bloqueado |
| USB | Desconectar/reconectar en idle y streaming, 5 veces por estado | Estado visible correcto, sin segundo motor ni retry storm, recuperación ≤15 s tras reaparición; controles preservados |
| Sleep/wake | 3 ciclos idle y 3 streaming con evidencia antes/después y observación retomada al despertar | Sin reinicio del OS, sin host colgado, transporte recuperado y audio medido. Un falso evento offline no sustituye esta prueba |
| Calidad de audio | Reusar matriz y gates físicos del proyecto para entradas/salidas anunciadas, frecuencias y rutas; capturar WAV de una señal conocida antes/después | Comparar con baseline y límites ya definidos de canales, amplitud, continuidad, reloj y timecode aplicable. Silencio enviado por IOProc no acredita calidad |
| Soak | 2 h streaming a 48 kHz/512, 1 h a 44,1 kHz/512, 1 h con control/MIDI idle y reaperturas periódicas; cerrar y reabrir Traktor al final | Ningún crash/timeout/reinicio inesperado, ni crecimiento no acotado de fds/threads/objetos; recursos vuelven al estado estacionario definido |

Loopback, capturador o reconexión física deben estar realmente disponibles. Si no hay actuador o cableado para una fila, queda BLOCKED/NOT_RUN con motivo; no simular que se ejecutó ni marcar “todo probado”. Cuando sea necesaria una acción humana física, la herramienta debe describirla y esperar su finalización real.

No usar la RSS aislada para afirmar fuga: registrar objetos vivos, fds, hilos, pools y pendientes tras warmup. Las colas/cachés deben quedar dentro de sus límites y los tokens/lectores volver a cero donde corresponda. Un soak de cuatro horas no demuestra estabilidad durante semanas, pero sí permite aceptar esta campaña acotada.

### R15 Instalar el mismo artefacto validado y cerrar el informe

**Depende de:** R13–R14 aprobados y gates aplicables de seguridad/calidad sin pendientes. **Reutiliza:** decisión final de L19.

1. Resolver la lista de 16 gates locales de R00. Los de seguridad/runtime y las rutas de audio usadas deben tener evidencia actual del candidato. Una exclusión solo es posible para funcionalidad realmente no incluida, con justificación explícita; nunca transformar un fallo relevante en SKIP.
2. Congelar un único bundle firmado y manifiesto. Una recompilación o refirma que cambie su identidad exige repetir las comprobaciones afectadas; no instalar un build posterior no probado.
3. Instalar/confirmar ese artefacto por R03. Verificar por separado bytes en disco, firma, versión/identidad del módulo cargado y dispositivos enumerados. Si se redistribuye por PKG, probar también los hooks/recibos del paquete exacto antes de afirmar validación del PKG.
4. Smoke final: sesión con Audio 8 realmente seleccionado, progreso USB/señal, cierre normal, 60 s de salud y reapertura. No convertir otro restart de Core Audio en requisito del smoke.
5. Mantener backup y comando de rollback que referencia esa transacción exacta; restaurar preferencias del usuario. Informar qué versión/hash quedó activa y qué se probó, con enlaces y límites.

**Terminado significa:** código y pruebas offline aprobados, campaña física completa, candidato exacto instalado y cargado, recuperación sin reinicio validada en los escenarios reclamados, ninguna prueba requerida pendiente. Si una condición falla, el informe dice incompleto y el estado instalado se registra literalmente; no afirmar “todas las mejoras in place”.

## Cómo debe continuar un modelo pequeño

1. Leer este plan y `macos-lifecycle-remediation-status-2026-10-11.json`. La primera tarea es R00. No empezar instalando otro candidato.
2. Revisar cambios existentes de la tarjeta; no reemplazar trabajo ajeno ni dar por completo lo marcado parcial en el plan antiguo.
3. Añadir primero la prueba de su fallo observado. Para un defecto confirmado, documentar que falla antes de corregirlo; para infraestructura nueva, no inventar un resultado “antes”.
4. Implementar solo esa tarjeta, construir y ejecutar sus tests y los de los módulos que afectó. Registrar logs, flags y hashes. Conservar fallos y evidencia parcial.
5. Actualizar estado con `changed_files`, `commands`, `evidence`, `result` y razón de bloqueo si existe. DONE exige aceptación completa. Seguir con la siguiente dependencia satisfecha.
6. Antes de R13, comprobar una única puerta offline ligada al candidato. Durante físico, el primer fallo detiene nuevas mutaciones; rollback y diagnóstico son operaciones distintas de repetir la prueba.
7. Al finalizar, leer de nuevo el estado instalado y la evidencia. No deducir éxito porque un comando se lanzó o porque otro guard pasó después de reiniciar el Mac.
