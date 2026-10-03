# GuitarPilot — Contexto de desarrollo para Codex

## Propósito de este archivo

Este documento resume el estado técnico, decisiones de producto, arquitectura,
reglas musicales y pendientes de GuitarPilot para que Codex pueda trabajar
sobre el repositorio local sin perder el contexto acumulado.

**Regla de ramas**
- Trabajar normalmente en `testing`.
- No modificar `main` sin autorización explícita.
- `main` se usa como base estable.
- `testing` es la rama donde se permiten refactors, experimentos y cambios que
  pueden romper temporalmente el proyecto.

---

# 1. Visión del producto

GuitarPilot es una app Android de acompañamiento e interpretación en vivo para
guitarra.

Idea central:

```text
La app mantiene el ritmo.
El músico controla la armonía.
```

El usuario elige manualmente los acordes en tiempo real. La app **NO debe
avanzar acordes automáticamente**.

Frase de producto útil:

```text
Tú elige los acordes. Yo toco la guitarra.
```

Casos de uso:
- acompañarse cantando;
- componer sin guitarra física;
- practicar progresiones;
- crear ideas en transporte público con auriculares;
- tocar con una sola mano;
- usar ritmos importables/exportables;
- eventualmente compartir ritmos entre usuarios.

---

# 2. Reglas musicales que NO deben romperse

1. La app mantiene el ritmo, el usuario cambia los acordes.
2. Nunca auto-avanzar acordes.
3. Cambiar acorde no debe reiniciar el transporte.
4. Las muestras de audio deben poder solaparse naturalmente.
5. Un nuevo rasgueo no debe cortar la cola del anterior, salvo que exista una
   razón física específica como una cuerda que pasa a quedar muteada.
6. El patrón se repite indefinidamente hasta PAUSE o STOP.
7. PAUSE deja de programar nuevos golpes pero permite que las colas terminen.
8. STOP detiene la programación del patrón; la política exacta sobre cortar
   colas sigue siendo refinable.
9. El acorde define qué cuerdas son físicamente legales.
10. El evento rítmico define qué zona de cuerdas quiere tocar.
11. La máscara real es la intersección entre forma del acorde y zona rítmica.
12. El bajo raíz debe resolver la cuerda/traste real del acorde, no un simple
    subconjunto aproximado.
13. OPEN es un estado válido.
14. Segundo toque sobre el acorde actualmente activo vuelve a OPEN.
15. El rasgueo manual debe existir independientemente del transporte automático.
16. El rasgueo manual no debe reiniciar ni desplazar el ritmo automático.
17. Preferir sonidos grabados reales frente a articulaciones sintéticas.
18. Palm mute digital/sintético es plan B.
19. Pocos ritmos oficiales muy buenos son preferibles a una biblioteca enorme
    mediocre.
20. Debe existir al menos un arpegio/picking bueno desde las primeras versiones.

---

# 3. Stack actual

Proyecto Android nativo.

- Kotlin: UI y glue Android.
- C++: motor de audio.
- Oboe: salida de audio de baja latencia.
- JNI: puente Kotlin ↔ C++.
- Gradle/CMake/NDK.
- ABI principal de prueba: `arm64-v8a`.

Repositorio:

```text
https://github.com/actnada/Guitarpilot
```

Ruta local habitual:

```text
C:\Users\admin\AndroidStudioProjects\GuitarPilotRepo
```

Namespace Kotlin/C++ JNI:

```text
com.example.guitarpilot
```

Application ID:

```text
com.example.guitarpilot2
```

---

# 4. Arquitectura de audio actual

Flujo:

```text
Kotlin UI
→ JNI
→ GuitarEngine C++
→ mixer propio
→ un stream Oboe
→ salida estéreo
```

Características:
- float stereo output;
- resampling lineal simple;
- voces polifónicas;
- máximo aproximado actual: 128 voces;
- las voces pueden solaparse;
- cada cuerda puede tener una voz activa y colas;
- cambio de acorde no reinicia el ritmo;
- transporte y acordes están conceptualmente separados.

Archivos principales:

