# Especificación de pruebas de estabilidad macOS

Estado: **especificación por implementar**. Complementa el [plan de implementación](macos-lifecycle-implementation-plan-2026-10-10.md). Las pruebas T01–T98 y el runner de campaña H01–H12 son entregables NUEVOS; los resultados reales se guardan fuera de este documento.

## Reglas de los tests

Cada Txx implementa una función/caso con nombre estable. El test debe ejecutar el código de producción afectado, observándolo mediante un adaptador falso cuando interviene el sistema. No es suficiente copiar la lógica a un modelo y probar la copia.

El test sigue siempre esta secuencia:

1. Crear fixture limpia con reloj y scheduler deterministas, socketpair si procede y contadores de recursos.
2. Guardar estado inicial: objetos, fds, generaciones y pendientes.
3. Ejecutar la acción de la columna «Estímulo».
4. Colocar la carrera con barreras, entregar callbacks o avanzar el reloj según indique el caso.
5. Comprobar exactamente el resultado de la columna «Resultado obligatorio».
6. Cerrar fixture, drenar el scheduler y verificar que no queda trabajo inesperado. En casos de callback deliberadamente ausente, verificar la retención esperada y usar proceso hijo para su contención final.

No confiar en asserts que desaparecen con `NDEBUG`: usar una función `CHECK` que falle también en Release, con test ID, línea, esperado y observado. No poner llamadas con efectos laterales dentro de un assert compilable fuera. Cada proceso tiene watchdog externo; un timeout es FAIL, aunque no imprima nada.

Un fallo previsto por el test es distinto del resultado del test. Por ejemplo, si inyectamos `abort failed`, el test pasa solo cuando el driver devuelve error y conserva correctamente los buffers. No marcar una ejecución como sana porque «era esperable que se colgase».

## Constantes de partida y presupuestos

Son decisiones nuevas de aceptación; no mediciones históricas. Guardarlas en `tests/macos/lifecycle_test_policy.json` NUEVO y reflejar las de producción en una cabecera compartida con nombres y unidades claros. El falso puede inyectar plazos menores para pruebas, sin cambiar los defaults de producción.

| Operación o recurso | Límite inicial | Qué sucede al alcanzarlo |
| --- | --- | --- |
| Respuesta al inicio de audio | 3.000 ms totales | Error; no publicar streaming ni cliente iniciado |
| Stop de audio | Deadline 1.000 ms; p95 normal ≤500 ms | Error y Faulted si no drena; buffers retenidos |
| Cierre completo cooperativo | 2.000 ms totales | Resultado de cierre incompleto; no permitir unload inseguro |
| Cierre IPC de un cliente/servidor | 250/500 ms en prueba nativa | FAIL si no se retira sin ayuda del peer |
| Frame IPC parcial | 2.000 ms desde el primer byte | Desconectar esa conexión; no cerrar las sanas |
| Sin progreso de salida IPC pendiente | 2.000 ms | Desconectar al consumidor lento |
| Conexiones IPC | 16 | Rechazar la siguiente antes de usarla |
| Salida por conexión | 128 KiB y 128 frames | Desconectar, sin envío parcial de un frame nuevo |
| Trabajo por handler IPC | 16 frames o 64 KiB | Ceder la cola y continuar después |
| Órdenes EP1 esperando | 32 | Busy explícito, sin crecimiento de memoria |
| Orden de control aceptada | 2.000 ms, acotada además por la operación padre | Timeout invalida correlación de respuestas; no reusar sin recuperar |
| Clientes HAL registrados | 64 | Error de capacidad, sin sobrescribir otro cliente |
| Reintentos USB automáticos | 3, esperas 100/500/2.000 ms | Faulted estable; nada de retry cada milisegundo |
| Recuperación total al volver USB | 15 s desde el evento de reaparición | Error visible; parar intentos de esa generación |
| Contextos viejos no drenados | Máximo 1 | Prohibir nueva apertura hasta drenaje o recuperación del host |
| Ring de eventos | 256 entradas | Sobrescribir la más antigua y contar descartes |
| Enumeración Core Audio supervisada | 5 s | Guardar timeout y fallar el paso |
| Espera a terminar el host viejo | 10 s | Recuperación explícita o rollback; nunca seguir como si hubiese terminado |
| Estabilización tras carga/descarga | 20 s | Muestrear CPU y verificar enumeración otra vez |
| Transacción admin sin intervención humana | 90 s, excluye entrada de credenciales | Fallo y rollback supervisado; journal persistente |

Los plazos anidados comparten un deadline absoluto: tres órdenes de 2 s no pueden convertir StartIO de 3 s en 6 s. Usar tiempo monotónico; el reloj falso no duerme. En pruebas reales, registrar el tiempo del API y el de finalización de limpieza por separado.

## Comandos offline

Los siguientes comandos solo existen al terminar L17. El ejecutor debe implementar exactamente esta interfaz o actualizar plan, tests y estado juntos antes de usar una distinta.

