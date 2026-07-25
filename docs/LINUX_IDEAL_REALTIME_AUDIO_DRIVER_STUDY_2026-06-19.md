# Linux Ideal Real-Time Audio Driver Study - 2026-06-19

Este documento define como deberia ser un driver Linux moderno, ideal y de alto
rendimiento para Audio 8 DJ / OpenA8DJ. Es un estudio separado del plan Windows
y usa Linux como arquitectura real de destino, no solo como pista para macOS.

Objetivo: un driver Linux upstream-quality, con ALSA bien integrado, baja
latencia real, PREEMPT_RT-friendly, calidad audiofila medible, bajo uso de CPU,
soporte completo del hardware y comportamiento excelente con PipeWire, JACK,
DAWs y workflows DVS/timecode.

## Resumen ejecutivo

El driver Linux ideal debe ser un driver ALSA USB/CAIAQ mantenible, no una app
de usuario que empuja paquetes USB por su cuenta. Debe integrarse con el modelo
PCM de ALSA, exponer controles correctos, mantener timestamps y delay honestos,
y tratar el streaming USB isocrono como el centro del diseno.

Arquitectura objetivo:

```text
Apps / DAWs / Traktor-like apps / DJ software
    |
PipeWire / JACK / ALSA clients
    |
ALSA PCM + control + rawmidi
    |
snd-opena8dj or improved snd-usb-caiaq path
    |
OpenA8DJ CAIAQ engine
    - PCM hw constraints
    - period accounting
    - USB packet scheduler
    - capture-paced playback
    - MIDI/control handling
    - tracepoints/counters
    |
Linux USB core
    - URB pools
    - isochronous IN/OUT
    |
Audio 8 DJ hardware
```

La referencia publica mas importante es `snd-usb-caiaq`, porque ya implementa
la familia CAIAQ y usa un modelo donde la completacion de captura da forma al
paquete de salida. El driver perfecto debe estudiar esa ruta, preservar sus
lecciones y mejorar observabilidad, robustez, RT behavior y validacion.

## Principios de producto

- Integracion nativa: ALSA PCM/control/rawmidi primero; PipeWire/JACK deben
  funcionar por encima sin hacks.
- Full-duplex desde el diseno: playback y capture no son subsistemas aislados.
- Cadencia USB correcta: para este hardware, packet layout y URB lifecycle son
  tan importantes como el contenido de muestras.
- RT-friendly: debe funcionar bien en kernels normales y PREEMPT_RT.
- Calidad medible: captura fisica externa, no solo ausencia de XRUNs.
- Upstreamability: estilo, locking, tracepoints, controles, docs y tests deben
  ser aceptables para kernel/ALSA.

## Requerimientos funcionales

### ALSA PCM

El driver debe exponer PCM playback y capture con:

- 8 canales de salida.
- 8 canales de entrada.
- Nombres y channel map A/B/C/D L/R.
- 44.1 kHz y 48 kHz primero.
- 88.2/96 kHz solo si el hardware y captura fisica lo prueban.
- Formato que preserve el camino 24-bit del hardware.
- Buffer y period constraints honestos.
- Full-duplex estable.
- `pointer`, `trigger`, `hw_params`, `prepare`, `close` y period elapsed
  correctos.

La API ALSA exige que el driver mantenga bien `avail`, `delay`, hardware
pointer y application pointer. Si esos valores son falsos, JACK/PipeWire/DAWs
pueden tomar decisiones erroneas aunque "suene algo".

### ALSA controls

Controles requeridos:

- Input mode: timecode vinyl, timecode CD/line, phono.
- Ground lift para vinyl.
- Ground lift para CD/line.
- Ground lift para phono.
- Software lock.
- Clock/rate status si esta disponible.
- Streaming diagnostics si se exponen como solo lectura.

Los nombres deben seguir convenciones ALSA donde aplique. Controles raros deben
documentarse claramente.

### RawMIDI

El hardware debe publicar MIDI In/Out via ALSA rawmidi:

