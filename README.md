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


# v0.10 — Humanization Lab

Esta versión conserva intacto el patrón `ZONAS` de v0.9 y añade un A/B
controlado llamado `ZONAS HUMAN`.

## Qué cambia en ZONAS HUMAN

El reloj musical sigue siendo exacto. La humanización se aplica únicamente
al gesto que genera cada evento:

- retardo local por evento entre 0 y 6 ms, centrado alrededor de 3 ms;
- variación de +/-10% en el spread total del rasgueo;
- variación de +/-3% en la energía global del golpe;
- variación adicional de +/-3% por cuerda;
- se conserva la caída natural de energía desde la primera cuerda tocada;
- las máscaras `BASS_3`, `TREBLE_3`, `TREBLE_4`, `ALL_ALLOWED` y
  `ROOT_BASS` siguen funcionando exactamente como en v0.9.

La implementación usa un PRNG xorshift32 muy ligero dentro del hilo de audio
y no usa `std::random` ni asignaciones dinámicas durante el callback.

## Por qué se prueba como A/B

`ZONAS` queda como referencia limpia y `ZONAS HUMAN` usa exactamente la
misma secuencia de eventos. De este modo se puede juzgar únicamente si la
microvariación mejora o empeora la interpretación.

## Bajo de C

No se modifica todavía su ruta porque `ROOT_BASS` sigue resolviendo
explícitamente C como cuerda 5, traste 3 (C3), y ese mismo sample funciona
correctamente en PIMAMI. El comportamiento extraño restante se seguirá
aislando comparando `ZONAS` contra `ZONAS HUMAN` y, si hace falta, con
una prueba dedicada del bajo.

## Prueba recomendada

1. Probar `ZONAS` con G, D, Em, C y Bm.
2. Cambiar a `ZONAS HUMAN` sin cambiar BPM ni volumen.
3. Escuchar si desaparece parte de la repetición mecánica.
4. Prestar atención especial a C en los eventos de zona grave y ROOT_BASS.
5. Confirmar que cambiar de acorde no reinicia el transporte.


# Pendientes de audio y articulaciones — recursos faltantes

Esta sección documenta mejoras confirmadas por la investigación de bancos SFZ
que **no conviene implementar todavía** porque GuitarPilot no dispone aún de
los recursos de audio necesarios. El objetivo es preservar la arquitectura y
retomar estas ideas cuando exista presupuesto, nuevos bancos compatibles o
grabaciones propias.

## 1. Round robin real por nota y articulación

Los bancos estudiados usan varias tomas diferentes de una misma nota para
evitar la repetición mecánica. Ejemplos observados incluyen secuencias de 3,
4, 8 y hasta 16 posiciones.

Estado actual de GuitarPilot:
- Iowa aporta capas `pp`, `mf` y `ff`, pero son **niveles de intensidad**,
  no variantes equivalentes de round robin.
- No se deben alternar `pp/mf/ff` como si fueran tomas diferentes de la misma
  intensidad.

Recurso necesario:
- 2–4 grabaciones por cuerda/traste/articulación y por capa dinámica, idealmente
  con el mismo instrumento, posición de micrófono y cadena de grabación.

Arquitectura futura prevista:

```text
SampleSlot
├─ stringId
├─ fret
├─ articulation
├─ velocityLayer
└─ variants[]
```

## 2. Ataques DOWN y UP con timbre propio

Unreal separa articulaciones de ataque descendente y ascendente. GuitarPilot
actualmente puede invertir el orden físico de las cuerdas, pero usa el mismo
tipo de muestra para ambos sentidos.

Recurso necesario:
- muestras separadas de púa DOWN y UP para sustain y, si es posible, mute.

Mientras no existan esas tomas:
- mantener la diferencia por dirección mediante orden de cuerdas, zona,
  dinámica, spread y humanización;
- no simular falsamente un timbre UP/DOWN distinto si el material fuente no lo
  contiene.

## 3. Palm mute coherente con guitarra nylon