```sh
python3 scripts/run-macos-lifecycle-offline --configuration Debug --run-dir local-analysis/lifecycle/debug
python3 scripts/run-macos-lifecycle-offline --configuration Release --run-dir local-analysis/lifecycle/release
python3 scripts/run-macos-lifecycle-offline --sanitizer address-undefined --run-dir local-analysis/lifecycle/asan
python3 scripts/run-macos-lifecycle-offline --sanitizer thread --run-dir local-analysis/lifecycle/tsan
python3 scripts/run-macos-lifecycle-offline --stress-cycles 1000 --seed 20261010 --run-dir local-analysis/lifecycle/stress
```

El runner debe rechazar un run-dir que ya contenga resultados completos; elegir otro nombre o crear un subdirectorio con run-id. No necesita sudo. No llama a `make install-hal`, ni carga el bundle instalado, ni cambia los dispositivos Core Audio.

Contrato del runner:

- Configura builds distintos dentro de `build/lifecycle/{debug,release,asan,tsan}`. Nunca reutiliza objetos compilados con otro sanitizer.
- Usa la configuración de sonido publicada para la compilación del transporte; corre las pruebas relevantes también con la configuración preparada opcional, sin activación real.
- Ejecuta CTest con labels `offline` y `macos_lifecycle`, haciendo visibles los Source Contracts que efectivamente comprueban este árbol. Comprueba número de tests encontrado: cero tests seleccionados es FAIL.
- Serializa los casos que comparten procesos de prueba. Las fixtures no comparten sockets ni paths.
- ASan/UBSan: detener al primer error, símbolos presentes y resultado no cero. TSan: carrera no explicada es fallo; una limitación del runtime se registra como BLOCKED, no como PASS ni como licencia para deshabilitar el test.
- No instrumenta ni inyecta bibliotecas en el host de audio instalado. Los sanitizers corren únicamente en ejecutables de prueba y bundle falso.
- Exit 0 solo si todo lo seleccionado pasa; 1 por fallo; 2 por uso inválido; 75 por recurso ocupado cuando se aplique. El estado JSON distingue FAIL de BLOCKED.

Antes de L17 usar directamente el target y CTest que se hayan creado en L03:

```sh
cmake -S . -B build/lifecycle/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/lifecycle/debug -j 3
ctest --test-dir build/lifecycle/debug -L macos_lifecycle --output-on-failure --timeout 15
```

Esos labels/targets también son NUEVOS. L03 registra cada caso con nombre `macos_lifecycle_Txx_<descripcion>`. Para una tarea concreta, `ctest -R '^macos_lifecycle_T1[0-4]_'` selecciona T10–T14. Comprobar siempre el inventario con `ctest -N` antes de atribuir un resultado a un grupo.

## T01 a T09 Infraestructura y diagnóstico

| ID | Estímulo | Resultado obligatorio |
| --- | --- | --- |
| T01 | Compilar en carpeta cuyo nombre no sea audio8djcpp | Raíz de informe idéntica a CMAKE_SOURCE_DIR real |
| T02 | Colocar árbol señuelo con contenido opuesto y path histórico disponible | El test nunca lee sus fuentes ni evidencias; modificar señuelo no altera resultado |
| T03 | Falta archivo requerido en la raíz configurada; fuente dirty después del commit | Error por archivo ausente; hash/diff revela el cambio, sin fallback a otro árbol |
| T04 | Suite offline en HOME y evidencia temporal vacíos | Pasa sin JSON/WAV locales; fixtures se generan dentro de la prueba |
| T05 | Gate local con evidencia ausente, vieja, de otro commit y válida | Rechaza las tres primeras; acepta solo la válida; promoción falsa cuando NOT_RUN |
| T06 | Fixture intenta el path real o una llamada USB/CoreAudio real | El adaptador lo intercepta y falla antes de cualquier acceso; suite normal no lo intenta |
| T07 | Insertar 300 eventos en ring de 256 | Conserva exactamente los 256 últimos en orden y contador 44 de sobrescrituras |
| T08 | Leer snapshots concurrentes durante 1.000 transiciones | Estado consistente, generaciones monotónicas y TSan limpio |
| T09 | Trace encendido/apagado y reloj avanzado hasta timeout | Mismo resultado funcional; ningún log ni allocation nueva desde callback de audio |

## T10 a T23 Servidor IPC