```text
app/src/main/cpp/GuitarEngine.cpp
app/src/main/cpp/GuitarEngine.h
app/src/main/cpp/native-lib.cpp

app/src/main/java/com/example/guitarpilot/MainActivity.kt
app/src/main/java/com/example/guitarpilot/NativeAudioEngine.kt
app/src/main/java/com/example/guitarpilot/RhythmJson.kt
app/src/main/java/com/example/guitarpilot/RhythmEditorActivity.kt
```

---

# 5. Banco de audio principal

Fuente principal actual:
- University of Iowa Musical Instrument Samples.
- Se prioriza identidad física de cuerda.
- El mismo pitch tocado en otra cuerda puede sonar distinto.

Capas Iowa conocidas:
- pp
- mf
- ff

IMPORTANTE:
- `pp/mf/ff` son capas de dinámica, NO round robin.
- No usarlas como variantes aleatorias de la misma intensidad.

En el proyecto actual:
- capa MF principal cargada;
- 118 muestras MF;
- 17 Palm Mute de MicRos;
- `kSampleCapacity = 135`.

Preferencia histórica del usuario:
- FF sonaba mucho mejor que MF en pruebas previas.
- Si se retoma FF, preferir controlar volumen en motor/UI antes que forzar MF a
  sonar más fuerte.

---

# 6. Articulaciones y recursos futuros

Pendientes por falta de recursos de audio:
- round robin real;
- ataques DOWN/UP grabados por separado;
- palm mute coherente con guitarra nylon;
- release samples;
- stop noise;
- fret noise;
- pick noise;
- pick scratch;
- body hits;
- ghost/dead/brush strums;
- articulaciones de slide/bend más naturales.

No inventar estas capas si no hay recurso sonoro adecuado.

Sí se pueden experimentar:
- offset;
- attack;
- release;
- pitch envelope sintético;
- variación de gain;
- variación de spread;
- microtiming.

---

# 7. String targets

En C++ existen actualmente:

```text
ALL_ALLOWED
BASS_3
TREBLE_3
TREBLE_4
ROOT_BASS
```

Interpretación:
- ALL_ALLOWED: todas las cuerdas permitidas por la digitación.
- BASS_3: físicas 6,5,4.
- TREBLE_3: físicas 3,2,1.
- TREBLE_4: físicas 4,3,2,1.
- ROOT_BASS: una sola cuerda/traste raíz declarada por el acorde.

Esto es importante para construir ritmos realistas y evitar barrer seis cuerdas
cuando el gesto real usa solo parte del acorde.

---

# 8. Acordes

La librería se amplió a 37 estados:
- OPEN;
- 12 mayores;
- 12 menores;
- 12 séptimas dominantes.

IDs aproximados actuales:

```text
0 OPEN

1..12  mayores
13..24 menores
25..36 séptimas
```

Los acordes están definidos en `GuitarEngine.h`.

Actualmente hay 6 botones visibles configurables.

Por defecto:

```text
C   G   D
Em  Am  Bm
```

Interacción:
- toque corto: selecciona/toca acorde;
- segundo toque sobre acorde activo: vuelve a OPEN;
- mantener pulsado: reasigna el acorde del botón;
- durante PLAY, la reasignación de botones está bloqueada;
- durante PLAY sí se puede cambiar de acorde normalmente con toque corto.

No romper este comportamiento.

---

# 9. Problema conocido del bajo de C

C está definido como digitación física tipo:

```text
x32010
```

Bajo raíz:
- cuerda 5, traste 3 = C3.

Problema observado:
- el bajo de C sonaba extraño en algunos cambios.

Arreglo ya implementado:
- si una cuerda pasa de activa a muteada al cambiar de acorde, se aplica un
  release corto (~20 ms) para evitar que una cuerda vieja grave siga sonando
  debajo del nuevo acorde.

Caso típico:
- G/Em → C
- 6ª cuerda vieja debe desaparecer.

Seguir observando este comportamiento.

---

# 10. Ritmos heredados

Históricamente los ritmos estuvieron hardcodeados en:

```text
GuitarEngine.cpp
```

principalmente dentro de:

```cpp
void GuitarEngine::triggerPatternEvent(...)
```

Había varios patrones con IDs 0..13, incluyendo:
- Pop/Rock
- Balada Rock
- PIMAMI
- Rock Muted
- Golden 01
- Golden 02
- ChordBox Folk A/B
- ChordBox Ballad A/B
- ZONAS
- ZONAS HUMAN
- Rock Pulse
- Latin Ballad

