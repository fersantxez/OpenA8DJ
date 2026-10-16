# Ejecución offline de estabilidad HAL — 12 de octubre de 2026

La Audio 8 permanece desconectada. No se instaló el HAL, no se modificó `/Library` y no se reinició Core Audio.

## Candidato y cobertura

`make hal`, `make sign-hal`, `codesign --verify --deep --strict build/OpenA8DJ.driver` y `verify-lifecycle-manifest.py --require-bundle` pasan. El manifiesto actual ata las fuentes a SHA-256 `d4bf8a8361a68c7e25c7b61f19fbf86a501c5c227c4cc347d3e7beb070f78490` y UUID arm64 `8DBD51A1-DA0F-38AA-8F30-5219D6F0578C`. La firma ad-hoc se verifica localmente; esto no prueba todavía carga del candidato.

La auditoría de R01 usa deltas de CPU por proceso de `ps time`, identidad PID/nacimiento/boot, reloj civil y monotónico por ventana, UUID de boot y `audio-list` al cierre de cada ventana. Una ejecución integrada real de tres ventanas de 10 s pasó: `coreaudiod` 5,36 %, suma vigilada 11,02 %, idle global medio 48,72 % y enumeración PASS en las tres ventanas y al final. También queda guardada una ejecución anterior `ENVIRONMENT_BUSY` por Spotify; no se cerró ninguna aplicación ni se hizo recuperación automática.

R05 ahora serializa `accept/stop`, confirma propiedad inode/device del pathname antes de eliminarlo, preserva cleanup por generación, reconstruye frames fragmentados y limita replies de peers lentos mediante poll con deadline de 50 ms. El broadcast no espera a cada peer: si el peer está lleno se desconecta. Se añadió un test dedicado que llena el socket y verifica retorno antes de 250 ms. La suite R05 pasa en Debug, Release y ASan/UBSan.

La suite completa sin gates `local_evidence` ni benchmark terminó 99/99 en Debug y 99/99 en Release después de recapturar el manifiesto. La primera repetición había tenido solo un fallo de identidad R00 por manifiesto desfasado; se reconstruyó y firmó el HAL, se regeneró el manifiesto y R00 pasó en Debug/Release antes de repetir la suite completa.

## Trabajo pendiente antes de conectar la tarjeta

R05 sigue parcial: falta adoptar la cola de salida acotada por conexión y extender la política al daemon MIDI. R04 todavía no inyecta el adaptador real de IOUSBHost ni prueba cada adquisición, abort y completion tardía. R06–R12 conservan trabajo de concurrencia, drenaje captura/reproducción, callbacks HAL, desconexión/suspensión, descarga con workers cancelados y cierre de la puerta offline. También falta completar y probar el backend privilegiado/recibos/rollback del instalador R02–R03. El script de instalación continúa fail-closed antes de mutar el sistema.

Por estos bloqueos, aún no es momento de conectar la Audio 8: primero deben cerrarse los gates offline R02–R12. Las pruebas físicas R13–R15 van después de esa puerta.