- visible por `aconnect`, PipeWire/JACK MIDI bridge y DAWs;
- loopback largo sin bytes perdidos;
- no bloqueo de audio;
- recuperacion tras hotplug.

### Userspace compatibility

Objetivos:

- `aplay` / `arecord`.
- `alsaloop`.
- PipeWire.
- JACK con buffers bajos.
- Ardour/Reaper/DAWs Linux.
- Software DJ compatible con ALSA/JACK/PipeWire.
- UCM profile si ayuda a presentar endpoints y controles.

## Requerimientos no funcionales

### Latencia

Metas iniciales:

- 48 kHz estable con periodos conservadores.
- 48 kHz con periodos bajos despues de validar fisicamente.
- JACK/PipeWire a 256/128 frames solo cuando el driver no produce XRUNs ni
  sidebands fisicos.

No se debe optimizar a buffers diminutos antes de demostrar que packet cadence
esta resuelto.

### CPU

El driver debe:

- minimizar trabajo en completion handlers;
- no imprimir por paquete;
- evitar conversion por muestra innecesariamente cara;
- evitar locks de alta contencion;
- no hacer polling salvo fallback diagnosticado;
- no bloquear en USB completion;
- mantener cache locality razonable.

### Calidad de audio

Aceptacion:

- pitch/speed exactos;
- ausencia de clicks;
- ausencia de white noise/radio noise;
- sidebands bajo umbral medido;
- channel isolation;
- capture estable;
- DVS/timecode scope estable;
- no degradacion bajo capture + playback simultaneo.

## Arquitectura recomendada

### Capa 1: ALSA card/device registration

Responsabilidades:

- Registrar `struct snd_card`.
- Registrar PCM playback/capture.
- Registrar rawmidi.
- Registrar kcontrols.
- Registrar proc/debug info si procede.
- Manejar disconnect/hotplug.

Debe integrarse al arbol `sound/usb` o una ruta equivalente si se busca
upstream. Si se basa en `snd-usb-caiaq`, la pregunta es si conviene extenderlo
o crear un driver nuevo con compatibilidad especifica para Audio 8 DJ.

### Capa 2: PCM engine

Responsabilidades:

- Definir `snd_pcm_hardware`.
- Aplicar constraints de rate, channels, period size, buffer size.
- Gestionar `hw_params` y asignacion DMA/USB buffer.
- Implementar `trigger`.
- Implementar `pointer`.
- Mantener period accounting.
- Llamar `snd_pcm_period_elapsed()` desde el contexto correcto.
- Gestionar XRUNs sin esconderlos.

Regla: el driver debe reportar underrun/overrun cuando ocurre. Silenciar fallos
para que apps no vean XRUN solo mueve el bug a la salida analogica.

### Capa 3: USB CAIAQ scheduler

El punto central es el ciclo de URBs. Modelo objetivo inspirado por
`snd-usb-caiaq`:

```text
stream start:
    allocate IN URBs
    allocate OUT URBs
    allocate coherent/appropriate buffers
    initialize active masks and counters
    submit IN URBs

IN completion:
    if not streaming: requeue or stop safely
    validate iso packet status and length
    find unused OUT URB
    derive OUT iso_frame_desc layout from IN packet layout
    copy capture samples into ALSA capture ring
    fill playback samples from ALSA playback ring
    update period counters
    submit OUT URB
    requeue IN URB

OUT completion:
    mark OUT URB free
    wake prepare/start waiters if needed
    update diagnostics
```

El driver debe ser muy cuidadoso con:

- `iso_frame_desc.length`;
- `iso_frame_desc.actual_length`;
- `iso_frame_desc.offset`;
- packet status;
- output URB active mask;
- requeue order;
- GFP flags correctas (`GFP_ATOMIC` donde toque);
- no acceder buffers despues de disconnect.

Historicamente, bugs de offsets y tracking de output URBs en CAIAQ causaron
fallos reales. Eso debe tratarse como riesgo de primer nivel.

### Capa 4: controls/MIDI