Problema:
- tocar un ritmo era propenso a romper otros;
- timing difícil de afinar;
- cambios exigían recompilar;
- algunos ritmos recientes quedaron muy fuera de tiempo.

Por eso se decidió migrar a JSON.

---

# 11. Sistema JSON de ritmos — dirección actual

Objetivo:

```text
crear/editar JSON
→ importar
→ aplicar
→ probar
→ ajustar tiempos
→ probar otra vez
→ exportar
```

sin recompilar.

Formato inicial:

```json
{
  "schemaVersion": 1,
  "id": "example",
  "name": "Example",
  "category": "custom",
  "defaultBpm": 100,
  "timeSignature": "4/4",
  "ticksPerBar": 16,
  "events": []
}
```

Eventos inicialmente soportados:
- STRUM_DOWN
- STRUM_UP
- ROOT_BASS
- ALT_BASS
- PLUCK_STRING

Campos por evento:
- tick
- offsetMs
- type
- target
- velocity
- spreadMs
- layer
- humanize
- stringNumber (cuando aplica)

Rangos actuales:
- offsetMs: -100..+100
- velocity: 0..1
- spreadMs: 0..60
- ticksPerBar: 1..64
- eventos: 1..128

Archivo semilla:

```text
app/src/main/assets/rhythms/zonas_human.json
```

---

# 12. Editor de ritmos dentro de la app

Pantalla:

```text
RhythmEditorActivity
```

Permite actualmente:
- editar JSON directamente;
- importar .json;
- exportar .json;
- aplicar al motor;
- probar inmediatamente;
- recorrer eventos;
- ajustar microtiming del evento seleccionado:
  -10 ms
  -2 ms
  +2 ms
  +10 ms

Meta:
- usar el editor como herramienta de desarrollo y luego como base del editor de
  ritmos para usuarios.

No hace falta recompilar para afinar un JSON ya importado.

---

# 13. Timing y forma de trabajar los ritmos

El usuario toca guitarra de oído y no estudió teoría formal.

Las referencias verbales funcionan muy bien, por ejemplo:

```text
"chocolate"
"bon chaca bon"
"NO NO ahora no ahora no"
```

Estas frases son útiles para describir sensación, acentos y agrupaciones.

Pero:
- no usarlas como sustituto del timing real;
- cuando haya video/audio de referencia, medir tiempos reales;
- usar la frase verbal como mapa perceptivo;
- usar audio medido para colocar ticks y offsets.

Arquitectura deseada:
- cuadrícula para estructura;
- `offsetMs` para microtiming real.

---

# 14. Ritmos planeados / referencias

Se habló de:
- bolero;
- balada rítmica "chocolate";
- rock con bajo + agudas;
- patrón asociado a Hojas Secas;
- patrón asociado a Apuesta por el Rock and Roll;
- PIMAMI;
- uno o más arpegios adicionales.

Importante:
- no nombrar los patrones oficiales con nombres de canciones.
- usar referencias internas para estudiar el gesto.
- publicar nombres genéricos como `Rock Pulse 01`, `Latin Ballad 01`, etc.

---

# 15. Humanización

ZONAS HUMAN fue una mejora notable.

Ideas actuales:
- microtiming local;
- ligera variación de gain;
- variación de spread;
- scheduler principal estable;
- no acumular deriva de tempo.

La humanización debe afectar interpretación, no destruir el reloj.

---

# 16. Manual strum

Debe existir:
- rasgueo manual DOWN;
- rasgueo manual UP.

Uso:
- iniciar canción con acorde sostenido;
- cortes;
- finales;
- cambios rápidos en sílabas;
- golpes expresivos.

El manual strum:
- no reinicia el ritmo;
- no desplaza transport;
- puede coexistir con patrón automático.

---

# 17. Bug actual importante: volumen bajo

Issue GitHub:

```text
#2 Audio output much quieter in app than source Iowa samples
```

Síntoma:
- los WAV de Iowa suenan bien fuera de GuitarPilot;
- dentro de la app el volumen es mucho más bajo;
- al grabar la app en video, el audio del video luego se oye más fuerte que el
  sonido en vivo de la app.

