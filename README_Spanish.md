[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Este repositorio es un proyecto personal de Johannes Maria Frank. Lo uso por diversión y para aprender, sobre todo para practicar la programación con agentes en aplicaciones heredadas (brownfield). El README original se conserva en `readme_original`.

Aviso: este proyecto está en desarrollo.


# Dónde está el proyecto (noviembre de 2026)

El port en sí está en gran parte terminado: el juego se ejecuta de forma nativa en Windows, macOS y Linux, habla diez idiomas y todos los editores están portados. Ahora mismo el proyecto está atascado en el arte gráfico, en las vistas giradas de edificios y objetos del escenario. Consulta «El muro: el arte gráfico» más abajo.


# Lo que se ha hecho

**El juego**

- Aquí está todo el juego para un jugador con sus datos, compilado con `zig build` (Zig + clang) como programa nativo de 64 bits. Las campañas, los tutoriales y las misiones aleatorias se pueden jugar, con guardar y cargar partidas, vídeo y sonido.

- El motor ha dejado atrás Win32 y DirectX y usa una capa portable: SDL3 para ventanas, entrada y audio, y un renderizador SDL GPU.

- Las bibliotecas cerradas ya no están: se han eliminado FMOD, Bink y Stingray. El sonido y las películas ahora usan Ogg Vorbis y Ogg Theora.

- StreamIO.dll, la capa de persistencia del motor, se ha sustituido por una implementación en Zig (`Sources/src/StreamIOZig`).

- El C++ heredado se compila con clang y UBSan, lo que ha sacado a la luz y corregido decenas de errores que la compilación de 2003 toleraba en silencio.

- Las misiones aleatorias originales vuelven a funcionar, y cada capítulo de la campaña desbloquea su misión histórica, tal como lo prevé su guion.

- Los perfiles de jugador pueden acompañar al jugador entre máquinas mediante la sincronización en la nube.

- El multijugador en red local (LAN) funciona. He quitado el código de GameSpy, y el juego por internet todavía no está conectado.

**Diez idiomas**

- El juego, todos los editores, las herramientas de línea de comandos y sus guías, y este README están disponibles en inglés, ruso, chino (simplificado), hindi, español, francés, alemán, portugués (Brasil), japonés e indonesio.

- Las nueve traducciones se instalan como mods de idioma, con un nuevo motor de texto TrueType que dibuja todas estas escrituras.

- Las traducciones son automáticas y aún necesitan la revisión de hablantes nativos. Se agradecen las correcciones.

**Todos los editores**

- El Map Editor, el Resource Editor con todos sus subeditores y ELK, el kit de localización, se han reescrito como aplicaciones Zig con Dear ImGui (`Sources/editor`). Hablan con el motor de C++ mediante una ABI de C (`Sources/src/EditorBridge`).

- Los antiguos editores MFC, solo para Windows, se han eliminado. El juego lee lo que escriben los nuevos editores, y las pruebas lo comprueban con los lectores del propio juego.

- Las antiguas herramientas de línea de comandos se han sustituido por siete portables para crear mods: `bk-pak`, `bk-table`, `bk-image`, `bk-video`, `bk-font`, `bk-map` y `bk-sprite`. Consulta `tools/guide/README.md`.

**La opción Remastered**

- Opciones > Ajustes > Jugabilidad > Remastered viene desactivada por defecto, y desactivada se juega con las reglas originales. Activada añade funciones que el juego de 2003 no tenía, como girar la cámara en cuatro pasos, un buscador de la ruta más rápida y mover la infantería directamente de un contenedor a otro.

- Las repeticiones de misiones están en construcción; su base ya está integrada.


# El muro: el arte gráfico

Girar la cámara significa que cada edificio y cada objeto del escenario deben mostrarse desde cuatro lados, pero el arte de 2003 es un conjunto de imágenes de cada objeto desde un solo lado. Los demás lados nunca se dibujaron.

Hasta ahora he probado dos enfoques con los 412 objetos:

- Construir cada imagen girada a partir de píxeles de 2003 (lo abandoné el 2026-10-29: por cada problema que resolvía creaba uno nuevo).

- Renderizar modelos 3D sencillos pintados con esas mismas imágenes. Es lo que hace hoy el proyecto. El lado que muestra el arte de 2003 se ve bien, pero todo lo que nunca se vio hay que adivinarlo. Además, los detalles fallan: una buhardilla pintada sobre un tejado plano no puede verse bien desde otro lado.

Ninguno es suficientemente bueno. La forma correcta es un modelo 3D real de cada objeto, con detalles como las buhardillas hechos de geometría real y una textura en cada cara plana, de modo que cada lado se deduzca de la forma.

La cuestión es de dónde salen esos modelos y texturas:

- Modelarlos a mano o con agentes, con los diseños de 2003.

- Los modelos de Blitzkrieg 2, que también publicó Nival. Un render de prueba muestra que están construidos así y que se ven limpios en los cuatro giros, pero muchos son edificios distintos de los de 2003 con el mismo nombre, y la licencia no cubre claramente su arte.

- Generadores de imagen a 3D, que no he probado con este arte.

No se ha tomado ninguna decisión. Si eres artista 3D o conoces este tipo de trabajo, me gustaría saber de ti.


# Conversión y validación de recursos de Blender

La cadena que sustituye a Maya40 está documentada en `docs/blender-replacement.md`.

Comandos rápidos:

- Convertir: `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Solo validar: `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Plataformas compatibles

Se indica un destino con `-Dtarget=`. El árbol generado se organiza como `zig-out/game/<os>/<arch>/<config>`, de modo que varios destinos y ambos niveles de optimización conviven uno al lado del otro.

| Plataforma | Triple de destino | Estado |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | El juego y los editores funcionan; probado en CI |
| macOS (Apple Silicon) | `aarch64-macos` | El juego y los editores funcionan; probado en CI |
| Linux (x86_64) | `x86_64-linux-gnu` | El juego y los editores funcionan; probado en CI con un renderizador por software |
| macOS (Intel) | `x86_64-macos` | Compila; el runner de CI no tiene GPU para el juego, así que el juego se comprueba en un Mac Intel real |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Solo pruebas de plataforma; el juego y los editores aún sin probar |
| Windows (MinGW) | `x86_64-windows-gnu` | Solo pruebas de plataforma; el motor todavía no compila aquí |

CI ejecuta cada destino en un runner de su propia arquitectura, de modo que las suites se ejecutan en lugar de solo enlazarse. Algunos trabajos de Linux y macOS están fallando ahora mismo.

En Linux el icono de la ventana se define en el programa, y X11 y KDE lo respetan. GNOME en Wayland lo ignora y toma el icono del dock y de alt-tab de una entrada de escritorio con el nombre del app id del programa (`org.blitzkrieg.game`, y `org.blitzkrieg.mapeditor`, `org.blitzkrieg.resourceeditor`, `org.blitzkrieg.elk` para los editores). La instalación incluye un script opcional que escribe esas entradas y el icono en `~/.local/share`; nada lo ejecuta por ti. Desde la carpeta de instalación ejecuta `Tools/install_desktop_entry.sh` (desde una copia del repositorio: `tools/install_desktop_entry.sh [install-dir]`), y `Tools/install_desktop_entry.sh --remove` lo deshace.

Solo `x86_64-windows-msvc` necesita una instalación de Visual Studio. Zig aporta su propia cadena de herramientas, cabeceras y bibliotecas de importación para cualquier otro destino, MinGW incluido.


# Compilar y ejecutar

1. Clona el repositorio con sus submódulos, o ejecuta `git submodule update --init --recursive` en una copia existente.

2. Instala Zig 0.17. Para `x86_64-windows-msvc` también necesitas la cadena de herramientas MSVC y un SDK de Windows (las rutas se pueden configurar con `-Dmsvc-include`/`-Dwindows-sdk-include` y sus equivalentes `lib` si difieren de los valores por defecto de `build.zig`).

3. Ejecuta `zig build install-all -Dtarget=<triple> --release=fast` para obtener el juego, los nueve mods de idioma y todos los editores, o `zig build install-game` solo para el juego. Sin `-Dtarget` se compila para el host, y sin `--release=fast` se obtiene una compilación de depuración.

4. Inicia el binario generado, por ejemplo `zig-out/game/windows/x86_64/release/Game.exe` en Windows o `zig-out/game/macos/arm64/release/Game` en Apple Silicon (`.../debug/...` sin `--release=fast`). El juego se ejecuta en una ventana; `-fullscreen` está disponible como opción de línea de comandos.

5. `zig build package` crea paquetes zip distribuibles, y `zig build --help` enumera todos los pasos y pruebas.


# Historia hasta ahora

- Encontré las bibliotecas que faltaban y logré que la solución de 2003 compilara desde cero con un Visual Studio moderno; después jugué el primer tutorial.

- Pasé la compilación a Zig: `build.zig` compila cada proyecto con clang y genera una estructura de juego ejecutable.

- Sustituí StreamIO.dll por Zig e hice el juego jugable de principio a fin con la compilación de Zig.

- Porté el juego a 64 bits y lo probé jugando a través de los tutoriales, fallo a fallo, con UBSan.

- Dejé Win32 por SDL3 y un renderizador SDL GPU, y puse el juego en marcha en macOS y Linux.

- Porté el Map Editor, el Resource Editor y ELK, y eliminé las versiones MFC. Sustituí las antiguas herramientas de línea de comandos.

- Eliminé FMOD, Bink y Stingray.

- Añadí el motor de texto y los diez idiomas en el juego, los editores y las herramientas.

- Añadí la opción Remastered y el giro de cámara, que condujo al muro descrito arriba.


# Hoja de ruta

1. Superar el muro del arte gráfico: decidir de dónde salen los modelos 3D y las texturas, y conseguir permiso para lo que lo necesite.

2. Terminar las repeticiones de misiones.

3. Lograr que todos los trabajos de CI vuelvan a pasar.

4. Poner en marcha el juego y los editores en Linux arm64 y MinGW, y añadir Windows arm64.

5. Conectar el multijugador por internet mediante un sustituto de GameSpy basado en OpenSpy.

6. Crear una utilidad para convertir partidas guardadas de 32 bits al formato de 64 bits.

7. Seguir sustituyendo proyectos de C++ por código Zig, usando el port de StreamIO como plantilla.
