@echo off
cd /d c:\Users\yuuka\Desktop\vs\shootgame\lib
curl -L -o sfml.zip "https://github.com/SFML/SFML/releases/download/2.6.1/SFML-2.6.1-windows-gcc-13.1.0-mingw-64-bit.zip"
echo Download complete: %errorlevel%
