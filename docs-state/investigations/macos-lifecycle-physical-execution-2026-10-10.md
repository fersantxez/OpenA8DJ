# Ejecución física de estabilidad del HAL — 2026-10-10

Revisión del 11 de octubre: el [análisis y plan de corrección](macos-lifecycle-remediation-plan-2026-10-11.md) precisa el alcance de estos resultados. Los 201 starts/stops son llamadas del cliente Core Audio y los callbacks se contaban en total; no acreditan señal física en cada ciclo. La recuperación completa que pasó quedó registrada después del reinicio del Mac. El error de enumeración con código 0 procede de un defecto del clasificador y no permite distinguir timeout de otro error. Se conserva abajo el informe original de ejecución.

## Decisión

**No promover el candidato.** La campaña de audio start/stop fue satisfactoria, pero la prueba repetida de descarga/recarga no pasó sus puertas de salud. La instalación activa del laptop se dejó en la versión base verificada.

## Evidencia

- Host: macOS 26.6.2, Apple Silicon; Audio 8 DJ presente. Traktor Pro 3.5.1.
- Compilación HAL y firma ad-hoc del candidato: correctas (`build-hal.log`, `sign-hal.log`, `physical-retry1/codesign-verify.log`).
- Instalación transaccional del candidato: correcta en la repetición; el hash coincide con `4b4cbba820373fc5da1be266d158bf0beda7a43dd62cf1b9a5f1b319173878b9`.
- Prueba de flujo físico: PASS, 100 ciclos a 44.1 kHz y 100 a 48 kHz; 201 StartIO/201 StopIO, 889 callbacks; buffer 512; 11 s idle/reopen. Ver `physical-retry1/start-stop-stress.json`.
- Descarga/recarga del plugin: una de las repeticiones elevó `coreaudiod` a ~110% CPU y fallaron salud/enumeración en la ventana de prueba. Se guardó `unload-reload-final/coreaudiod.sample`; la muestra cae en reconciliación de `PropertiesChanged` de Core Audio y no muestra una pila `OpenA8DJ`.
- Recuperación: se instaló de nuevo el bundle base desde el respaldo, se verificó firma y hash exacto `4af4b1207f81846208fd2fcc0b8f5a600c2e11346d523ee5fd8e4f55700b9f66`, y el candidato quedó aparcado bajo `HAL.disabled`. Un guard posterior confirmó enumeración PASS y salud de Core Audio PASS.
- Traktor: se abrió con el bundle base y mostró su biblioteca; cerró después de `Storing Track Collection`. No hubo reproducción. El candidato no se probó en Traktor.

## Incidente del sistema

`last reboot` registra un arranque a las 18:02; macOS creó `/Library/Logs/DiagnosticReports/shutdown_stall_2026-10-10-180218_basoa.shutdownStall` a las 18:02:18. Ninguno de los scripts de esta campaña contiene un comando de reinicio o apagado y no se ejecutó uno desde la terminal. La secuencia temporal no demuestra causalidad: se desconoce quién inició el apagado y si el incidente está relacionado con estas pruebas. Se suspendieron pruebas adicionales que reinician Core Audio.

## Límites y siguiente trabajo

La prueba con `killall coreaudiod` no identifica por sí sola si el pico se debe al plugin o a la reconciliación interna del host. Sin embargo, la puerta de descarga repetida no pasó y el código aún tiene un `DelayedCloseThread` detached que puede coincidir con la retirada. Además, se observó un reinicio no solicitado del OS en la ventana de recuperación, sin atribución causal. Por eso no se afirma que la descarga in-process sea segura. Completar la propiedad/cancelación/drenaje de operaciones (L09-L14), añadir harness de host hijo y repetir primero una descarga/reload controlada con métricas de tiempo y CPU. Mantener el bundle base activo mientras el gate falle.
