# Corrección de continuidad de acordes basada en rama testing

## Comparación de código verificada

Se comparó `GuitarEngine.cpp` de `testing` y `version-b` antes de ajustar el comportamiento.

- **testing / applyPendingChordChange**: si cambia el traste, `newFret < 0` llama a `releaseString(cuerda, 20)` y silencia la cuerda nueva no permitida. Si `newFret >= 0`, llama a `cancelFutureVoicesOnString(cuerda)`: cancela ataques futuros pero **no apaga la nota ya sonando**.
- **version-b anterior**: ejecutaba `releaseString(cuerda, 20)` indiscriminadamente para cualquier traste cambiado. Eso producía caídas de volumen o silencio entre acordes.
- **Adaptación a version-b**: `cancelFutureVoicesOnString` se sustituye por `cancelPendingAttacksOnString`, que actúa sobre el heap `mPendingAttacks` introducido en version-b. La misma política conserva la voz activa de la cuerda permitida y evita futuros ataques con notas antiguas. Si el acorde nuevo prohíbe una cuerda, `releaseString` conserva fade de 20 ms y cancelación de pendientes.

## PIMAMI

**Se conserva el comportamiento de testing sin modificarlo**: el cambio de acorde solicitado queda pendiente y se aplica al comenzar el siguiente ciclo en el evento **P** (pulgar/bajo). Las notas I-M-A-M-I previas completan la frase de su acorde original; el contador y el tempo no se reinician. Esto evita una transición parcial de PIMAMI que la rama testing no realizaba.

## JSON y muestras

- Los patrones JSON y sus offsets no cambian.
- Para cuerdas cuyo traste cambia pero siguen permitidas, una muestra ya activa sigue resonando hasta que termine o un nuevo ataque la reemplace (crossfade por cuerda de 5 ms).
- Para cuerdas que pasan a muteadas se aplica fade de 20 ms.
- Se mantienen `RELEASE_TARGET`, los silencios deliberados del patrón y la máscara de cuerdas.
- Los cambios no alteran los 49 acordes (48 + OPEN) ni las seis asignaciones configurables.

## Regresiones añadidas

- `chordChangePreservesPlayingPitchWhenStringRemainsAllowed` comprueba que el volumen no se corta y que se cancelan únicamente ataques futuros con una nota obsoleta.
- `pimamiSwitchesChordAtNextPWithoutRestarting` comprueba que PIMAMI termina P-I-M-A-M-I con el acorde anterior y activa el siguiente en P sin desplazar el reloj.
- `liveChordChangeKeepsAudioUntilNextNote` comprueba que la cola mantiene amplitud antes del siguiente ataque.
- Se conservan los tests de cuerdas muteadas, cancelación de pendientes, sample 7m y patrones JSON.

Pendiente: ejecutar las regresiones nativas de host y compilar/escuchar en Android para verificar el resultado sonoro.
