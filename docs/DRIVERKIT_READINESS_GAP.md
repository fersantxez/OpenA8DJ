# DriverKit Readiness Gap

Auditoria limitada a `/Users/fer/dev/audio8djcpp`.

Advertencia operativa: PROHIBIDO tocar, editar, formatear, generar archivos,
limpiar, resetear, instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj`
o `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo se puede
escribir en `/Users/fer/dev/audio8djcpp`. No tocar hardware/audio/CoreAudio/USB
sin lock global y sin autorizacion de ventana.

## Blocker exacto

`real_driverkit_sdk_and_selected_xcode_missing` significa que el repo puede
probar contratos offline de DriverKit, pero todavia no puede probar un dext real
ni un build real de AudioDriverKit/USBDriverKit.

Evidencia local actual:

- `local-analysis/cpp-offline/driverkit-sdk-preflight-gate.json:11-17`:
  `sdk_required_for_real_driverkit=true`,
  `xcrun_driverkit_sdk_available=false`,
  `xcode_select_path=/Library/Developer/CommandLineTools`,
  `selected_full_xcode=false`, `xcode_app_present=false`.
- `local-analysis/cpp-offline/driverkit-sdk-preflight-gate.json:23-34`:
  `/Applications` free space is `9.917 GiB`, below the `80.000 GiB` install
  threshold, so `product_driverkit_build_allowed=false` and
  `real_driverkit_claim_blocked=true`.
- `docs/DECISION_LOG.md:7293-7331` records the promotion decision: the scaffold
  exists, but a real DriverKit candidate must be measurable as a build artifact,
  not only source skeleton.
- `scripts/run-cpp-offline-gates:400-409` hard-codes
  `real_driverkit_sdk_and_selected_xcode_missing` in `promotion_hard_blockers`.

Apple public docs support the repo's distinction: DriverKit is the framework for
drivers, DriverKit drivers are system extensions that are activated by a host
app, and USB transport requires the `com.apple.developer.driverkit.transport.usb`
entitlement. See official Apple docs only:

- https://developer.apple.com/documentation/driverkit
- https://developer.apple.com/documentation/driverkit/creating-a-driver-using-the-driverkit-sdk
- https://developer.apple.com/documentation/SystemExtensions/installing-system-extensions-and-drivers
- https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.developer.driverkit.transport.usb

## Que evidencia offline falta

Falta una prueba build-only/no-install con SDK real. En concreto:

- Un `xcode-select -p` que apunte a un full Xcode, no a Command Line Tools.
- Un `xcrun --sdk driverkit --show-sdk-path` exitoso.
- Evidencia de `xcodebuild` mostrando el DriverKit SDK seleccionado.
- Evidencia de que existen herramientas necesarias para un dext build-only
  (`clang`, `iig`, `codesign`, `xcodebuild`) bajo el developer dir seleccionado.
- Un target opt-in que compile `driverkit/extension/src/OpenA8DJAudioDriver.cpp`,
  `driverkit/extension/src/OpenA8DJAudioDevice.cpp`, y los `.iig` contra
  DriverKit/AudioDriverKit/USBDriverKit sin instalar ni activar nada.
- Un JSON de build-only que registre artefacto esperado, bundle id, SDK path,
  SDK version, toolchain path, entitlements template consumido, and
  `driver_installed_or_activated=false`, `system_extension_activated=false`.

Lo que si existe hoy:

- Scaffold no instalable:
  `local-analysis/cpp-offline/driverkit-extension-scaffold-contract.json:1-16`.
- Binding fuente al runtime model:
  `local-analysis/cpp-offline/driverkit-runtime-binding-gap-gate.json:1-39`.
- El scaffold esta excluido del build default:
  `tools/driverkit_extension_scaffold_contract.cpp:126-131`.

## Comandos seguros que podrian comprobar SDK/Xcode

Estos comandos son de solo lectura del estado de developer tools. No instalan,
no activan system extensions, no cargan dext, no tocan audio, CoreAudio, USB ni
hardware:

```sh
xcode-select -p
xcrun --sdk driverkit --show-sdk-path
xcodebuild -showsdks
xcodebuild -version -sdk driverkit Path
xcodebuild -version -sdk driverkit SDKVersion
xcrun --sdk driverkit --find clang
xcrun --sdk driverkit --find iig
xcrun --find codesign
test -d /Applications/Xcode.app/Contents/Developer && echo full_xcode_present
```

Comandos build-only aceptables para una futura ventana de software, si el SDK
existe y el repo ya tiene un target opt-in:

```sh
cmake -S /Users/fer/dev/audio8djcpp -B /Users/fer/dev/audio8djcpp/build/driverkit-sdk-probe -DOPENA8DJCPP_ENABLE_DRIVERKIT_SDK_BUILD=ON
cmake --build /Users/fer/dev/audio8djcpp/build/driverkit-sdk-probe --target opena8djcpp_driverkit_extension_build_probe
```

No ejecutar `systemextensionsctl`, no activar desde host app, no instalar, no
cargar, no descargar Xcode, no usar hardware.

## Archivos que deberian endurecerse

- `tools/driverkit_sdk_preflight_gate.cpp:94-118`: subir a schema v2 y agregar
  comprobaciones de `xcodebuild -showsdks`, `xcodebuild -version -sdk driverkit
  Path`, `SDKVersion`, `xcrun --sdk driverkit --find clang`, `xcrun --sdk
  driverkit --find iig`, y `DEVELOPER_DIR` efectivo.
- `tools/driverkit_sdk_preflight_gate.cpp:120-169`: emitir campos separados para
  `driverkit_sdk_path`, `driverkit_sdk_version`, `driverkit_sdk_path_exists`,
  `iig_available`, `clang_available`, `xcodebuild_driverkit_sdk_visible`,
  `build_only_probe_allowed`, y mantener `product_driverkit_build_allowed=false`
  hasta que todos pasen.
- `scripts/run-cpp-offline-gates:1671-1710`: copiar esos nuevos campos al
  resumen offline para que no queden enterrados en el JSON individual.
- `tools/evidence_schema_check.cpp:448-458`: exigir los nuevos campos del schema
  v2. El schema debe seguir pasando en hosts sin SDK siempre que el blocker este
  registrado de forma explicita.
- `CMakeLists.txt:160-174`: agregar solo un target opt-in de build probe. No debe
  entrar al default build ni a CTest normal, igual que el scaffold actual.
- `driverkit/extension/README.md:21-44`: anadir la secuencia build-only/no-install
  permitida y la prohibicion explicita de activation/install como parte del
  checklist.
- `tools/driverkit_extension_scaffold_contract.cpp:109-125`: exigir que el
  README mencione el build-only probe, que `system_extension_activated=false`,
  and que la extension siga excluida del default build.
- `docs/SUCCESS_METRICS.md:1974-1995`: actualizar los criterios de readiness:
  SDK/Xcode readiness no se cierra con `xcrun` solamente; se cierra con
  `product_driverkit_build_allowed=true` mas build-only probe PASS.

## Siguiente cambio minimo recomendado

Agregar un `DriverKit SDK build probe` opt-in, sin instalar ni activar dext:

1. Extender `opena8djcpp_driverkit_sdk_preflight_gate` a schema v2 con las
   comprobaciones de SDK/toolchain anteriores.
2. Agregar un target CMake no-default, por ejemplo
   `opena8djcpp_driverkit_extension_build_probe`, habilitado solo con
   `OPENA8DJCPP_ENABLE_DRIVERKIT_SDK_BUILD=ON`.
3. Hacer que el probe compile contra el DriverKit SDK cuando exista, escriba
   evidencia JSON build-only, y falle con un blocker explicito cuando no exista.
4. Mantener fuera de alcance cualquier host app activation, `systemextensionsctl`,
   signing/install real, hardware, audio, CoreAudio o USB.

Ese cambio convierte la readiness de "tenemos scaffold y contratos offline" a
"podemos demostrar, sin cargar un dext, que el source compila contra el SDK real".
Hasta entonces, la recomendacion es mantener
`real_driverkit_sdk_and_selected_xcode_missing` como hard blocker.