| ID | Estímulo | Resultado obligatorio |
| --- | --- | --- |
| T10 | Cancelar cliente; cerrar realmente; reutilizar fd con dup2; ejecutar remove viejo | Descriptor nuevo sigue abierto y contiene el mismo recurso |
| T11 | Stop, error de write y EOF llegan en los seis órdenes posibles | Un solo cierre final; registro vacío; nada de decrementar dos veces |
| T12 | Abrir 16 clientes y un 17.º | 16 funcionales; 17 rechazado; no hello/worker sobre el rechazado |
| T13 | Accept completa a la vez que se cancela listener | Conexión se rechaza o se registra y cancela; ninguna queda huérfana |
| T14 | Viejo servidor termina después de que otro bind haya creado socket nuevo en el path | Cleanup viejo no elimina el socket nuevo; test usa path temporal |
| T15 | Llenar buffer de peer que no lee y solicitar stop | Stop termina ≤500 ms sin drenar desde el test; fd cerrado una vez |
| T16 | Un cliente lento y otro que lee durante 1.000 broadcasts | El sano recibe frames válidos y ordenados; el lento se aísla por capacidad/plazo |
| T17 | Forzar writes de 1–7 bytes, EAGAIN y EINTR alternos | Byte stream final coincide exactamente con frames originales |
| T18 | Respuesta de controles y MIDI se encolan simultáneamente | No cabeceras/payloads intercalados; cada mensaje se decodifica completo |
| T19 | Enviar cabecera byte a byte y payload fragmentado | Un único mensaje despachado después del último byte |
| T20 | Magic/version incorrectos, longitud 4097, EOF parcial y frame sin completar en 2 s | Solo esa conexión se cierra; no lectura fuera de límites ni allocation ilimitada |
| T21 | Cancelar con sources de lectura/escritura vivos; cancel handlers llegan separados | Ningún close antes del último handler; exactamente uno después |
| T22 | Peer cierra mientras se escribe, disposición SIGPIPE por defecto en proceso hijo | Proceso no muere; resultado de conexión cerrada y cleanup correcto |
| T23 | Inundar hasta límites de bytes/frames y alternar cola vacía/no vacía 10.000 veces | Memoria acotada, handler cede, cancelación funciona; cero source sin retirar |

El test T10 no pasa solo porque el cierre devuelva error: verifica con `fcntl(F_GETFD)` y una lectura/escritura de prueba que el recurso nuevo sigue siendo el correcto. T15 usa barrera para demostrar que el buffer está lleno antes de parar. No usar el drenaje del peer como cleanup que enmascare el defecto original.

## T24 a T29 Daemon MIDI

| ID | Estímulo | Resultado obligatorio |
| --- | --- | --- |
| T24 | Callback MIDI con socket bloqueado y cola llena | Callback retorna sin I/O bloqueante; política de saturación explícita y contada |
| T25 | Servidor desaparece a mitad del envío | Sin SIGPIPE fatal ni double-close; estado desconectado |
| T26 | Servidor ausente 60 s simulados y después disponible | Reintentos siguen backoff; una conexión final; no loop intensivo |
| T27 | 100 desconexiones/reconexiones | Mismos dos endpoints, contadores de recursos al baseline tras estabilizar |
| T28 | SIGTERM/SIGINT con lectura pendiente | Hijo termina ≤2 s; sockets y endpoints falsos dispuestos exactamente una vez |
| T29 | Modo keepalive opcional activo y apagado por defecto | Default no inicia audio; modo explícito destruye IOProc al salir |

Usar un adaptador de CoreMIDI en el daemon de pruebas: no crear endpoints reales para estos tests offline.

## T30 a T45 Apertura y control USB

| ID | Estímulo | Resultado obligatorio |
| --- | --- | --- |
| T30 | Fallo tras cada etapa de adquisición antes de EP1 | Todos los recursos realmente adquiridos liberados; nunca depende de running=true |
| T31 | Fallo después de arrancar EP1, con cancelación que sí completa | Drenaje antes de destroy; interfaz/device se entregan en orden seguro |
| T32 | Listener IPC no puede hacer bind; pipe requerido devuelve nil; device-info falla | Apertura falla con etapa identificada; sin bridge listo ficticio |
| T33 | Abrir parcialmente, close dos veces y liberar motor | Propiedad liberada exactamente una vez, sin handles/fds vivos |
| T34 | Fallo de apertura seguido de apertura sana | Segundo intento funciona y no encuentra el dispositivo retenido por el primero |
| T35 | 50 órdenes concurrentes Start/Stop/Close sobre motor falso | Transiciones serializadas, resultados asociados al operationID correcto |
| T36 | Apertura SDK no responde y avanza deadline | API retorna Timeout ≤plazo; snapshot y audio siguen respondiendo |
| T37 | Apertura responde tarde cuando ya se pidió shutdown | No publica motor; limpia su resultado cuando sea seguro |
| T38 | Callback audio toma referencia mientras control la despublica | Buffer vivo hasta último lector; última destrucción fuera de audio |
| T39 | Cola USB bloqueada por una fixture mientras llega Stop | Ningún lock HAL mantenido esperando USB; timeout y diagnóstico disponibles |
| T40 | Lectura EP1 completa inmediatamente, tarde o con error | Una petición activa máximo; continuación/cancelación exactamente una vez |
| T41 | EP1 no entrega completion y se solicita close | Timeout, buffer retenido; no se anuncia Closed ni se destruye memoria usada |
| T42 | 33 órdenes de control; la primera no responde | Máximo 32 pendientes; la 33 recibe Busy; cancelación despierta resultados pendientes |
| T43 | Respuesta de comando X llega tras su timeout antes de un nuevo X | No completa el nuevo X; recuperación/drenaje requerido antes de reutilizar canal |
| T44 | Cerrar durante write de control y entregar completion después | No use-after-free ni reenvío; resultado de orden abortada |
| T45 | MIDI entrante continuo junto a negociación de audio | MIDI no bloquea respuestas de control; plazos y orden preservados |

