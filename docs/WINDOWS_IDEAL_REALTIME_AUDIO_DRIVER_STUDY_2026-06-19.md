# Windows Ideal Real-Time Audio Driver Study - 2026-06-19

Este documento define como deberia ser un driver Windows moderno, ideal y de
alto rendimiento para Audio 8 DJ / OpenA8DJ. Es un estudio separado del plan
Linux y no sustituye al driver macOS actual.

Objetivo: una arquitectura Windows 10/11 real, profesional, de baja latencia,
con calidad audiofila medible, bajo uso de CPU, soporte completo del hardware y
validacion fuerte antes de ofrecer cualquier build como candidato.

## Resumen ejecutivo

Un driver Windows perfecto para este hardware no es solo un driver USB KMDF.
Debe ser una pila de audio completa:

```text
Apps Windows / DAWs / Traktor
    |
    +-- WASAPI shared/exclusive, system audio, apps normales
    |
    +-- ASIO, para DAWs y Traktor cuando convenga
    |
Windows audio engine / endpoint stack
    |
ACX 1.1 audio driver, KMDF, WaveRT-style streaming
    |
OpenA8DJ audio engine
    - topology 8-in/8-out
    - packet scheduler
    - format converter
    - clock/cadence follower
    - MIDI/control bridge
    - diagnostics counters
    |
WDF USB CAIAQ transport
    - EP1 control
    - 0x82 isochronous capture
    - 0x06 isochronous playback
    |
Audio 8 DJ hardware
```

La direccion principal debe ser ACX 1.1/KMDF. ACX es el modelo moderno de
Microsoft para drivers de audio y esta recomendado para nuevo desarrollo. La
direccion secundaria, si ACX no encaja con este dispositivo USB
vendor-specific, debe ser AVStream/KS. PortCls/WaveRT aporta conceptos
importantes, pero no deberia ser el primer objetivo directo porque Microsoft
documenta que PortCls no soporta buses externos como USB.

ASIO debe existir como capa profesional, pero no debe esconder una pila Windows
incompleta. El dispositivo tiene que funcionar tambien como endpoint normal de
Windows.

## Principios de producto

- Calidad audible primero: el driver no esta listo si suena peor que el
  baseline validado aunque sus contadores internos esten limpios.
- Baja latencia real: no basta con exponer buffers pequenos si aparecen DPCs,
  jitter, sidebands, dropouts o carga alta.
- Uso de CPU bajo y estable: el hot path no puede depender de asignaciones,
  logs, locks amplios, conversiones variables o temporizadores imprecisos.
- Funcionalidad completa: 8 entradas, 8 salidas, MIDI, modos de entrada,
  ground lift, software lock, 44.1/48 kHz primero y tasas mayores solo si se
  prueban fisicamente.
- Integracion Windows correcta: Device Manager, INF, catalog, signing, PnP,
  power management, hotplug, sleep/wake, WASAPI, ASIO, diagnosticos y uninstall.
- Observabilidad de ingenieria: todo candidato debe dejar evidencia de USB,
  audio, DPC/ISR, CPU, errores de paquete y captura fisica.

## Requerimientos funcionales

### Audio endpoints

El driver debe publicar endpoints Windows usables por aplicaciones normales.
El diseno debe decidir con pruebas entre dos modelos:

- Un endpoint render/capture multicanal 8-in/8-out.
- Cuatro pares estereo A/B/C/D para ergonomia de DJ.

La recomendacion inicial es prototipar ambos. Traktor y DAWs decidiran. El
modelo multicanal suele ser mas limpio arquitectonicamente, pero cuatro pares
estereo pueden ser mas faciles de rutear para usuarios de DJ.

Requerimientos:

