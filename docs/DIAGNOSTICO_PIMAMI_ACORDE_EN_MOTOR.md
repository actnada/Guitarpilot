# Instrumentación para diagnosticar cambios de acorde en PIMAMI

## Reporte
El teléfono puede mostrar el acorde elegido en la interfaz mientras el audio de PIMAMI sigue sonando con el acorde anterior.

## Comportamiento musical preservado
El motor sigue aplicando el nuevo acorde **solamente al comienzo del próximo evento P (bajo)**. No cambia la estructura P-I-M-A-M-I, no reinicia el reloj y conserva el apagado selectivo de cuerdas de la política `testing`. Los ritmos JSON no se modifican.

## Datos del motor expuestos
El método nativo `getChordDebugState` devuelve cinco enteros, leídos desde campos atómicos:
1. ID solicitado desde la UI (`mChord`).
2. ID efectivamente aplicado por el hilo de audio (`mPublishedAppliedChord`).
3. ID del acorde que estaba activo en el último evento P.
4. Número acumulado de eventos P desde que se creó el motor.
5. Índice de muestra de bajo seleccionada en ese P (`-1` si no hay muestra).

Estos datos se actualizan desde el callback sin hacer `Log`, bloqueos ni asignaciones de memoria. La interfaz los consulta cada 350 ms solo mientras la Activity está visible. El texto solo se muestra al reproducir PIMAMI.

## Cómo interpretar la prueba
- **Solicitado cambia, sonando no cambia tras dos o más P**: problema al aplicar el acorde o con el transporte; revisar `mChord`, `mAppliedChord` y cuenta de eventos P.
- **Solicitado y sonando coinciden, sample = -1**: la digitación se aplicó pero falta la grabación del bajo elegido.
- **Solicitado y sonando coinciden, sample válido**: revisar las notas efectivas de las cuerdas 3, 2 y 1, la ruta de muestras y el mezclador si el oído sigue percibiendo el mismo acorde.
- **Cuenta de P no aumenta**: PIMAMI no está recorriendo los ciclos nativos esperados.

No se ha diagnosticado todavía en el dispositivo del usuario. Las pruebas host aún deben ejecutarse. Esta instrumentación no afirma que el audio esté corregido; da evidencia para identificar la causa y aplicar una corrección precisa.
