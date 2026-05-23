cd c:\Users\yuuka\Desktop\vs\shootgame
$SFML_DIR = "lib\SFML-2.6.1"
$env:PATH += ";D:\ruan\mingw64\bin"

Write-Host "Compiling..."
& g++ -std=c++17 -O2 -Wall -finput-charset=UTF-8 -fexec-charset=UTF-8 -I"$SFML_DIR\include" main.cpp -L"$SFML_DIR\lib" -lsfml-graphics -lsfml-window -lsfml-system -o shootgame.exe 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build success!"
    Copy-Item "$SFML_DIR\bin\*.dll" -Destination "." -Force
    Write-Host "DLLs copied."
} else {
    Write-Host "Build failed!"
}