L08 ejecuta T30–T34 con completions de cancelación que llegan. La ausencia deliberada de completion se introduce en T41 después de L09/L10; no bloquear la implementación temprana intentando resolverla con una liberación insegura.

## T46 a T53 Sesiones y drenaje

| ID | Estímulo | Resultado obligatorio |
| --- | --- | --- |
| T46 | Fallan todos o parte de los primeros submits de captura/playback | Start falla y retira los aceptados; no devuelve Streaming sin pipeline válido |
| T47 | Enqueue entrega completion inmediata antes de retornar al llamador | Sin underflow ni token perdido; accepted/terminal balanceados |
| T48 | Stop con N capturas y M playback; completions en orden inverso | Stop solo tiene éxito tras N+M terminales y cero callbacks ejecutándose |
| T49 | Abort devuelve error en uno de los pipes | Resultado no OK, generación invalidada; recursos retenidos hasta fin real |
| T50 | Vence stop y luego llegan todos los callbacks | Primero Faulted/Timeout; después cleanup seguro, sin volver a streaming solo |
| T51 | Callback de sesión A llega después de iniciar B en fixture | Solo modifica contadores de A; no escribe ring de B ni reencola |
| T52 | Retry/timer de A estaba encolado al parar; reloj avanza con B activa | No genera un submit de A dentro de B |
| T53 | Completion duplicada en falso y prepared bridge opcional | Detecta token terminal; sin doble decremento/release; bridge vive hasta retirar sus peticiones |

Para T51 construir la intercalación reteniendo explícitamente A y forzando un evento obsoleto después de su invalidez; la producción no debe iniciar B si A aún mantiene una operación que impide la reapertura física. El test comprueba aislamiento, no autoriza dos tomas simultáneas del dispositivo.

## T54 a T68 HAL y recuperación

| ID | Estímulo | Resultado obligatorio |
| --- | --- | --- |
| T54 | Dos clientes hacen Start; uno hace Stop | Un solo arranque físico; no parar hasta que no quede demanda |
| T55 | Mismo cliente Start dos veces/Stop dos veces, seguido de Stop extra | startDepth balanceado, sin underflow ni cierre nuevo por Stop extra |
| T56 | Remove sin Stop, Stop tardío y Remove duplicado | Retira demanda una vez; motor para solo en total 1→0 |
| T57 | Remove llega antes de terminar Start; llegar al límite de 64 clientes | Inicio tardío no revive cliente; capacidad devuelve error sin sobrescritura |
| T58 | Cambio 48→44,1 kHz falla en USB | No éxito ni frecuencia lógica nueva ficticia; estado anterior o Faulted coherente |
| T59 | Cambio exitoso y luego misma frecuencia solicitada otra vez | Una transición real; segunda solicitud no reabre USB ni repite notificaciones |
| T60 | Dos solicitudes config y Abort de la primera | Segunda conserva su ID/valores; no mezcla frecuencia/buffer pendientes |
| T61 | Host reentra a GetPropertyData desde PropertiesChanged | Sin deadlock, snapshot coherente y ninguna recursión infinita |
| T62 | 10.000 lecturas de propiedades sin cambio | Cero notificaciones generadas por leer; sin escrituras/logs/USB desde getters |
| T63 | Disconnect duplicado de device/interface durante streaming | Una invalidación; audio pasa a silencio; generación vieja no reencola |
| T64 | Reconnect antes de terminar el drenaje | No nueva adquisición exclusiva mientras la anterior siga viva |
| T65 | Tres aperturas de recuperación fallan | Tiempos 100/500/2.000 ms; máximo 3; Faulted estable sin spin |
| T66 | Recovery tiene éxito con/sin clientes y perfil distinto de DVS Vinyl | Con demanda: renegociar y streaming; sin demanda: OpenIdle y MIDI operativo; perfil confirmado preservado |
| T67 | Suspend/Resume y mensajes de configuración repetidos | Orden correcto, no bucle causado por nuestros propios eventos |
| T68 | Shutdown coincide con reaparición USB y timer de recovery | Ningún nuevo open; Closed o Faulted retenido correctamente |

## T69 a T88 Descarga e instalación