Los comandos EP1/control deben estar aislados del streaming hot path.

Reglas:

- Serializar controles con state machine del dispositivo.
- Evitar control writes en medio de paquetes de audio si alteran hardware.
- No dormir bajo locks que se usan en completion.
- MIDI no debe tomar locks de PCM durante tiempo variable.

### Capa 5: observabilidad

El driver ideal debe tener observabilidad upstream-friendly:

- ALSA tracepoints: `hwptr`, `applptr`, `xrun`, `hw_ptr_error`.
- Tracepoints propios si se justifica: USB late completion, packet error,
  output URB exhaustion.
- `/proc/asound` info util.
- debugfs opcional para counters si se acepta.
- rate-limited warnings.
- No logging continuo en streaming.

## PREEMPT_RT y locking

PREEMPT_RT cambia supuestos clasicos del kernel:

- Muchas interrupciones se fuerzan a threaded interrupts.
- `spinlock_t` puede dormir bajo RT.
- `raw_spinlock_t` conserva semantica dura y debe usarse con mucho cuidado.
- Priority inheritance reduce inversion, pero no arregla secciones criticas
  largas.

Reglas para el driver:

- Mantener hard/primary interrupt work minimo.
- No depender de que softirq sea no-preemptible.
- Evitar `local_bh_disable()` como proteccion implicita.
- Usar locks pequenos y con ownership claro.
- No mezclar locks de PCM, USB y controls en ordenes ambiguos.
- Probar con lockdep, KASAN/KCSAN donde sea posible.
- Probar en kernel normal y PREEMPT_RT.

El objetivo no es "usar RT para esconder un driver caro". Es que el driver sea
predecible incluso cuando RT permite que hilos mas prioritarios preempten rutas
del kernel.

## Notas de implementacion

### Memory allocation

- Preasignar URBs antes de start.
- Preasignar buffers.
- No `kmalloc` en completion hot path salvo que este probado fuera de RT y no
  haya alternativa.
- Usar flags de allocation correctas segun contexto.
- Liberar todo de forma segura en disconnect.

### Period accounting

El driver debe contar frames, no bytes confusos, en las superficies publicas.
Internamente puede contar bytes por stream, pero debe convertir a frames de
forma consistente.

Errores tipicos:

- llamar `snd_pcm_period_elapsed()` demasiadas veces;
- no llamarlo cuando varios periodos han pasado;
- mezclar capture/playback period counters;
- reportar pointer adelantado al hardware;
- esconder XRUN y rellenar silencio sin avisar.

### Timestamping y delay

ALSA permite timestamps de trigger, timestamps actuales, `avail`, `delay` y
audio timestamps. Para DVS y DAWs, delay falso puede ser tan malo como un click.

El driver debe:

- reportar delay basado en ring + USB + hardware estimado;
- refinar trigger timestamp si el hardware exige retraso;
- usar clocks monotonic/raw segun API;
- documentar limites de precision.

### Format conversion

Se necesita una capa clara:

- ALSA sample format expuesto;
- formato interno;
- packing CAIAQ;
- endianess;
- clipping/saturation;
- silence pattern correcto;
- check bytes/padding si el protocolo los usa.

Las conversiones deben ser testeables con patrones conocidos.

### Power management

Soportar:

- start/stop repetido;
- hotplug;
- USB reset;
- suspend/resume;
- autosuspend desactivable si rompe audio;
- idle sin dejar hardware en estado raro;
- reconnect sin kernel oops.

Para interfaces pro-audio, la politica de power saving debe ser conservadora.
El ahorro de energia no puede romper streaming.

## Plan de validacion

### Nivel L0: build/kernel hygiene

- Build contra kernel target.
- `modpost` limpio.
- `sparse` donde aplique.
- `checkpatch` razonable.
- Lockdep sin warnings.
- KASAN/KCSAN si es viable.

### Nivel L1: enumeration

- `lsusb -v`.
- `dmesg`.
- `/proc/asound/cards`.
- `/proc/asound/devices`.
- `/proc/asound/pcm`.
- `aplay -l`.
- `arecord -l`.
- `aconnect -l`.

