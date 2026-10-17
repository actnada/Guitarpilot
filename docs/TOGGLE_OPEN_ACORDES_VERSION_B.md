# Segunda pulsación de acorde → OPEN

## Regla musical
GuitarPilot es una guitarra de acompañamiento interpretada en tiempo real. Los seis botones rápidos son configurables. Una pulsación selecciona el acorde del botón y otra pulsación **del mismo acorde que está activo** selecciona OPEN, interpretado como todas las cuerdas al aire.

- G → G: OPEN
- OPEN → G: G
- G → Am: Am
- G → G → G: G, OPEN, G
- El gesto también aplica a acordes m7 y a los seis botones del editor de ritmo.

## Implementación
`ChordSession.tap(chordId)` aplica la regla del gesto y publica `NativeAudioEngine.setChord(0)` cuando debe sonar OPEN. `ChordSession.select(id)` es una operación distinta e **idempotente** para restaurar el estado después de rotaciones, navegar entre pantallas o cambiar de patrón; repetir `select` no alterna a OPEN.

No se introducen órdenes `stop`, `pause`, `play` ni cambios en `mTickIndex` o `mNextTickFrame`. El arreglo no afecta la secuencia de PIMAMI; en ese patrón las solicitudes, incluidas OPEN, se aplican en el siguiente pulgar P como antes.

## Validación
Se añade `ChordSessionGestureTest` como test JVM con cinco casos: pulsación repetida, salida de OPEN, sustitución directa, botón OPEN dedicado e ID inválido. El comportamiento auditivo, especialmente el fallo independiente de selección de acordes en PIMAMI, requiere todavía verificación en un teléfono.
