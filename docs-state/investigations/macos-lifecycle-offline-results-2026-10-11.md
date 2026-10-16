# Ejecución offline de estabilidad HAL

Fecha: 11 de octubre de 2026. La Audio 8 no está conectada, por lo que aquí no se instala ni carga el HAL y no se reinicia Core Audio.

## Identidad del trabajo

La evidencia debe distinguir el commit base del árbol modificado. El manifiesto offline asociado registra ambos hashes y la configuración compilada cuando el build termine. Este documento no atribuye a un binario pruebas ejecutadas solo contra fakes.

## Clasificación de los 16 fallos CTest históricos

Los 16 pertenecen al grupo `local_evidence` en CMake. Su fallo protege contra afirmar readiness sin la evidencia local esperada; no se cambiarán a SKIP ni se falseará un resultado verde.

| Test | Causa observada | Relevancia y siguiente evidencia |
| --- | --- | --- |
| `opena8djcpp_timecode_readiness_gate` | Sin todos los artefactos requeridos de matriz, señal, decode, stress y transporte | Calidad/DVS; campaña de señal de R14 |
| `opena8djcpp_prepared_transport_migration_gate` | Sin candidato/métricas completos de migración | No es requisito de lifecycle si el candidato estable no habilita esa ruta; conservar como fallo hasta clasificar build final |
| `opena8djcpp_transport_budget_model` | Inputs del runtime preparado incompletos; modelo devuelve FAIL | Riesgo de CPU/cola solo si la variante forma parte del candidato; R00/R15 deben cotejar flags reales |
| `opena8djcpp_audiophile_tone_gate` | WAV esperado `local-analysis/physical-tone/.../tone.wav` ausente | Calidad de audio; señal real y WAV baseline/candidato de R14 |
| `opena8djcpp_audiophile_precision_claim_gate` | Bundles físico y runtime ausentes | Claim de precisión/producto, no prueba de descarga; mantener bloqueado para claims |
| `opena8djcpp_human_test_rc_gate` | Bundle HAL/PKG/evidencia de RC ausentes | Gate de release; bundle exacto y expediente de R15 |
| `opena8djcpp_physical_evidence_frontier` | No hay runs físicos reconocidos | Evidencia de candidato de R13–R14 |
| `opena8djcpp_physical_capture_forensics` | No hay WAV físicos elegibles | Diagnóstico de captura de R14 |
| `opena8djcpp_direct_usb_path_attribution` | No hay runs USB con artefactos obligatorios | Ruta física/atribución de señal de R14 |
| `opena8djcpp_physical_window_readiness_gate` | Faltan pruebas offline limpias y ruta vigente | Preflight de R13; no promover sin ruta legible |
| `opena8djcpp_capture_readiness_contract` | Captura ausente y ruta ambigua | Cableado/ruta real y estado Core Audio de R13–R14 |
| `opena8djcpp_physical_route_matrix_contract` | Matriz de pares físicos incompleta | Matriz física de entrada/salida en R14 |
| `opena8djcpp_diagnostic_pass_semantics_gate` | Evidencia insuficiente para consumir los artefactos semánticos | Actualizar artefactos a medida que R13–R15 los producen |
| `opena8djcpp_product_quality_claim_gate` | Evidencia insuficiente; claim permanece bloqueado | Mantener claim bloqueado; R14/R15 si se pretende habilitarlo |
| `opena8djcpp_hal_candidate_safety_gate` | Última ventana de safety es FAIL; guard y enumeración no pasan | Directamente relevante: R13 debe registrar una secuencia controlada y estable baseline/candidato |
| `opena8djcpp_hal_transport_runtime_gate` | No hay evidencia actual; defaults de transporte candidato no cumplen el contrato esperado | Directamente relevante al build: auditar flags y ruta activa en R00/R12; no instalar hasta resolver |

## Cambios offline iniciados

