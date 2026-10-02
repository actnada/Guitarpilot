# GuitarPilot Oboe v0.7 — Golden Rhythms

Esta versión agrega dos ritmos de referencia tomados de la lógica de
`ako/backing-tracks` para hacer una comparación controlada con GuitarPilot.

## Golden 01 — strum_up_down

Referencia externa: `pop-sections.btml`

- BPM inicial: 120
- 4/4
- 8 corcheas por compás
- secuencia: DOWN / UP / DOWN / UP / DOWN / UP / DOWN / UP
- DOWN: MIDI velocity 80
- UP: MIDI velocity 70
- spread: 12 ticks a 480 PPQ
- a 120 BPM, 12 ticks = 12.5 ms
- DOWN: grave -> aguda
- UP: aguda -> grave

No se añade jitter, Palm Mute, treble-only ni otras humanizaciones propias.

## Golden 02 — folk

Referencia externa: `folk-strum.btml`

- BPM inicial: 100
- 4/4
- beat 1: ROOT_BASS, velocity MIDI 85
- beat 2: notas superiores del acorde, simultáneas, velocity 70
- beat 3: ROOT_BASS, velocity MIDI 85
- beat 4: notas superiores del acorde, simultáneas, velocity 70

Nota importante: `backing-tracks` genera aquí un voicing MIDI de triada y no
una digitación física de seis cuerdas. GuitarPilot conserva la estructura
rítmica y las velocities, pero la aplica a sus digitaciones físicas reales y
a su `rootBassString`.

## Objetivo del test

No estamos intentando que FluidSynth y GuitarPilot tengan el mismo timbre.
La comparación es de lógica rítmica:

1. estabilidad del groove;
2. dirección del rasgueo;
3. spread;
4. dinámica DOWN/UP;
5. bajo + acorde;
6. respuesta al cambio manual de acorde.

Si la estructura se siente bien en GuitarPilot con nuestras muestras reales,
Oboe sigue siendo válido y podemos construir patrones propios sobre esta base.

## Instalación

Esta entrega contiene solo archivos fuente a reemplazar:

- `app/src/main/cpp/GuitarEngine.cpp`
- `app/src/main/cpp/GuitarEngine.h`
- `app/src/main/cpp/native-lib.cpp`
- `app/src/main/java/com/example/guitarpilot/NativeAudioEngine.kt`
- `app/src/main/java/com/example/guitarpilot/MainActivity.kt`
- `app/src/main/res/layout/activity_main.xml`

Se basa en v0.6, por lo que conserva el banco MF y los 17 Palm Mute ya copiados.

Compilar:

```powershell
cd C:\Users\admin\AndroidStudioProjects\GuitarPilotOboe
.\gradlew.bat installDebug --no-configuration-cache
```

## Prueba recomendada

Golden 01:
- seleccionar GOLDEN 01;
- comprobar que el BPM salte a 120;
- probar G, D, Em, C;
- cambiar acordes manualmente sin detener el transporte.

Golden 02:
- seleccionar GOLDEN 02;
- comprobar que el BPM salte a 100;
- probar G, C, D, Em y Bm;
- escuchar bajo en 1/3 y acorde en 2/4.

No modificar todavía slider de separación para juzgar Golden 01: el patrón
usa sus propios 12 ticks de spread y no depende del slider de 13 ms.


## v0.8 — A/B de gesto de rasgueo (ako + ChordBox)

Esta versión conserva Golden 01/02 y agrega cuatro pruebas controladas basadas en ChordBox:

- **CB FOLK 20**: D D U - U D U -, spread total de 20 ms.
- **CB FOLK 13**: mismo patrón y misma curva de velocity, pero 13 ms entre cuerdas.
- **CB BALLAD 20**: patrón Pop Ballad de ChordBox, spread total 20 ms.
- **CB BALLAD 13**: mismo patrón, 13 ms entre cuerdas.

La intención es comparar únicamente el gesto/spread. En ambos métodos se aplica la misma curva de ChordBox: velocity base 90 y -3 por cada nota posterior del mismo rasgueo. UP invierte el orden.