- 8 canales de salida: Output A/B/C/D L/R.
- 8 canales de entrada: Input A/B/C/D L/R.
- 24-bit hardware path preservado; conversion interna controlada y medible.
- 44.1 kHz y 48 kHz como primer milestone.
- 88.2/96 kHz solo despues de loopback fisico y stress.
- Buffer inicial estable: 512 frames.
- Buffers menores solo despues de aprobar captura fisica, DPC/ISR y Traktor.
- Latencia publicada honesta: no declarar latencias optimistas.

### MIDI

El hardware tiene MIDI 1 in / 1 out. El driver Windows ideal debe exponer MIDI
estable despues de install, reboot, hotplug y sleep/wake.

Opciones:

- Integrar MIDI en el paquete kernel/user-mode del driver.
- Usar un componente separado de tipo MIDI si Windows lo exige.
- Exponer un servicio usuario solo para control avanzado, no para transporte
  MIDI hot-path si eso empeora latencia.

Aceptacion:

- MIDI In/Out visible en apps.
- Loopback largo sin bytes perdidos.
- No bloquear ni perturbar el stream de audio.

### Controles de hardware

Debe haber una API estable y una UI/control tool para:

- modo timecode vinyl;
- modo timecode CD/line;
- modo phono;
- ground-lift vinyl;
- ground-lift CD/line;
- ground-lift phono;
- software lock;
- estado de firmware/hardware si el protocolo lo permite.

Los cambios de control deben estar serializados con el estado de streaming.
Si cambiar un control durante playback puede romper cadencia, el driver debe
rechazarlo o aplazarlo a un punto seguro, no hacerlo de forma oportunista.

### Instalacion y distribucion

Requerimientos de release:

- INF correcto para `USB\VID_17CC&PID_1978`.
- `.sys`, `.inf`, `.cat`, simbolos PDB internos, versionado y release notes.
- Test signing para desarrollo.
- Microsoft signing / Partner Center para distribucion publica.
- MSI/bootstrapper solo despues de que el driver real este listo.
- Uninstall limpio.
- No usar binarios, firmware, logos ni payloads de Native Instruments.

## Requerimientos no funcionales

### Latencia

Metas iniciales:

- 48 kHz / 512 frames estable en full-duplex.
- 48 kHz / 256 frames despues de aprobar 512.
- 44.1 kHz equivalente.
- 128 frames solo como objetivo posterior si DPC/ISR y captura fisica lo
  justifican.

No se debe prometer 64/32 frames hasta tener datos reales. Un buffer pequeno
que produce clicks no es baja latencia util.

### CPU

Objetivos:

- Uso de CPU estable y bajo durante playback/capture sostenido.
- Sin picos periodicos correlacionados con USB completions.
- Sin spin loops.
- Sin trabajo de UI/control en el hot path.
- Sin logs por paquete en streaming.

### Calidad audiofila

El driver debe conservar:

- pitch/speed correctos;
- ausencia de white noise, radio-noise, metallic bass, sidebands audibles;
- canal limpio e independiente;
- ausencia de clicks/outliers;
- respuesta estable bajo carga de DAW/Traktor;
- no degradarse cuando hay capture activo.

La captura fisica externa sigue siendo obligatoria para calificar builds.

## Arquitectura recomendada

### Capa 1: ACX audio driver

Responsabilidades:

- Publicar endpoints render/capture.
- Declarar formatos y topology.
- Gestionar state transitions: create, run, pause, stop.
- Proporcionar RT Packet Stream o modelo ACX equivalente.
- Sincronizar con el motor OpenA8DJ.
- Exponer propiedades de latencia y glitch counters.

Puntos a investigar en WDK:

- Sample ACX minimo que publique render/capture reales.
- Si ACX permite poseer WDF USB targets en el mismo stack.
- Si conviene ACX multi-stack para separar audio circuit y USB transport.
- Como mapear cuatro pares estereo vs un endpoint 8-channel.

### Capa 2: OpenA8DJ audio engine

Modulo propio, framework-agnostic donde sea posible.

Responsabilidades:

