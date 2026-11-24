# NORAFO — No Sleep Pogo

Primer juego hecho enteramente con IA.

**No Sleep Pogo** es un juego de parkour de precisión en primera persona: subís a un pogo, elegís dónde caer y encadenás rebotes desde la vereda hasta el **ojo azul** pintado en la azotea. Tiene estética PS1 (low poly, texturas pixeladas, vértices que tiemblan, color de 15 bits con dithering). Está hecho en **Unreal Engine 5.8.4 con C++**. Los modelos y las animaciones se generaron con **Blender** a partir del documento de diseño *GDD Visual No Sleep Pogo (preproducción 0.2)*.

## Controles

| Acción | Teclado / mouse | Mando |
|---|---|---|
| Cargar el resorte (mantener) y saltar (soltar) | Espacio o clic izquierdo | A / gatillo derecho |
| Dirección y corrección corta en el aire | WASD o flechas | Stick izquierdo |
| Mirar | Mouse | Stick derecho |
| Volver al último punto de reaparición | R | Y |
| Pausa | Esc o P | Start |

- Sin pulsar nada, el pogo da un **rebote mínimo** en cada contacto.
- La carga máxima se alcanza en **0,45 s** (suena un clic). Si mantenés más tiempo, la carga se conserva.
- Podés empezar a mantener Espacio en el aire: la carga arranca en el siguiente contacto.
- El **anillo** muestra dónde vas a caer. Se parte en dos si apuntás fuera de un apoyo válido.

## Circuitos

1. **La cuadra** (aprender el rebote): vereda, cruce con un auto que repite su recorrido, refugio y frente comercial. Hay dos rutas hasta la azotea:
   - **Ruta principal:** 4 saltos sobre apoyos anchos.
   - **Línea alta:** 3 saltos sobre dos toldos estrechos.
2. **Galería de toldos** (enlazar saltos laterales): toldos alternados sobre un canal, con una vereda de servicio para recuperar el ritmo.
3. **Azoteas en zigzag** (anticipar la salida): cornisas con dos giros, un apoyo cercano que te deja mal orientado y un techo ancho para preparar otra línea.

Completar un circuito abre el siguiente. El juego guarda en disco, por circuito:

- el mejor tiempo,
- la marca de **pasada limpia** (0 caídas),
- la marca de **línea alta** (tocar los dos toldos marcados en orden).

El reloj arranca al primer despegue y sigue corriendo cuando reaparecés.

## Compilar y jugar

Requisitos: Unreal Engine 5.8.4 y Visual Studio 2022 con el workload de C++ para juegos.

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" NoSleepPogoEditor Win64 Development "-Project=$PWD\NoSleepPogo.uproject" -WaitMutex
```

Después abrí `NoSleepPogo.uproject` y dale a *Play*.

Para generar la build de PC (Shipping), corré lo siguiente. El ejecutable queda en `Packaged/Windows/NoSleepPogo.exe` (unos 570 MB, no se sube al repositorio):

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$PWD\NoSleepPogo.uproject" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -iostore -archive "-archivedirectory=$PWD\Packaged" -unattended
``` El mapa `L_City` abre el menú. Para entrar directo a un circuito, usá la URL `L_City?Circuit=0|1|2` (o `99` para el patio gris de prueba).

## Cómo se construyó (todo con scripts reproducibles)

| Paso | Script | Salida |
|---|---|---|
| Texturas PS1 (PIL, paleta de 15 bits + Bayer) | `Tools/textures/gen_textures.py` | `SourceArt/Textures` |
| Efectos, ambiente y música lo-fi (síntesis) | `Tools/audio/gen_audio.py` | `SourceArt/Audio` |
| Modelos low poly, esqueleto y animaciones del jinete (Blender 5.x, CLI) | `Tools/blender/build_assets.py` | `SourceArt/Models` |
| Importación a Unreal: texturas Nearest, materiales maestros PS1, mallas, animaciones y mapa | `Tools/unreal/import_assets.py` | `Content/Pogo`, `Content/Maps` |

```powershell
python -I Tools/textures/gen_textures.py SourceArt/Textures
python -I Tools/audio/gen_audio.py SourceArt/Audio
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup --python Tools/blender/build_assets.py -- SourceArt
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\NoSleepPogo.uproject" -run=pythonscript -script="$PWD\Tools\unreal\import_assets.py" -unattended
```

Los niveles se arman en C++ (`APogoCircuit`) con las medidas del GDD, usando las piezas modulares importadas.

## Código (Source/NoSleepPogo)

| Clase | Qué hace |
|---|---|
| `APogoCharacter` | Controlador del pogo: estados Listo, Contacto, Carga, Vuelo y Meta. Curvas de altura y de impulso, corrección en el aire, anillo de caída, cámara que sigue la animación con balanceo ajustable. |
| `APogoGameMode` | Reloj, caídas, reaparición, secuencia de la línea alta, meta y resultados. También arma la luz y el post-proceso PS1. |
| `APogoCircuit` | Construye los 3 circuitos y el patio gris. |
| `APogoCar` | Auto en ciclo, con zona de peligro. |
| `APogoPlayerController` y `PogoUI` | Enhanced Input creado en código, y todas las pantallas en Slate: HUD, menú, pausa, opciones y resultados. |
| `UPogoGameInstance` | Opciones y progreso local (SaveGame). |
| `APogoAutoTest` | Pruebas automáticas sin intervención humana. |

## Pruebas automáticas

```bash
Tools/run_test.sh 0 bot       # el bot juega La cuadra por la ruta principal con la física real
Tools/run_test.sh 0 bothigh   # ... por la línea alta
Tools/run_test.sh 0 shots     # capturas: primera persona, vistas del nivel, pausa, opciones, resultados
Tools/run_test.sh 99 mech     # patio gris: rebote mínimo, carga breve y carga completa (3 repeticiones)
Tools/run_test.sh menu menu   # captura del menú principal
Tools/run_test.sh menu flow   # flujo real: desde el menú entra al circuito 1 (también se probó sobre la build Shipping)
```

El bot tiene **30 s por intento**. Si no llega a la meta, se reinicia el nivel, con un máximo de 4 intentos. Las capturas y los logs quedan en `Saved/PogoTest`.

Resultados de la última corrida:

- **Mecánica:** rebote mínimo 40 cm, carga breve 82 cm, carga completa 290 cm. Las 3 repeticiones dieron lo mismo.
- **La cuadra, ruta principal:** completada sin caídas (26,4 s; 30,4 s con la versión final del bot).
- **La cuadra, línea alta:** el bot llega al toldo 1 pero no salta al toldo 2 dentro de los 30 s.
- **Galería de toldos:** completada por las dos rutas, sin caídas (27,8 s y 29,1 s).
- **Azoteas en zigzag:** el bot no llega a la meta (queda en la cornisa 2).

Los casos sin completar son limitaciones del planificador del bot. Las medidas de los niveles se mantienen tal como están.