| ID | Estímulo | Resultado obligatorio |
| --- | --- | --- |
| T69 | Cargar/unload bundle falso 100 veces, inactivo | Cero callback o hilo sobreviviente; ASan limpio |
| T70 | Unload durante preopen y grace timer | Cancelación confirmada antes de devolver unload seguro |
| T71 | Unload durante EP1/stop con callbacks que llegan | Drenaje completo y memoria/código vivos hasta entonces |
| T72 | Operación nativa falsa nunca termina | Host informa unload inseguro; supervisor termina solo a su hijo, sin colgar suite |
| T73 | Evento de interés llega durante destructor | No usa gHost invalidado ni agenda reapertura/trabajo nuevo |
| T74 | Instalación limpia en filesystem temporal vacío | Todos los componentes esperados y journal committed |
| T75 | Actualización de versión previa con componentes distintos | Backup completo y nueva versión validada; no restos mezclados |
| T76 | Fallo inyectado antes/después de cada mutación de transacción | Estado previo restaurado exactamente; exit no cero; evidencia de paso |
| T77 | Interrumpir con TERM; después simular SIGKILL con journal a medias | TERM revierte; siguiente invocación reconcilia journal del segundo caso |
| T78 | Lock de otro PID vivo e intento de herencia solo por env var | BUSY sin mutaciones; herencia falsa rechazada |
| T79 | Herencia con token válido, lock stale y PID reciclado | Solo dueño correcto libera; verificar identidad de proceso además del número |
| T80 | Path vacío, raíz, symlink o candidato dentro de HAL activo | Rechazo antes de copy/rm/restart |
| T81 | Rollback falla por permiso/disco o restaura archivos con recibo PKG distinto | FAILED_RECOVERY o ROLLBACK_FILES_RESTORED_METADATA_PENDING según caso; backup conservado y sin éxito falso |
| T82 | Dos instalaciones concurrentes y resume repetido | Una transacción muta; otra BUSY; resume es idempotente |
| T83 | Descubrimiento de host devuelve cero/uno/varios procesos | Semántica definida; nunca usar primer PID arbitrario ni matar otros |
| T84 | Viejo host no termina, nuevo no enumera o hash difiere | Timeout/fallo seguido de rollback; no declarar instalado |
| T85 | CoreAudio o launchctl devuelve error | Propagado al journal y exit; no `|| true` en paso obligatorio |
| T86 | preinstall termina y postinstall usa otro PID | Lease sigue perteneciendo a transacción Installer verificable; no hueco de exclusión |
| T87 | Usuario de consola cambia o daemon ya estaba detenido | Gestiona solo instancia identificada; restaura estado y no duplica endpoints |
| T88 | Uninstall con drivers ajenos, ficheros ausentes y backups históricos | Solo quita producto previsto; resultado verificable; deja ajenos y backups seguros |

El backend falso admin registra cada comando y ruta. T74–T88 deben poder ejecutarse con usuario sin privilegios y sin acceso a `/Library`. T76 compara contenido, permisos relevantes y presencia/ausencia inicial de cada componente, no únicamente el bundle HAL.

## T89 a T98 Integración y herramientas de campaña

| ID | Estímulo | Resultado obligatorio |
| --- | --- | --- |
| T89 | Runner offline con USB conectado o ausente | Misma ejecución falsa, cero aperturas físicas y cero acceso al socket real |
| T90 | Selección CTest vacía y test que falla con stdout bonito | FAIL por ambos; el runner usa exit code e inventario real |
| T91 | Compilar Debug/Release y prepared opcional | Misma corrección funcional; no asserts eliminados ni enlaces rotos |
| T92 | ASan/UBSan sobre toda la suite nativa | Cero errores de memoria/UB en código propio; no supresión para pasar |
| T93 | TSan sobre IPC/control/HAL con carreras programadas | Cero carreras propias; limitaciones del runtime informadas como BLOCKED |
| T94 | 1.000 ciclos con seed 20261010 y seeds adicionales 1, 7, 42 | Recursos balanceados, generación aislada; seed y secuencia guardados al fallar |
| T95 | Runner físico en dry-run con dispositivos falsos | Lista UID exacto, acciones y restore; no acción física |
| T96 | Hijo de enumeración/audio colgado y PID ajeno señuelo | Mata solo hijo propio; guarda incidente, campaña FAIL |
| T97 | Lock ocupado y operador no confirma un paso manual | BUSY/WAITING, ninguna acción inferida ni PASS de la fila |
| T98 | Cambiar hash de candidato/estado del dispositivo entre dos pasos | Rechazo; evidencia no mezcla versiones; restore identificado |

Las seeds adicionales se ejecutan al menos 100 ciclos cada una. Las intercalaciones deterministas de T10–T88 siguen siendo obligatorias: el estrés aleatorio no las sustituye.

## Preparación para pruebas físicas

No usar directamente los scripts de instalación/recuperación antiguos. L15–L18 deben estar completadas antes de H01. Esta especificación no ejecuta acciones ahora: sirve para cuando el usuario haya encargado implementar y probar el candidato y exista una ventana coordinada de hardware.

1. Confirmar baseline de rollback instalado o copia verificable del paquete oficial; guardar todos sus componentes. No regenerar el baseline desde fuentes nuevas.
2. Verificar que el operador no está en una sesión de uso real; reservar la interfaz y la ruta de captura con el lock global.
3. Registrar Mac, OS, firmware observado, versión Traktor, USB/hub/cable, rate, buffer, perfil y ruteo. Documentar las conexiones físicas antes de tocar nada.
4. Fijar UID de Audio 8 DJ y del capturador externo. No seleccionar por índice ni usar silenciosamente micrófono interno como sustituto.
5. Guardar default input/output, frecuencia y buffer de los dispositivos que la prueba vaya a cambiar. Confirmar volumen/nivel de escucha adecuado antes de tonos.
6. Elegir una carpeta de evidencia nueva y la identidad exacta del candidato. El runner verifica el hash antes de cada instalación.