### Nivel L2: PCM smoke

- `speaker-test`.
- `aplay` known WAV.
- `arecord` known input.
- Full-duplex `alsaloop`.
- 44.1/48 kHz.

### Nivel L3: physical quality

- Output A/B 1 kHz.
- Captura con interfaz externa independiente.
- Medir level, frequency, sidebands, clipping, clicks.
- Repetir con musica real.
- Repetir A/B/C/D.

### Nivel L4: low latency

- JACK/PipeWire at 512, 256, 128 frames.
- `cyclictest` baseline.
- ftrace/perf alrededor de USB completions.
- ALSA tracepoints para XRUN/hwptr.

### Nivel L5: stress

- CPU load.
- Disk load.
- USB bus load.
- Network load.
- Suspend/resume.
- Hotplug.
- Long run.
- Capture + playback + MIDI.

### Nivel L6: DVS/timecode

- Input mode vinyl.
- Input mode CD/line.
- Deck A/B scope.
- Channel isolation.
- No drift.
- No mode switching glitches.

## Upstream y mantenimiento

Si el objetivo es upstream:

- Preferir extender `snd-usb-caiaq` si el hardware encaja y evita duplicacion.
- Separar cambios genericos de cambios Audio 8 DJ.
- Documentar quirks por VID/PID.
- Incluir fixes pequenos y revisables.
- Mantener logs rate-limited.
- Evitar APIs privadas.
- Escribir commit messages con sintomas, causa y prueba.

Si el objetivo inicial es fuera del arbol:

- Mantener modulo DKMS solo como puente temporal.
- No disenar APIs que luego no puedan upstream.
- Mantener tests y docs iguales que si fuese upstream.

## Riesgos principales

- Diferencias entre kernels/distribuciones.
- PipeWire/JACK ocultan o amplifican problemas de driver.
- PREEMPT_RT cambia timing y locks.
- USB autosuspend rompe el hardware.
- El driver existente `snd-usb-caiaq` ya soporta parte del hardware y un fork
  mal planteado duplica bugs.
- Cambios en packet layout producen audio corrupto sin XRUN visible.
- Timestamp/delay impreciso rompe DVS aunque playback normal suene bien.

## Decisiones iniciales recomendadas

1. Auditar `snd-usb-caiaq` actual antes de escribir driver nuevo.
2. Probar Audio 8 DJ en Linux moderno y capturar comportamiento real.
3. Decidir extension de `snd-usb-caiaq` vs driver nuevo solo despues de esa
   auditoria.
4. Mantener capture-paced OUT como hipotesis principal.
5. Exponer solo 44.1/48 kHz inicialmente.
6. Tratar PREEMPT_RT como requisito de compatibilidad, no como parche final.
7. Crear harness fisico de captura igual de estricto que en macOS.

## Fuentes principales

- Linux sound subsystem docs:
  <https://docs.kernel.org/sound/index.html>
- Linux ALSA driver API:
  <https://docs.kernel.org/sound/kernel-api/alsa-driver-api.html>
- Linux ALSA driver guide:
  <https://docs.kernel.org/sound/kernel-api/writing-an-alsa-driver.html>
- Linux ALSA timestamping:
  <https://docs.kernel.org/sound/designs/timestamping.html>
- Linux ALSA tracepoints:
  <https://docs.kernel.org/sound/designs/tracepoints.html>
- Linux PREEMPT_RT theory:
  <https://docs.kernel.org/core-api/real-time/theory.html>
- Linux PREEMPT_RT differences:
  <https://docs.kernel.org/core-api/real-time/differences.html>
- Linux USB host-side API:
  <https://docs.kernel.org/driver-api/usb/usb.html>
- Linux URB documentation:
  <https://docs.kernel.org/driver-api/usb/URB.html>
- Current Linux CAIAQ audio source browser:
  <https://codebrowser.dev/linux/linux/sound/usb/caiaq/audio.c.html>
