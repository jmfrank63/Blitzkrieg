[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



Este repositorio es un proyecto personal de Johannes Maria Frank. Lo uso para diversión y aprendizaje, especialmente para practicar codificación agentiva en aplicaciones brownfield. El README original se conserva en `readme_original`.

Advertencia: este proyecto todavía está en progreso.



# Qué puede hacer este repositorio hoy

- Contiene el código fuente completo de Blitzkrieg para un solo jugador y los datos del juego.

- Se compila limpiamente desde una solución `A7.sln` fresca con herramientas MSVC modernas.

- Ahora se ejecuta en una ventana real por defecto, con `-fullscreen` todavía disponible como opción de línea de comandos.

- El tutorial ahora funciona en la compilación `Debug | Win32`; las unidades se pueden seleccionar y mover mientras continúa la depuración en tiempo de ejecución.

- Incluye informes de excepciones nativos y soporte de depuración moderno.

- Usa submódulos Git para las bibliotecas faltantes.

- Soporta depuración tanto nativa de C++ como WinDbg en VS Code Insiders.

- Ha eliminado el manejo de fallos Legacy BugSlay y lo ha reemplazado con asserts estándar de C++.



# Ejecutar el juego con Visual Studio 2026 Insiders

1. Clona el repositorio con submódulos, o ejecuta `git submodule update --init --recursive` en un checkout existente.

2. Instala Visual Studio 2026 Insiders / Visual Studio 18 con la carga de trabajo Desktop development with C++, la toolchain MSVC de VS 2026 y un SDK de Windows 10 o Windows 11.

3. Abre `Sources/src/A7.sln`.

4. Selecciona `Debug | Win32`.

5. Compila el proyecto `Game`, o compila la solución completa.

6. Inicia el proyecto `Game` con F5, o ejecuta directamente `Sources/src/Game/Debug/Game.exe`.

7. El juego arranca en modo ventana por defecto. Añade `-fullscreen` a los argumentos de comando del proyecto Game si quieres el comportamiento antiguo de pantalla completa.

Si una compilación no puede copiar una DLL a `Sources/src/Game/Debug`, cierra cualquier proceso `Game.exe` en ejecución y vuelve a compilar.



# Historia hasta ahora

- Se encontraron las bibliotecas faltantes y se agregaron como submódulos Git.

- Se creó una VM de Windows XP SP3 con Visual Studio 6 para estudiar el entorno original.

- VS6 era inestable y se bloqueaba con frecuencia, así que el esfuerzo se trasladó a herramientas modernas.

- Se instaló Visual Studio 2010 y se convirtió el proyecto antiguo `.dwr` en `.sln`.

- Se cargó la solución en VS 2026 Insiders y se actualizaron todas las dependencias.

- Dos semanas de codificación agentiva permitieron que la solución compilara desde una base limpia sin errores ni advertencias.

- La depuración en tiempo de ejecución ya ha pasado del menú inicial: el tutorial carga y es jugable, incluida la selección y el movimiento de un tanque.

- El desarrollo se movió a VS Code Insiders por sus mejores herramientas.

- Se configuraron dos rutas de depuración: C++ nativo y WinDbg.

- Se eliminó BugSlay porque hacía la depuración más difícil y, en un caso, se estrelló después de un fallo.

- BugSlay se reemplazó por simples asserts estándar de C++.

- Con BugSlay eliminado, el proyecto ahora está listo para continuar la depuración enfocada hacia una ejecución completa.



# Hoja de ruta

1. Seguir eliminando las excepciones en tiempo de ejecución restantes más allá de la ruta del tutorial que ahora funciona.

2. Mover la compilación a Zig.

3. Reemplazar FMOD, Stingray y Bink con alternativas de código abierto.

4. Comenzar a reemplazar proyectos C++ uno por uno con código Zig.
