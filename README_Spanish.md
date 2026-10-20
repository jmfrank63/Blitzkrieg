[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Este repositorio es un proyecto personal de Johannes Maria Frank. Lo uso por diversión y para aprender, sobre todo para practicar la programación con agentes en aplicaciones heredadas (brownfield). El README original se conserva en `readme_original`.

Aviso: este proyecto está en desarrollo.


# Lo que este repositorio puede hacer hoy

- Contiene el código fuente completo del modo para un jugador de Blitzkrieg y los datos del juego.

- Compila todo el juego con `zig build` (Zig + clang) para seis plataformas — Windows (MSVC y MinGW), macOS (Intel y Apple Silicon) y Linux (x86_64 y arm64) — y genera una instalación ejecutable en `zig-out/game/<os>/<arch>/<config>`. Solo el destino MSVC necesita Visual Studio; Zig incluye su propia cadena de herramientas para los demás.

- Compila y ejecuta un Game.exe nativo de **64 bits** (`zig build install-game -Dtarget=x86_64-windows-msvc`); los destinos de 32 bits se han retirado. Las misiones de campaña y los tutoriales se pueden jugar en x64, con guardado y carga, vídeo y sonido.

- Se entrega con StreamIO.dll sustituida por completo por una implementación en Zig (`Sources/src/StreamIOZig`) — toda la capa de persistencia del motor: grafos de objetos binarios para guardar y cargar, árboles de datos XML, la fábrica de objetos, variables globales, consola y opciones.

- Compila el C++ heredado con clang y UBSan activado, lo que ha sacado a la luz y corregido decenas de errores latentes que la compilación MSVC de 2003 toleraba en silencio, incluido un fallo de llamada virtual pura de 22 años en el desmontaje de tanque y torreta.

- Valida cada destino admitido en CI y ejecuta las pruebas de plataforma de forma nativa en su propio runner — Linux x86_64 y arm64, Windows MSVC y MinGW, macOS Intel y Apple Silicon — en lugar de solo compilar de forma cruzada.

- Sigue compilando sin problemas desde la solución original `A7.sln` con las herramientas MSVC actuales (`Debug | Win32`) como referencia de comportamiento.

- Se ejecuta por defecto en una ventana real; `-fullscreen` sigue disponible como opción de línea de comandos.

- Usa submódulos de Git para las bibliotecas que faltan.

- Admite depuración nativa de C++ y con WinDbg en VS Code Insiders, además de scripts de captura de fallos sin interfaz en `tools/zig/`.

- Ofrece un flujo de trabajo auxiliar para empaquetar vídeos comprimidos en `VideoAssets/`.

- Ha eliminado el antiguo manejo de fallos BugSlay y lo ha sustituido por asserts estándar de C++.


# Conversión y validación de recursos de Blender

La cadena que sustituye a Maya40 está documentada en `docs/blender-replacement.md`.

Comandos rápidos:

- Convertir: `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Solo validar: `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Plataformas compatibles

`zig build` acepta seis destinos. Se indica uno con `-Dtarget=`; el árbol generado se organiza como `zig-out/game/<os>/<arch>/<config>`, de modo que varios destinos y ambos niveles de optimización conviven sin sobrescribirse.

| Plataforma | Triple de destino | Estado |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | Jugable |
| macOS (Apple Silicon) | `aarch64-macos` | Jugable |
| macOS (Intel) | `x86_64-macos` | Compila; sin probar jugando |
| Linux (x86_64) | `x86_64-linux-gnu` | En desarrollo |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Solo capa de plataforma |
| Windows (MinGW) | `x86_64-windows-gnu` | Solo capa de plataforma |

«Solo capa de plataforma» significa que el destino compila y supera las pruebas de plataforma, pero aún no se ha puesto en marcha la compilación completa del juego.

Solo `x86_64-windows-msvc` necesita una instalación de Visual Studio. Zig aporta su propia cadena de herramientas, cabeceras y bibliotecas de importación para cualquier otro destino, MinGW incluido.

Cada destino se ejercita con el flujo de trabajo `Cross-platform validation` en un runner de su propia arquitectura, de modo que las pruebas se ejecutan en lugar de solo enlazarse.


# Ejecutar el juego con Zig (principal)

1. Clona el repositorio con sus submódulos, o ejecuta `git submodule update --init --recursive` en una copia existente.

2. Instala Zig (0.17 o posterior). Para `x86_64-windows-msvc` también necesitas la cadena de herramientas MSVC y un SDK de Windows (las rutas se pueden configurar con `-Dmsvc-include`/`-Dwindows-sdk-include` y sus equivalentes `lib` si difieren de los valores por defecto de `build.zig`). Ningún otro destino los requiere.

3. Ejecuta `zig build install-game -Dtarget=<triple> --release=fast` para jugar, o quita `--release=fast` para una compilación de depuración. Sin `-Dtarget` se compila para el host.

4. Inicia el binario generado — `zig-out/game/windows/x86_64/release/Game.exe` en Windows, `zig-out/game/macos/arm64/release/Game` en Apple Silicon (`.../debug/...` sin `--release=fast`). El árbol generado lleva el nombre de la optimización que contiene, así que nunca se sobrescriben.

5. `zig build package` crea paquetes zip distribuibles; `zig build test` ejecuta las pruebas unitarias de Zig y la prueba de humo de la ABI de C++.


# Ejecutar el juego con Visual Studio 2026 Insiders

1. Clona el repositorio con sus submódulos, o ejecuta `git submodule update --init --recursive` en una copia existente.

2. Instala Visual Studio 2026 Insiders / Visual Studio 18 con la carga de trabajo «Desarrollo de escritorio con C++», la cadena de herramientas MSVC de VS 2026 y un SDK de Windows 10 o Windows 11.

3. Abre `Sources/src/A7.sln`.

4. Selecciona `Debug | Win32`.

5. Compila el proyecto `Game` o la solución completa.

6. Inicia el proyecto `Game` con F5, o ejecuta directamente `Sources/src/Game/Debug/Game.exe`.

7. El juego se inicia en ventana por defecto. Añade `-fullscreen` a los argumentos de línea de comandos del proyecto `Game` si quieres el antiguo comportamiento a pantalla completa.

Si una compilación no puede copiar una DLL en `Sources/src/Game/Debug`, cierra cualquier proceso `Game.exe` en ejecución y compila de nuevo.


# Historia hasta ahora

- Se encontraron las bibliotecas que faltaban y se añadieron como submódulos de Git.

- Se montó una máquina virtual con Windows XP SP3 y Visual Studio 6 para estudiar el entorno original.

- VS 6 era inestable y fallaba a menudo, así que el esfuerzo pasó a herramientas modernas.

- Se instaló Visual Studio 2010 y se convirtió el antiguo proyecto `.dwr` en una `.sln`.

- Se cargó la solución en VS 2026 Insiders y se actualizaron todas las dependencias.

- Se completaron dos semanas de programación con agentes para que la solución compilara desde cero sin errores ni advertencias.

- La depuración en tiempo de ejecución ya ha pasado del menú inicial: el tutorial carga y se puede jugar, incluida la selección y el movimiento de un tanque.

- El desarrollo se trasladó a VS Code Insiders por sus mejores herramientas.

- Se configuraron dos vías de depuración: C++ nativo y WinDbg.

- Se eliminó BugSlay porque dificultaba la depuración y en un caso falló después de un fallo.

- BugSlay se sustituyó por asserts estándar de C++ simples.

- Sin BugSlay, el proyecto está listo para una depuración en tiempo de ejecución centrada hasta lograr una ejecución completa.

- La compilación pasó a Zig: `build.zig` compila cada proyecto con clang, enlaza con la cadena de herramientas MSVC y genera una estructura de juego ejecutable.

- Se reemplazó StreamIO.dll — la capa de persistencia y servicios del motor — por una implementación en Zig con un puente de ABI de C++, corrigiendo por el camino el formato del grafo de objetos al guardar y cargar, la lectura y escritura de XML, las opciones y regresiones de rendimiento por fotograma.

- El juego se hizo jugable de principio a fin con la compilación de Zig: campañas, tutoriales, abordaje, aviación, ciclos de guardado y carga, vídeo sincronizado y minimapa correcto en resoluciones modernas.

- Se portó el juego a 64 bits: se corrigieron truncamientos de punteros en el renderizador, llamadas `_control87` de la era x87, `#pragma pack(1)` en tipos STL, ensamblador x86 en línea (sustituido por intrínsecos SSE portables), el canal de punteros de unidades de Lua y los identificadores de objeto del formato de guardado.

- Se probó jugando la compilación x64 a través de los tutoriales, fallo a fallo, y UBSan convirtió cada comportamiento indefinido de hace 20 años en un archivo y una línea exactos: coordenadas del minimapa, hash de formaciones, cálculo del ángulo de bombardeo y más.

- Se abandonó Win32 por una capa de plataforma portable: SDL3 para ventanas, entrada y audio, un renderizador SDL GPU en lugar de la ruta DirectX y un pequeño adaptador nativo por sistema operativo.

- Se puso en marcha el juego en macOS, donde se puede jugar en Apple Silicon, y Linux le sigue.

- Se añadieron Linux arm64 (clase Raspberry Pi) y la ABI GNU de Windows (MinGW) como destinos, lo que obligó a separar «¿es Windows?» de «¿es Visual Studio?» en toda la compilación. Así se descubrió que macOS Intel se compilaba sin ninguno de sus adaptadores Cocoa, que el trabajo de Linux estaba en rojo por falta de un runtime de C y que las rutas de C++ de Linux estaban fijadas al triple multiarch de x86_64.

- Se pusieron los seis destinos bajo CI, cada uno en un runner de su propia arquitectura.


# Hoja de ruta

1. Terminar de estabilizar la compilación de 64 bits en los tutoriales y campañas restantes.

2. Poner en marcha la compilación completa del juego en Linux arm64 y MinGW — hoy solo se validan sus capas de plataforma — y probar jugando macOS Intel.

3. Crear una utilidad para convertir partidas guardadas de 32 bits al formato de 64 bits.

4. Sustituir FMOD, Stingray y Bink por alternativas de código abierto.

5. Seguir sustituyendo uno a uno los proyectos de C++ por código Zig, usando el port de StreamIO como plantilla.