- El guard conserva códigos de salida, timeout y señal del enumerador; la ausencia del ejecutable pasa a `INFRA_ERROR`.
- Guard y health dejan de ofrecer reinicio/movimiento del HAL en cascada; rechazan esos argumentos antes de escribir.
- El supervisor de hijo usa deadline monotónico, process group propio y captura de stdout/stderr.
- Se añadieron policy JSON y tests CTest aislados con enumerador, top, pgrep y health falsos.

La comprobación con fakes valida clasificación y aislamiento del guard, no el comportamiento de Core Audio ni del bus USB.

## Cierre de la campaña offline del 11 de octubre

El candidato HAL se compiló con `make hal`, se firmó localmente con firma ad-hoc y pasó `codesign --verify --deep --strict`. El manifiesto actualizado registra `build/OpenA8DJ.driver`, sus flags efectivos, el SHA-256 `9eea7bd1bfc88dcac3423dfac0f2bf5cf107c33487ee3fe1ffdceb7cb6b998cb` y el UUID `AD7534A2-81FE-3F7C-BCA3-86B82BC71CC4` (arm64). `verify-lifecycle-manifest.py --require-bundle` también pasó. El bundle quedó sin instalar.

La batería dirigida de lifecycle terminó en **22/22 PASS en Debug y 22/22 PASS en Release**. Los logs son [Debug](../../local-analysis/lifecycle/20261011-offline/ctest-debug-lifecycle.log) y [Release](../../local-analysis/lifecycle/20261011-offline/ctest-release-lifecycle.log); ambos incluyen las puertas R00–R04, el self-test de callbacks cero y el chequeo estático del lock. El inventario CTest fue actualizado a 112 casos para Release, incluyendo los casos nuevos y dejando explícito el benchmark que no formó parte de esta batería dirigida. R00 (identidad y cobertura del inventario) volvió a pasar en Debug y Release con este manifiesto.

La prueba corta de `audio-stack-health.py` usó dos muestras reales de `ps` y del contador Mach de CPU idle y devolvió PASS con umbrales permisivos. No equivale a los tres preflight windows de 10 segundos ni a observar Traktor. El guard, el supervisor y el manager de transacciones tienen pruebas offline; la integración privilegiada del instalador, la coordinación de hooks PKG, el host USB aislado, el cierre de todas las rutas IPC/MIDI y las tareas R04–R12 siguen incompletos. Por eso R12 no ha pasado y no se debe conectar la Audio 8 para la campaña física todavía.

Después de esa batería también se recompiló y firmó de nuevo el HAL tras el cierre acotado de transferencias/colas, el retiro del canal EP1 al vencer su deadline, la emisión IPC MIDI no bloqueante para pares lentos y el cleanup IPC protegido por generación. El harness valida algunas intercalaciones IPC contra producción, pero EP1, drenaje de transferencias y el resto del protocolo aún carecen de pruebas aisladas.

La prueba R03 añadida para el instalador confirma que el flujo físico se detiene con `BLOCKED` antes de cualquier mutación porque faltan el backend privilegiado y la identidad del host; no se invocó `sudo`, no se tocó `/Library` y no se reinició Core Audio. Esto es un cierre seguro, no una instalación validada.

La suite Release y Debug excluyendo los 16 tests etiquetados `local_evidence` y `offline_bench` terminó en **95/95 PASS en cada configuración**. Los logs completos son [Debug](../../local-analysis/lifecycle/20261011-offline/ctest-debug-non-evidence.log) y [Release](../../local-analysis/lifecycle/20261011-offline/ctest-release-non-evidence.log). El harness R04 compila la clase de transporte real y valida seis comportamientos con USB/audio desconectados: limpieza de apertura parcial, límite 16/17, cleanup por generación tras reutilizar fd, aislamiento de peer lento, serialización de frames y stop/broadcast concurrentes. Esos 16 gates se mantienen pendientes porque requieren resultados/capturas físicas que aún no existen; el benchmark queda excluido de esta campaña. El inventario Release contiene 112 tests declarados.

El cleanup IPC lleva ahora un ID de generación por conexión, para que una retirada tardía no cierre una conexión nueva si macOS reutiliza el mismo número de descriptor. El harness probó esa intercalación concreta con la implementación de producción.
