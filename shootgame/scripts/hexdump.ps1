$bytes = [System.IO.File]::ReadAllBytes("c:\Users\yuuka\Desktop\vs\shootgame\main.cpp")
# Show first 10 bytes as hex
$hex = ($bytes[0..9] | ForEach-Object { "{0:X2}" -f $_ }) -join " "
Write-Host "First 10 bytes: $hex"
# Show first 100 bytes as string to see encoding
$enc = [System.Text.Encoding]::UTF8
$str = $enc.GetString($bytes[0..200])
Write-Host "First 200 bytes decoded: $str"
# Check for simhei.ttf
if (Test-Path "C:\Windows\Fonts\simhei.ttf") { Write-Host "simhei.ttf: EXISTS" } else { Write-Host "simhei.ttf: NOT FOUND" }
if (Test-Path "C:\Windows\Fonts\msyh.ttc") { Write-Host "msyh.ttc: EXISTS" } else { Write-Host "msyh.ttc: NOT FOUND" }
if (Test-Path "C:\Windows\Fonts\simsun.ttc") { Write-Host "simsun.ttc: EXISTS" } else { Write-Host "simsun.ttc: NOT FOUND" }
# List all .ttf/.ttc that might be Chinese
Get-ChildItem "C:\Windows\Fonts\*" -Include "*.ttf","*.ttc" -ErrorAction SilentlyContinue |
  Where-Object { $_.Name -match "sim|hei|song|kai|yahei|msyh|fang|FZY|FZS|STX|STH" } |
  ForEach-Object { Write-Host "  $($_.Name)" }
