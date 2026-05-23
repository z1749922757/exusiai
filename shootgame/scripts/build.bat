@echo off
cd /d c:\Users\yuuka\Desktop\vs\shootgame

set SFML_DIR=lib\SFML-2.6.1

g++ -std=c++17 -O2 -Wall ^
    -I"%SFML_DIR%\include" ^
    main.cpp ^
    -L"%SFML_DIR%\lib" ^
    -lsfml-graphics -lsfml-window -lsfml-system ^
    -o shootgame.exe

if %errorlevel% == 0 (
    echo Build success!
    copy "%SFML_DIR%\bin\*.dll" . >nul 2>&1
    echo DLLs copied. Run: shootgame.exe
) else (
    echo Build failed!
)
