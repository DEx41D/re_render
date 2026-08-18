@echo off
setlocal enabledelayedexpansion

set "SOURCES=.\re_render_windows_terminal.c re_render_utilities.c"

for %%A in (%*) do (
    set "ARG=%%~A"
    
    if "!ARG:~0,2!"=="--" (
        for /f "tokens=1,2 delims==" %%B in ("!ARG!") do (
            set "FLAG=%%B"
            set "VALUE=%%C"
            
            if "!FLAG!"=="--file" (
                set "SOURCES=!VALUE!"
            )
        )
    )
)

echo Compiling: %SOURCES%
clang %SOURCES% -o re_render.exe -lgdi32 -Wall -luser32 -O2

endlocal