El Palm Mute actual cumple bien la función rítmica, pero su timbre corresponde
a una guitarra de carácter más metálico que el banco principal de nylon.

Recurso necesario:
- palm mute grabado con una guitarra de nylon o una fuente con timbre compatible
  con el banco principal.

Hasta entonces:
- conservar el Palm Mute actual como recurso experimental;
- no expandir masivamente esa articulación hasta encontrar una fuente tímbrica
  más consistente.

## 4. Release samples y Stop Noise

Los bancos SFZ estudiados disparan capas específicas al soltar una nota,
cambiar de cuerda o realizar un choke. Esas capas incluyen round robin,
`offset`, `offset_random`, variación de amplitud y `trigger=release`.

Estado actual:
- GuitarPilot usa release/fade por voz, suficiente para evitar cortes duros.
- No existen todavía samples de release propios.

Recurso necesario:
- release noise por cuerda o por familia de cuerdas;
- stop noise / choke;
- varias variantes para evitar efecto de repetición.

Arquitectura futura:
```text
NOTE ON
→ voz principal

NOTE OFF / cambio / choke
→ release layer opcional
```

## 5. Fret noise, pick noise, pick scratch y ruidos mecánicos

Los instrumentos estudiados tratan estos sonidos como capas independientes y,
en varios casos, con múltiples variantes.

Recurso necesario:
- fret noise;
- pick noise;
- pick scratch;
- ruido de cambio de posición;
- stop noise;
- body/percussive noise.

Uso futuro:
- disparo probabilístico o contextual;
- intensidad baja;
- nunca en todos los eventos;
- posibilidad de desactivar estas capas.

## 6. Ghost strum / brush / dead note

Para un acompañamiento convincente hacen falta golpes que no sean acordes
completos ni palm mute. Algunas bibliotecas usan articulaciones específicas de
`Brush`, dead notes y golpes rítmicos cortos.

Recurso necesario:
- varias tomas de brush/ghost;
- dead strum / CHAK;
- preferentemente grabadas con la misma guitarra del banco principal.

Mientras tanto:
- `TREBLE_3`, `TREBLE_4`, gate corto y menor velocity pueden aproximar el
  gesto, pero no sustituyen una grabación real.

## 7. Capas dinámicas Iowa pp / mf / ff

Esto **sí se puede implementar más adelante con recursos ya disponibles**,
pero requiere integrar y validar las tres capas completas.

Modelo previsto:
```text
velocity baja  → pp
velocity media → mf
velocity alta  → ff
```

Dentro de cada capa seguirá habiendo una sola variante mientras no exista
round robin real.

Antes de activarlo:
- verificar que las tres capas estén completas y correctamente nombradas;
- normalizar diferencias extremas de nivel sin destruir el carácter de cada
  capa;
- medir memoria y tiempo de carga en Android.

## 8. Legato, slide, bend y articulaciones paramétricas

Unreal demuestra que algunas articulaciones pueden reutilizar una muestra base
cambiando `offset`, attack, release, pitch envelope y otros parámetros.

Parte de esta idea puede prototiparse sin nuevas grabaciones, pero no debe
confundirse con una articulación real.

Pendiente experimental:
- `LEGATO_SIM` con offset + attack suave;
- release variable;
- slide/bend sintético únicamente como laboratorio.

Criterio:
- si el efecto resulta artificial, se conserva solo como referencia técnica y
  se espera a disponer de muestras específicas.

## 9. Banco propio futuro

Objetivo a largo plazo cuando el proyecto genere ingresos o consiga apoyo:

- guitarra nylon principal;
- mismas cuerdas y digitaciones que usa el motor;
- `pp / mf / ff`;
- 2–4 round robins por capa;
- DOWN / UP;
- palm mute;
- dead/ghost strum;
- release/stop noise;
- fret noise;
- body hits;
- grabación consistente de principio a fin.

Esta biblioteca permitiría aprovechar completamente la arquitectura que ya se
está preparando en GuitarPilot sin volver a diseñar el motor.