Comandos NUEVOS después de L18; sustituir las rutas de artefacto por valores reales, nunca ejecutar placeholders:

```sh
python3 scripts/run-macos-lifecycle-physical --preflight --dry-run --candidate /ruta/real/candidato.driver --baseline /ruta/real/baseline.driver --run-dir /ruta/real/evidencia-nueva
python3 scripts/run-macos-lifecycle-physical --scenario H02 --cycles 3 --candidate /ruta/real/candidato.driver --baseline /ruta/real/baseline.driver --run-dir /ruta/real/otra-evidencia-nueva
```

El runner adquiere el lock, registra su token, prepara backup y lo libera al acabar o interrumpirse. No pedir al operador que deje un lock manual huérfano. Los pasos guiados usan el mismo proceso dueño durante la ventana, con timeout de inactividad y salida limpia; el tiempo de espera humana no cuenta como latencia del driver.

## H01 a H12 Campaña física en orden

### H01 Baseline y capacidad de recuperación

- Duración orientativa: 15–30 min. Artefacto: baseline original.
- Enumerar Audio 8 DJ como 8 entradas/8 salidas; comprobar MIDI y control sin iniciar Traktor.
- Capturar tres ventanas idle de 60 s y una de streaming de 5 min a 48 kHz/512 frames. Guardar CPU, memoria, fds y threads de los procesos identificados.
- Probar el diagnóstico y preparar la restauración sin retirar aún el baseline. Verificar copia completa, firma/hash y plan dry-run.
- Si baseline ya está averiado, registrar incidente separado y recuperarlo antes de comparar. No atribuir al candidato una avería previa.
- PASS: baseline funcional y restauración disponible. Si falta capturador externo, las filas de audio quedan pendientes; no afirmar validación física completa.

### H02 Carga y descarga corta del candidato

- Instalar mediante helper transaccional, estabilizar 20 s y enumerar con timeout de 5 s.
- Confirmar UID, canales, rate, hash, socket, MIDI y snapshot de estado coherente. Crear una sesión de audio de 10 s y pararla.
- Descargar mediante helper, estabilizar y verificar ausencia/terminación del host anterior. Restaurar baseline y comprobar su salud.
- Repetir **3 ciclos**. Al tercero, conservar candidato solo si la siguiente fila se ejecuta en la misma ventana; registrar lo dejado instalado.
- PASS: tres ciclos completos; ningún error silenciado ni uso de recovery extraordinario.

### H03 Inicio y parada con clientes Core Audio

- Usar el nuevo `audio-lifecycle-test`, no Traktor para esta fila. **200 ciclos**: 100 a 44,1 kHz y 100 a 48 kHz, buffer 512.
- Por rate: 25 con un cliente y reapertura a 0 ms; 25 a 100 ms; 25 a 11 s, cruzando el grace period histórico; 25 con dos clientes, parando uno mientras el otro continúa.
- Cada streaming dura 2 s. Verificar progreso de captura/playback, no solo `AudioDeviceStart==0`.
- Registrar start/stop/retirada de IOProc, pendientes, estado, fd/thread/memoria. El cliente restante debe conservar audio continuo.
- PASS: todos los ciclos funcionales dentro de plazos, cero recursos por sesión que sobrevivan a la retirada y sin reiniciar servicios.

### H04 Abrir y cerrar Traktor

- Guardar ruteo/perfil del operador. Usar una sesión de prueba sin cambios destructivos en su biblioteca.
- **100 cierres normales:** 50 por rate, reproducción/captura verificadas durante ≥10 s antes de cerrar. Alternar reapertura inmediata y después de 11 s.
- **50 cierres forzados:** 25 por rate, únicamente el proceso Traktor de prueba identificado por PID; volver a abrir y comprobar audio/DVS. Esto forma parte explícita de la campaña de prueba, no de una sesión real del usuario.
- Después de cada cierre esperar el deadline de Stop y comprobar OpenIdle saludable, ausencia de ISO pendiente, MIDI/control disponibles y siguiente apertura funcional.
- Registrar el orden real Add/Start/Stop/Remove. No asumir que Traktor omite Stop; esta fila determina si ocurre.
- PASS: 150 ciclos; ningún estado residual, bloqueo del host o reinicio de servicios. Automatización UI incompleta exige operador, no ciclos inventados.

### H05 MIDI y clientes de control

- Con audio activo, abrir dos clientes de control; uno sano y un fixture lento con tráfico acotado por la herramienta. Usar MIDI de prueba identificable y sin equipos externos que disparen acciones no deseadas.
- Ejecutar **100 ciclos** de conexión/cancelación, alternando parada de audio y cierre del servidor. Verificar que cliente sano y audio no se bloquean.
- Comprobar el límite de clientes una vez, endpoint MIDI estable y reconexión del daemon tras reiniciar solo la instancia de prueba de bridge mediante herramienta gestionada.
- PASS: comportamiento de saturación conforme a D5, sin pérdida de control global, fd reutilizado ni endpoints duplicados. Ninguna inyección deliberada de fallo altera el resultado de otras filas.

### H06 Desconectar y reconectar USB