Sospecha principal:
- acumulación de atenuación:
  - event velocity
  - per-string falloff
  - humanization
  - master volume

Ejemplo:
```text
0.56 velocity × 0.85 master ≈ 0.48
```

antes incluso de otras reducciones.

También investigar:
- Oboe `Usage::Game` vs `Usage::Media`;
- procesamiento del dispositivo;
- diferencia entre captura digital y altavoz físico;
- RMS/peak antes y después del mixer.

NO normalizar ni regrabar WAVs todavía.

---

# 18. Modo diagnóstico de volumen

Ya se añadió a `testing`.

Botón:

```text
TEST AUDIO · IOWA E4 100%
```

Reproduce:
- Iowa E4;
- cuerda 1 al aire;
- MF;
- voice gain = 1.0;
- sin velocity de ritmo;
- sin per-string falloff;
- sin humanización;
- bypass del master de GuitarPilot.

Sí atraviesa:
- sample;
- resampler;
- mixer;
- stream Oboe.

Interpretación:
- si suena igual que el WAV externo → el problema está en nuestra cadena de
  gain del acompañamiento;
- si sigue bajo → investigar ruta Oboe / Android / dispositivo.

---

# 19. Oboe

Configuración relevante actual:
- PerformanceMode::LowLatency
- intenta Exclusive;
- fallback a Shared;
- Float;
- Stereo;
- Usage::Game;
- ContentType::Music.

Se debe considerar testear `Usage::Media` si el diagnóstico de volumen sigue
bajo.

No cambiar esto sin medir antes.

---

# 20. Muestras y mezcla

Conversión PCM16 → float:

```cpp
sample.pcm[i] =
    static_cast<float>(data[i]) *
    (1.0f / 32768.0f);
```

Cada voz aplica:
- sample
- voice gain
- release gain

Luego el mix normal aplica master.

Actualmente hay clamp final:

```cpp
mix = std::clamp(mix, -1.0f, 1.0f);
```

No hay todavía:
- compressor;
- limiter;
- loudness compensation.

No añadirlos a ciegas. Primero medir.

---

# 21. Flujo Git recomendado

Antes de trabajar:

```powershell
git checkout testing
git pull
git status
```

Trabajar localmente.

Antes de cambios grandes:
- revisar `README.md`;
- revisar `GUITARPILOT_CONTEXT.md`;
- revisar `git status`.

Después de una mejora probada:
- commit descriptivo;
- push a `testing`.

Solo pasar a `main` después de una prueba exitosa y autorización explícita.

---

# 22. Reglas para Codex

Cuando trabajes en este repo:

1. No tocar `main` sin autorización.
2. Preferir cambios pequeños y verificables.
3. Antes de editar un archivo, leer la versión actual.
4. Compilar después de cambios de C++/JNI/Kotlin.
5. No asumir que una idea musical es correcta sin prueba auditiva.
6. No modificar samples originales.
7. No normalizar archivos fuente a ciegas.
8. No hacer auto chord progression.
9. No reiniciar transporte al cambiar acordes.
10. Preservar solapamiento natural de voces.
11. Evitar cortar colas salvo que haya razón física.
12. Mantener separación entre:
    - transport;
    - rhythm;
    - musician/manual chord/strum.
13. Para ritmos nuevos, preferir JSON antes que hardcode C++.
14. Si un ritmo necesita pequeños ajustes temporales, usar `offsetMs`.
15. Documentar bugs y decisiones importantes.
16. Si una refactorización rompe `testing`, se puede corregir ahí; no usar
    `main` como laboratorio.

---

# 23. Filosofía de desarrollo

No reinventar la rueda.

Se pueden estudiar proyectos externos aunque su licencia no permita reutilizar
código comercialmente, siempre que se use para:
- entender arquitectura;
- estudiar solución;
- hacer ingeniería inversa conceptual;
- implementar una solución propia.

Antes de copiar código o assets:
- revisar licencia;
- revisar atribución;
- revisar si los assets tienen una licencia separada.