# v0.11 — Rhythm menu + pulse experiments

Antes de esta versión, v0.10 Humanization Lab fue promovida a `main` como
primera base estable probada en dispositivo.

## Interfaz de ritmos

Se conserva todo el catálogo existente, pero la pantalla deja de mostrar un
botón por cada patrón.

Nueva UI:
- `ELEGIR RITMO ▼`: abre un menú con todos los patrones.
- acceso rápido 1: `ZONAS HUMAN`.
- acceso rápido 2: `PIMAMI`.

Los accesos rápidos son provisionales y podrán hacerse configurables.

## ROCK PULSE 01

Referencia verbal de diseño: `BON CHA-CA BON CHA-CA`.

Estructura física:
- `BON`: bajo raíz.
- `CHA-CA`: dos rasgueos rápidos en `TREBLE_3`.
- se usa la capa de humanización de v0.10.
- BPM inicial de prueba: 100.

La intención es construir un patrón rock genérico de bajo + respuesta aguda,
no reproducir un arreglo específico de una canción.

## BALLAD PULSE 01

Referencia verbal: `NO  NO  ahora no  ahora no`.

Estructura:
- dos bajos separados al inicio;
- durante ambos `ahora no` se omiten las cuerdas 5 y 6;
- los rasgueos llegan como máximo a cuerda 4 mediante `TREBLE_4`;
- cada frase aguda alterna UP/DOWN;
- BPM inicial de prueba: 90.

La frase verbal se conserva en documentación porque ayuda a relacionar la
sensación rítmica con la implementación técnica.

## Estado de ramas

- `main`: v0.10 Humanization Lab, probada y promovida.
- `testing`: v0.11 y siguientes experimentos.


## v0.11 — ajuste de cambio de acorde hacia cuerdas muteadas

Se corrigió una posible causa del comportamiento extraño del bajo de C.

Caso típico:
- G o Em dejan resonando la 6.ª cuerda.
- Al cambiar a C = `x32010`, esa cuerda debe desaparecer.
- Antes, el motor cancelaba solo voces futuras y permitía que la cola activa
  siguiera sonando debajo del nuevo acorde.

Nuevo comportamiento:
- si una cuerda pasa a `fret < 0` en el nuevo acorde, la voz activa entra en
  un release corto de 20 ms;
- si la cuerda sigue permitida pero cambia de traste, se conserva la lógica
  anterior: solo se cancelan voces futuras y la cola activa termina de forma
  natural.

Objetivo de la prueba:
- comparar G→C y Em→C;
- escuchar si desaparece el grave residual bajo C;
- confirmar que no reaparece la pausa audible entre cambios de acorde.


## v0.11 — reajuste de los dos ritmos nuevos tras prueba auditiva

La primera implementación de los patrones 12 y 13 no reprodujo correctamente
la sensación buscada. Se reescribieron siguiendo la descripción física de la
mano en lugar de la primera aproximación verbal.

### Pattern 12 — Rock Pulse 01
Nuevo ciclo:
- rasgueo UP solo en cuerdas 3,2,1;
- inmediatamente rasgueo DOWN solo en cuerdas 3,2,1;
- inmediatamente bajo raíz;
- una semicorchea de espera;
- repetir.

Referencia verbal interna:
`↑ ↓ BAJO · | ↑ ↓ BAJO ·`

### Pattern 13 — Latin Ballad 01
Referencia de prueba: acompañamiento asociado a "Hojas Secas".

- conserva los eventos de bajo separados;
- los golpes agudos usan `TREBLE_4`, por lo que omiten cuerdas 5 y 6;
- la dirección de los agudos alterna empezando DOWN:
  `DOWN / UP / DOWN / UP...`;
- la alternancia de dirección afecta solo a los agudos.


# v0.12 — Biblioteca de acordes + seis accesos configurables

La pantalla conserva seis botones de acorde visibles, pero dejan de estar
fijados a seis acordes concretos.