### Cambio de acorde
La v0.8 deja de cortar las colas activas al seleccionar otro acorde. Solo cancela voces futuras todavía no iniciadas. El siguiente ataque de cada cuerda se encarga de liberar suavemente la voz anterior. Esto busca eliminar la pausa audible observada en v0.7.

### Bajo de C
La lógica se verificó: C = x32010 y ROOT_BASS = cuerda 5, traste 3 (C3). Si ese bajo sigue sonando extraño, el siguiente diagnóstico debe ser el WAV `s5_f03_c3_mf.wav`, no el mapeo del acorde.


# v0.9 — Lifecycle + String Targets

Objetivo de esta versión: conservar el buen sonido alcanzado en v0.8 y añadir dos mejoras estructurales sin cambiar Oboe.

## 1. Lifecycle: el motor deja de pertenecer a MainActivity

Cambios:
- `NativeAudioEngine.create()` es idempotente: si ya existe el motor nativo, no crea otro.
- Se añadió `NativeAudioEngine.isCreated()`.
- Al rotar/recrear la Activity NO se destruye Oboe ni se recargan las 135 muestras.
- El estado de sesión (PLAY, acorde, patrón, BPM, delay y volumen) vive en el `companion object` mientras el proceso siga activo.
- `onDestroy()` solo destruye el motor cuando la Activity realmente está terminando por una salida explícita; una rotación no lo hace.

Esto es la primera etapa del arreglo del bug de minimizar/rotar. Si un teléfono mata el proceso completo en segundo plano, una Activity no puede impedirlo; en ese caso la siguiente etapa será mover la sesión a un Foreground Service de audio. No se añadió aún porque este ZIP es un overlay y no contiene el AndroidManifest.xml real del proyecto.

## 2. Targets / zonas físicas de cuerda

Nuevo modelo de evento:
- `ALL_ALLOWED`: todas las cuerdas permitidas por el acorde.
- `BASS_3`: cuerdas físicas 6,5,4, intersectadas con la máscara del acorde.
- `TREBLE_3`: cuerdas 3,2,1.
- `TREBLE_4`: cuerdas 4,3,2,1.
- `ROOT_BASS`: una sola cuerda/traste raíz explícita del acorde.

La máscara final es siempre:

`target del ritmo ∩ forma física del acorde`

Ejemplo C = `x32010`:
- `BASS_3` -> cuerdas 5 y 4 (la 6 está muteada por el acorde).
- `TREBLE_3` -> 3,2,1.
- `ROOT_BASS` -> exclusivamente cuerda 5, traste 3 = C3.

## 3. Nuevo patrón de prueba: ZONAS

Botón: `ZONAS · GRAVES / AGUDAS`

A 100 BPM alterna:
- zona grave,
- zona aguda,
- acorde completo,
- zona aguda de 4 cuerdas,
- bajo raíz explícito,
- y vuelve a combinar zonas.

El objetivo no es convertirlo todavía en ritmo oficial, sino verificar auditivamente que GuitarPilot pueda modelar la mano atacando distintas regiones de la guitarra.

## 4. Auditoría del bajo de C

La ruta `ROOT_BASS` queda explícitamente separada de `BASS_3`.
Para C, `ROOT_BASS` no busca "la primera cuerda grave válida": usa directamente cuerda 5 / traste 3 / C3. Esto permite comparar con PIMAMI, donde ese mismo bajo ya sonaba correctamente.

## Prueba recomendada

1. Iniciar un ritmo y pulsar HOME/minimizar. Volver y verificar que siga sonando.
2. Girar la pantalla mientras está reproduciendo y comprobar que el transporte no reinicia.
3. Probar `ZONAS` con C, G, D, Em y Bm.
4. En C escuchar especialmente la diferencia entre la zona grave y el evento ROOT_BASS.
5. Si minimizar todavía corta el audio, registrar el video/logcat: el siguiente paso será Foreground Service.