- **30 ciclos**, repartidos entre rates: 15 durante streaming y 15 en OpenIdle.
- El operador retira/reconecta el cable identificado. El runner registra evento IOKit, generación y demanda; no declara desconexión por un timeout simulado.
- Esperar hasta 15 s tras reaparición. Comprobar audio recuperado si aún hay demanda y MIDI/control en reposo; verificar no reapertura antes de drenar la sesión vieja.
- Probar cable directo primero. Si el uso habitual incluye hub, repetir una muestra de 6 ciclos con ese hub y etiquetar la configuración por separado.
- PASS: recuperación o error final explícito según el escenario de fallo inyectado; para reconexión de hardware sano se exige recuperar funcionalidad sin reiniciar servicios.

### H07 Suspensión y reanudación

- **20 ciclos:** 10 con streaming y 10 en OpenIdle, mitad a cada rate. Suspender 30–60 s cada vez.
- El operador controla suspensión/reanudación; el runner conserva journal. Un sleep no se cuenta como deadline de host incumplido; registrar inicio/fin y tratar wake como transición nueva.
- Al volver, plazo de recuperación 15 s y prueba funcional de audio/MIDI. Comprobar que el reloj HAL no retrocede dentro de una generación y que el cambio de seed anuncia una nueva ancla.
- PASS: sin reintentos intensivos, device fantasma ni necesidad de reiniciar macOS.

### H08 Frecuencia y varios clientes

- **100 cambios alternos** entre 44,1 y 48 kHz, con una reproducción/captura de 2 s después de cada cambio.
- Ejecutar 50 cambios desde aplicación de prueba, 25 desde Traktor y 25 con segundo cliente registrado, respetando la coordinación de configuración del host.
- Comparar propiedades publicadas, frecuencia negociada USB y contador de muestras. Probar buffer 512 y un tamaño adicional anunciado por el driver en 10 de los cambios; no asumir soporte de 128 frames.
- PASS: frecuencia coherente, sin notificación infinita, sin silencio persistente, errores de negociación propagados y recuperación sin nuevo host.

### H09 Instalación actualización y retirada

- **20 ciclos:** 8 baseline→candidato→baseline, 4 instalación limpia/retirada, 4 usando PKG candidato, 4 recuperación de transacción interrumpida en puntos predefinidos.
- Las interrupciones se limitan al proceso auxiliar/Installer de prueba y a estados recuperables previstos; no cortar alimentación ni forzar un panic del Mac. El corte eléctrico se cubre offline mediante journal.
- Verificar todos los componentes, no solo el HAL; LaunchAgent, firma, endpoints, aplicación, recibo de Installer y permisos relevantes. Comprobar que BlackHole/otros drivers y defaults no cambiaron por accidente.
- En fallo inyectado, rollback satisfactorio es PASS de esa prueba negativa, pero la instalación original debe informar fallo. Un fallo espontáneo es FAIL de campaña.
- PASS: todos los estados finales verificados, lock libre, proceso viejo retirado y siguiente sesión de audio funcional.

### H10 Calidad de audio y CPU

- Comparación A/B en la misma sesión: baseline y candidato por la misma ruta Audio 8 DJ→mixer→capturador externo, mismo cableado, ganancia, rate y buffer.
- Por artefacto y rate: tono/ruteo en los cuatro pares y tres capturas musicales de al menos 60 s con pasajes densos, transitorios y silencio. Confirmar que no se capturó el micrófono interno ni una ruta contaminada.
- Usar los analizadores existentes y los criterios de `docs/PHYSICAL_MUSIC_QUALITY_GATE.md`; guardar referencia/captura/CPU/JSON. No redistribuir música del usuario.
- Exigir ausencia de nuevos clicks, clipping, cruces de canal, ruido blanco, pérdida de captura y regresión de latencia. Comparar también el gate offline de timecode y perfil congelado 3072/1536/3072/9216.
- Si la baseline no pasa el gate absoluto existente, documentar el límite y no presentar el candidato como plenamente certificado; los criterios relativos tampoco deben empeorar. No bajar thresholds para alcanzar PASS.
- PASS: gates y comparación de CPU cumplen; juicio auditivo del operador registrado por separado.

### H11 Timecode y uso real en Traktor

- Dos decks físicos, ambos rates, modos absoluto y relativo. Por combinación: calibración, cue/start, reverse, scratch, levantar/reponer aguja y reapertura de Traktor.
- Registrar estabilidad del scope, enrutamiento, dropout count y respuesta comparada con 0.5.1. Probar MIDI/control después de cerrar Traktor.
- No inferir sensación de vinilo a partir del modelo offline de 78,667 ms. Guardar aceptación o rechazo explícito del operador.
- PASS: ninguna regresión funcional o perceptiva aceptada por el operador y métricas consistentes con H10.

### H12 Sesión prolongada y decisión final

