# Phantom Asym (sidechain only) v1.2.0

Silencia el audio que entra, en full range, siguiendo los cycles de la señal que le mandes por el **sidechain input**.

## Controles

| Control | Qué hace |
|---|---|
| **Depth** | Máxima reducción del audio, que se alcanza cuando el sidechain llega a 0 dBFS (100 % = silence total) |
| **Mode** | **1 cycle yes / 1 no**: silencia un cycle completo, deja pasar el siguiente, y así alternando. **Half-wave**: silencia un solo semiciclo de cada cycle |
| **First Cycle** | En cada golpe, si el primer cycle se silencia (**Silence**) o pasa (**Pass**) |
| **Polarity** | Dónde empieza cada cycle: semiciclo positivo o negativo |
| **Output** | Volumen final |

## Cómo usarlo

1. Poné Phantom Asym en la pista (o el master) que querés afectar.
2. Mandá al **sidechain input** una señal con solo el **sub** (por ejemplo, un kick o un 808 filtrado). El plugin no tiene filtros internos: si le mandás una señal con medios y agudos, va a detectar cycles de más. Filtrá el low end en la pista que mandás al sidechain.
3. Sin señal en el sidechain, el audio pasa sin cambios.

## Cómo se calcula la cantidad de efecto

La reducción **sigue el level del sidechain**: a 0 dBFS es igual a **Depth**, y a medida que el level baja (por ejemplo cuando el kick decae) el efecto baja de forma continua hasta llegar a cero en -inf dB. No hay threshold que lo corte de golpe.

- Dentro de cada cycle, el valor sale del **peak** de ese cycle del sidechain.
- La relación es lineal en amplitud: un sidechain con peak a -6 dBFS da la mitad del Depth, y a -12 dBFS un cuarto. Para más efecto, subí el level de la señal que mandás al sidechain.

## Notas

- Solo atenúa, nunca amplifica. No tiene latency ni filtros en el camino del audio.
- El sidechain se detecta en mono (promedio de L y R) y la misma bajada se aplica a todos los canales.
- Cuando el sidechain se apaga (entre golpes), el contador se reinicia y cada golpe arranca igual.

## Compilar e instalar

1. Reemplazá los archivos de tu carpeta del proyecto por los de este zip.
2. En GitHub Desktop: escribí un Summary, **Commit to main**, **Push origin**.
3. Esperá la compilación en **Actions**, descargá el artifact de Windows o Mac.
4. Mac: `.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `.component` a `~/Library/Audio/Plug-Ins/Components/`, después `xattr -cr` sobre cada uno.
   Windows: `.vst3` a `C:\Program Files\Common Files\VST3\`

## Logic Pro

- El sidechain se elige en el menú **Side Chain** del header de la ventana del plugin (no dentro de los controles del plugin).
- Si no aparece: cerrá Logic, reemplazá `Phantom Asym.component`, y en Logic Pro > Settings > Plug-in Manager seleccioná Phantom Asym y tocá **Reset & Rescan Selection**.