Proyectos/referencias estudiados o mencionados:
- Google Oboe
- tonejs-instruments
- Muted.io guitar fretboard
- Wavebase
- MWEngine
- DrumPadMachine con Oboe
- dylanburati/Sampler
- ChordBox
- guitar-chords-db-json
- Aubio Tuner Android
- ako/backing-tracks
- keystrum
- PauSala/muzik
- cadence
- GarageBand Smart Guitar
- Unreal Instruments / SFZ banks

---

# 24. Referencias técnicas importantes observadas

De bancos SFZ:
- trigger=release;
- offset;
- offset_random;
- amp_random;
- seq_length;
- round robin;
- separaciones de DOWN/UP;
- sustain/mute/release como capas independientes;
- slides construidos con pitch envelopes y offsets.

Conclusión:
la arquitectura futura debería poder contemplar:
- articulation
- pickDirection
- pitchEnvelope
- startOffset
- attack
- release
- variant

No hace falta implementar todo ahora.

---

# 25. Monetización y ecosistema

Plan acordado:
- Pro Pass de pago único: $14.99 USD.
- Usuario gratis puede desbloquear Pro temporalmente viendo un anuncio
  bonificado.
- Cada anuncio bonificado da 2 horas de Pro.
- No interrumpir sesión creativa/performance con anuncios.

Futuro:
- usuarios pueden crear y compartir ritmos;
- la comunidad puede nominar ritmos;
- GuitarPilot puede seleccionar ritmos para biblioteca oficial;
- contactar al creador;
- pedir permiso;
- dar crédito;
- posiblemente compensar económicamente según valor/calidad/ingresos.

No fijar una cantidad todavía.

---

# 26. Roadmap inmediato sugerido

Prioridad actual:

1. Compilar y validar el sistema JSON/editor.
2. Comparar `ZONAS HUMAN` heredado vs `ZONAS HUMAN JSON`.
3. Confirmar que JSON reproduce igual sin recompilar.
4. Probar microtiming desde el editor.
5. Resolver bug de volumen con TEST AUDIO.
6. Si el test sigue bajo, comparar Oboe Usage::Game vs Usage::Media.
7. Migrar progresivamente ritmos heredados a JSON.
8. Crear/afinar:
   - bolero;
   - chocolate;
   - rock pulse;
   - latin ballad;
   - nuevos arpegios.
9. Rediseñar después la UI de selector de ritmos y acordes.
10. Convertir el editor JSON técnico en editor visual más amigable.

---

# 27. Filosofía de UI

Funcionalidad primero.

Problemas actuales:
- el menú desplegable de ritmos no gusta mucho;
- la selección de acordes también es funcional pero visualmente provisional.

No invertir demasiado tiempo en estética hasta validar:
- JSON;
- editor;
- timing;
- audio;
- acordes;
- estabilidad.

Meta futura:
- 6 acordes rápidos visibles;
- configurables;
- pocos ritmos rápidos visibles;
- selector extendido mejor diseñado;
- editor visual;
- controles grandes y aptos para una mano.

---

# 28. Dispositivo de prueba y entorno

PC conocida:
- Intel Core i5-4590S @ 3.0 GHz
- 8 GB RAM
- Intel HD Graphics 4600
- Windows 64-bit

Recomendación:
- evitar emulador Android;
- usar dispositivo físico;
- mantener pocas apps abiertas;
- compilar solo ABI necesario.

Android de prueba histórico:
- teléfono físico conectado por ADB;
- arm64-v8a.

---

# 29. Errores históricos relevantes

Hubo un problema con Ninja/CMake:

```text
manifest 'build.ninja' still dirty after 100 tries
```

Causa encontrada:
- archivos C++ con timestamp futuro respecto al reloj del PC.

Solución:
- corregir LastWriteTime;
- detener Gradle;
- borrar `.cxx` y `build`;
- clean;
- recompilar.

Si vuelve a aparecer, revisar timestamps antes de asumir que CMake está roto.

---

# 30. Principio central

Cuando haya duda entre una implementación muy sofisticada y una prueba simple:

```text
medir
→ probar
→ escuchar
→ ajustar
```

No adivinar comportamiento musical si puede verificarse con audio real.

El objetivo de GuitarPilot no es demostrar teoría musical perfecta, sino sentirse
como una guitarra de acompañamiento convincente, inmediata y expresiva.