- **24 h**, con checkpoint a las 8 h. Usar reproducción/captura controlada, MIDI acotado y reaperturas programadas de la aplicación de prueba. No exigir dejar una aguja física sobre un disco durante 24 h; el DVS real ya se cubre en H11.
- Primeras 8 h a 48 kHz; siguientes 8 h a 44,1 kHz; últimas 8 h alternando 30 min streaming/5 min OpenIdle, con transición de rate cada bloque. El cambio final de bloque que exceda 24 h se corta con cierre normal.
- Guardar recursos cada minuto y diagnósticos de transición, sin logs de audio por buffer. Comprobar una sesión Traktor corta al inicio y al final con el operador.
- Al final, parar todo, estabilizar 60 s y comparar con la muestra inicial postcalentamiento. Retirar candidato/restaurar baseline si no se ha decidido mantenerlo y se termina la ventana.
- PASS: ningún crash/hang/reinicio oculto, memoria y recursos dentro de límites, sonido funcional y las once filas anteriores completas. Un sistema reiniciado no puede continuar contando el mismo soak.

## Umbrales de recursos y reglas de rechazo

- Tras 20 ciclos de calentamiento, los contadores internos de objetos/peticiones/sockets por sesión deben volver exactamente al nivel de OpenIdle; ningún objeto retirado sigue creciendo por ciclo.
- Fds del host: al final de cada bloque, mismo nivel estable con las mismas conexiones de diagnóstico. Cualquier diferencia necesita identificar su dueño; no aceptar una pendiente creciente.
- Threads: como máximo +2 frente al nivel postcalentamiento y sin tendencia por bloques; distinguir workers compartidos de libdispatch de threads propios supervivientes.
- Memoria: comparar tras calentamiento de 10 min, usando medianas de ventanas de 5 min. Presupuesto inicial de crecimiento retenido ≤32 MiB y pendiente durante soak ≤0,5 MiB/h. Un exceso rechaza hasta analizar, no demuestra automáticamente fuga. Los contadores de propiedad internos deben estar balanceados aunque RSS no baje por cachés.
- CPU idle del driver: p95 ≤1 % después de 20 s de estabilización. CoreAudio idle: p95 no más de 2 puntos porcentuales por encima de la baseline de la misma sesión. En streaming aplicar gates existentes de sonido/CPU y ausencia de regresión significativa respecto a baseline.
- Si procesos ajenos saturan el equipo e invalidan una medida, marcar `INVALID_ENVIRONMENT`, conservarla y repetir en condiciones comparables. No invalidar solo porque salió mal.
- Cualquier petición vieja que toque una sesión nueva, sanitizador con error propio, descriptor ajeno cerrado, enumeración colgada, reinicio espontáneo del host o necesidad de reiniciar macOS: FAIL inmediato.
- Una recuperación de servicios para continuar H03/H04/H06/H07/H08/H12 cuenta como fallo, aunque después vuelva a sonar. H09 puede probar recuperación deliberada, claramente identificada.

## Evidencia obligatoria y formato de resultados

Cada run crea:

```text
manifest.json          # commit, diff hash, flags, SDK, OS, hashes baseline/candidato
results.ndjson         # una entrada por test/ciclo, nunca sobrescribir un fallo
transitions.ndjson     # estados, generaciones, operationID, motivos y duraciones
resources.csv          # tiempo, PID/start-time, CPU, RSS, fds, threads
commands.json          # argv, cwd, exit code, stdout/stderr o rutas a sus ficheros
summary.json           # PASS/FAIL/BLOCKED/NOT_RUN, totales y gate de promoción
incident/              # solo si hubo fallo; snapshot previo a recuperar
```

Campos mínimos de una fila de resultado:

```json
{
  "schema": "opena8dj.lifecycle-test.v1",
  "test_id": "T15",
  "iteration": 1,
  "mode": "offline",
  "status": "NOT_RUN",
  "expected": "server closes without draining the slow peer",
  "observed": null,
  "duration_ms": null,
  "source_commit": null,
  "source_dirty_hash": null,
  "candidate_sha256": null,
  "engine_generation": null,
  "stream_generation": null,
  "evidence_paths": []
}
```

No llenar ese ejemplo con PASS hasta ejecutar el caso. Un `summary.json` de campaña exige H01–H12; `physical_promotion_allowed` solo puede ser true cuando todas estén PASS y no haya incidencias sin resolver. Offline green significa `offline_candidate_ready=true`, nunca aprobación física.

## Qué hacer al primer incidente

1. Marcar ciclo y hora exacta. No cerrar automáticamente otras aplicaciones ajenas.
2. Guardar últimos eventos, operación pendiente, generaciones, contadores, PIDs/start-time y hashes.
3. Recoger log unificado acotado al driver/CoreAudio y, si es posible, una muestra breve del proceso bloqueado. Cada recolector tiene timeout propio.
4. Detener nuevos ciclos. Intentar cierre normal con plazo; si no funciona, usar exclusivamente el helper de recuperación ya probado.
5. Restaurar baseline, verificar enumeración/audio y lock. Si no se puede restaurar, informar el estado exacto y conservar el backup; no repetir una cascada de kills.
6. Asociar el incidente a una tarea Lxx y un test Txx nuevo o existente. Corregir, ejecutar regresión y empezar una nueva campaña con nuevo hash. Conservar el run fallido.