## Interacción

- toque corto sobre un botón: selecciona/toca el acorde asignado;
- segundo toque sobre el acorde actualmente activo: vuelve a OPEN;
- mantener pulsado un botón: abre el selector para reasignar ese botón;
- las seis asignaciones sobreviven a rotaciones/recreaciones mientras el
  proceso de la app siga activo.

Valores iniciales:
`C · G · D · Em · Am · Bm`.

## Biblioteca inicial

Se añadieron 37 estados de acorde:

- OPEN;
- 12 mayores: C, C#, D, D#, E, F, F#, G, G#, A, A#, B;
- 12 menores: Cm, C#m, Dm, D#m, Em, Fm, F#m, Gm, G#m, Am, A#m, Bm;
- 12 séptimas dominantes: C7, C#7, D7, D#7, E7, F7, F#7, G7, G#7, A7, A#7, B7.

El selector temporal usa submenús `Mayores / Menores / Séptimas`. La estética
del selector no se considera definitiva; en esta etapa importa validar la
funcionalidad y las digitaciones.

## Digitaciones

El motor usa posiciones abiertas o cejillas comunes, siempre como seis valores
ordenados de cuerda 6 a cuerda 1. Cada acorde conserva además bajo raíz y bajo
alternativo para los patrones que los necesiten.

Todos los trastes utilizados por esta biblioteca quedan dentro del rango del
banco MF de Iowa ya cargado.


## v0.12 — bloqueo de reasignación durante reproducción

Mientras el transporte está en PLAY, mantener pulsado un botón de acorde ya
no permite cambiar qué acorde está asignado a ese botón.

Importante:
- el toque corto sigue cambiando de acorde durante la reproducción, porque ese
  es el comportamiento central de GuitarPilot;
- lo único bloqueado durante PLAY es la **reasignación de los seis accesos
  rápidos**;
- para reasignar un botón hay que pausar o detener la reproducción.


# v0.13 — Ritmos JSON editables en tiempo de ejecución

La rama testing incorpora la primera versión del motor de ritmos externos.

## Objetivo

Crear, importar, ajustar, escuchar y exportar un ritmo sin recompilar la APK.

Flujo:
```text
JSON
→ importar / editar
→ APLICAR
→ PROBAR
→ ajustar offset de eventos
→ PROBAR otra vez
→ EXPORTAR
```

## Formato JSON v1

Campos principales:
- `schemaVersion`
- `id`
- `name`
- `category`
- `defaultBpm`
- `timeSignature`
- `ticksPerBar`
- `events[]`

Eventos soportados inicialmente:
- `STRUM_DOWN`
- `STRUM_UP`
- `ROOT_BASS`
- `ALT_BASS`
- `PLUCK_STRING`

Targets:
- `ALL_ALLOWED`
- `BASS_3`
- `TREBLE_3`
- `TREBLE_4`
- `ROOT_BASS`

Cada evento puede definir:
- `tick`
- `offsetMs` entre -100 y +100 ms
- `velocity`
- `spreadMs`
- `layer` = MF o PALM_MUTE
- `humanize`
- `stringNumber` para PLUCK_STRING

## Editor inicial

La pantalla principal añade `EDITOR / IMPORTAR JSON`.

El editor permite:
- importar un archivo JSON mediante el selector de Android;
- exportarlo;
- aplicar el JSON al motor;
- probarlo inmediatamente;
- recorrer eventos;
- mover el evento seleccionado -10, -2, +2 o +10 ms;
- volver a reproducir sin recompilar.

Como semilla se incluye `assets/rhythms/zonas_human.json`, una versión en
datos del patrón Zonas Human.

## Nota de arquitectura

Los ritmos heredados siguen presentes en C++ durante la transición. Elegir uno
de ellos desactiva el patrón JSON activo. El objetivo posterior es migrarlos
gradualmente al formato JSON una vez validemos que el runtime reproduce
correctamente los patrones.
