$url = "https://github.com/SFML/SFML/releases/download/2.6.1/SFML-2.6.1-windows-gcc-13.1.0-mingw-64-bit.zip"
$output = "c:\Users\yuuka\Desktop\vs\shootgame\lib\sfml.zip"
New-Item -ItemType Directory -Force -Path "c:\Users\yuuka\Desktop\vs\shootgame\lib"
Write-Host "Downloading SFML..."
Invoke-WebRequest -Uri $url -OutFile $output
Write-Host "Done!"
