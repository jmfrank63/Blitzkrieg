@echo off
rem The one canonical way to launch the current build. Stages nothing, builds
rem nothing - runs exactly what "zig build install-game --release=fast" staged.
cd /d "%~dp0zig-out\game\windows\x86_64\release"
if not exist Game.exe (
    echo No release build staged. Run: zig build install-game -Dtarget=x86_64-windows-msvc --release=fast
    pause
    exit /b 1
)
echo Running: %CD%\Game.exe %*
Game.exe %*