- Ring buffers render/capture.
- Conversion float/int/24-bit CAIAQ segun formato activo.
- Cadencia USB.
- Mezcla cero: no DSP oculto, no SRC salvo que se documente explicitamente.
- Contadores.
- Estado de formato.
- Sin memoria dinamica despues de stream start.

Este motor debe ser testeable sin hardware con fuentes deterministas y sinks de
memoria.

### Capa 3: WDF USB CAIAQ transport

Responsabilidades:

- Enumerar dispositivo y endpoints.
- EP1 command channel.
- Isochronous IN `0x82`.
- Isochronous OUT `0x06`.
- URB/request pools preasignados.
- Packet status tracking.
- Queue depth tracking.
- Reconexion/hotplug.
- Power management.

Modelo recomendado:

```text
stream start:
    allocate fixed IN request pool
    allocate fixed OUT request pool
    allocate packet descriptor storage
    allocate nonpaged audio buffers
    arm counters
    submit IN pool

IN completion:
    validate packet statuses
    read capture payload into capture ring
    acquire unused OUT request
    copy/derive OUT packet layout from IN packet layout
    fill OUT payload from render ring or silence
    submit OUT
    requeue IN

OUT completion:
    mark OUT request reusable
    update counters
```

El punto clave es que Audio 8 DJ parece sensible a cadencia. Windows no debe
empezar con un timer software arbitrario para OUT si el modelo Linux CAIAQ y la
evidencia macOS apuntan a capture-paced output.

### Capa 4: ASIO facade

ASIO debe construirse sobre el mismo motor de streaming, no sobre otro camino
de audio separado.

Responsabilidades:

- API ASIO para DAWs/Traktor.
- Buffer-size negotiation coherente.
- Reporte de latencia.
- Sample-rate switching.
- Channel names A/B/C/D.

Reglas:

- Separar licencias Steinberg SDK del repo.
- No copiar SDK ni asumir permisos de Microsoft/Yamaha.
- Comparar ASIO contra WASAPI exclusive con las mismas pruebas fisicas.

### Capa 5: user-mode service/control panel

Responsabilidades:

- Cambios de perfil.
- Diagnosticos exportables.
- Firmware/status si existe.
- UI/CLI.
- Recoleccion de logs fuera del hot path.

Nunca debe estar en la ruta critica de audio.

## Notas de implementacion

### Memoria y concurrencia

- Todo buffer de streaming debe estar preasignado y nonpaged.
- Evitar locks globales.
- Usar SPSC rings por direccion cuando sea posible.
- Separar locks de control, stream state y packet pools.
- No llamar APIs pageable desde completion/audio callbacks.
- Marcar codigo hot path como no pageable.
- Mantener contadores atomicos simples.

### Formato de audio

El hardware usa stream CAIAQ 24-bit big-endian segun la implementacion macOS
actual. Windows debe tener conversiones explicitas:

- host format activo;
- formato interno del ring;
- formato USB;
- clipping/saturation definido;
- dithering solo si se justifica y se puede desactivar para tests bit-exact.

No hacer conversion por muestra con ramas complejas en el hot path si se puede
vectorizar o convertir en bloques.

### Cadencia y reloj

El driver debe distinguir:

- reloj de aplicacion/audio engine;
- posicion de ring;
- paquete USB;
- tiempo fisico de DAC/ADC;
- latencia publicada.

No mezclar esos contadores. El error clasico seria usar un contador que parece
estable en software pero no representa lo que el DAC esta recibiendo.

### Power management

El driver debe soportar:

- idle;
- stream start/stop repetido;
- hotplug;
- suspend/resume;
- sleep/wake;
- sample-rate switch tras stop;
- device surprise removal.

Nunca dejar el dispositivo consumiendo un timeline vacio si Windows cree que el
stream esta parado.

### Diagnosticos

Contadores minimos:

- render frames submitted;
- capture frames delivered;
- USB IN packets completed;
- USB OUT packets completed;
- packet status errors;
- zero-length packets;
- late completions;
- missed OUT request;
- underruns;
- overruns;
- min/max/avg queue depth;
- sample-rate changes;
- start/stop count;
- DPC/ISR trace markers if feasible.

## Plan de validacion

### Nivel 0: build/package

- Visual Studio + WDK build.
- InfVerif.
- Inf2Cat.
- Test signing.
- pnputil install/uninstall.
- Device Manager clean.

### Nivel 1: endpoint skeleton

- Endpoint aparece en Windows Sound.
- WASAPI shared tone.
- WASAPI exclusive tone.
- No hardware todavia si el skeleton es virtual.
- ETW limpio.

### Nivel 2: USB hardware smoke

- Detectar `17cc:1978`.
- Mapear EP1, `0x82`, `0x06`.
- Leer info/control.
- Programar 48 kHz.
- Stream silencio sin errores.

### Nivel 3: audio fisico determinista

- 1 kHz Output A/B.
- Captura externa por interfaz independiente.
- Medir frecuencia, nivel, sidebands, clipping, dropouts.
- Repetir 5, 10, 30 minutos.

### Nivel 4: full-duplex

- Output A/B + Input A/B.
- Luego A/B/C/D.
- Channel isolation.
- No leakage.
- No speed drift.

### Nivel 5: apps reales

- Windows system audio.
- Spotify/local player.
- WASAPI exclusive test app.
- Traktor.
- DAW ASIO.

### Nivel 6: stress

- CPU load.
- Disk load.
- Network load.
- USB devices adicionales.
- Sleep/wake.
- Hotplug.
- Reboot.
- Long run 2+ horas.

## Riesgos principales

- ACX no encaja limpiamente con un USB vendor-specific full-duplex driver.
- El modelo de endpoints elegido no coincide con Traktor.
- ASIO introduce una segunda ruta de bugs.
- DPC/ISR de otros drivers oculta problemas propios.
- Windows power management rompe cadencia despues de idle/sleep.
- Cambios de control durante streaming alteran el hardware.
- 88.2/96 kHz existen en metadata pero no son estables con calidad real.

## Decisiones iniciales recomendadas

1. Hacer una auditoria ACX samples/WDK antes de escribir mas transporte.
2. Construir un endpoint ACX virtual minimo.
3. Integrar el motor USB despues de que endpoint/RT stream este claro.
4. Empezar con 48 kHz / 512 frames.
5. Implementar capture-paced OUT desde el primer prototipo hardware.
6. Exponer solo Output A/B al principio si reduce superficie.
7. No empezar por ASIO; ASIO viene despues del endpoint Windows estable.
8. No publicar MSI hasta que audio, MIDI, hotplug y captura fisica pasen.

## Fuentes principales

- Microsoft ACX overview:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-audio-class-extensions-overview>
- Microsoft ACX version information:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-version-overview>
- Microsoft ACX streaming:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-streaming>
- Microsoft ACX multi-stack:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-multi-stack>
- Microsoft WDM audio overview:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/getting-started-with-wdm-audio-drivers>
- Microsoft PortCls introduction:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/introduction-to-port-class>
- Microsoft WaveRT miniport development:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/developing-a-wavert-miniport-driver>
- Microsoft low-latency audio:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio>
- Microsoft USB Audio 2.0 driver:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/usb-2-0-audio-drivers>
- Microsoft media glitch analysis:
  <https://learn.microsoft.com/en-us/windows-hardware/test/weg/delivering-a-great-media-experience>
- Microsoft low-latency UAC2/ASIO repository:
  <https://github.com/microsoft/low-latency-audio>
- Steinberg ASIO driver notes:
  <https://helpcenter.steinberg.de/hc/en-us/articles/17863730844946-Steinberg-built-in-ASIO-Driver-information-download>
- USB-IF UAC2 specification page:
  <https://www.usb.org/document-library/usb-device-class-definition-audio-devices-release-20-errata-and-ecn-through-april>
