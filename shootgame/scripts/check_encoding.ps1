# Check encoding of main.cpp
$bytes = [System.IO.File]::ReadAllBytes("c:\Users\yuuka\Desktop\vs\shootgame\main.cpp")
Write-Host "First 3 bytes (BOM check): $($bytes[0]) $($bytes[1]) $($bytes[2])"
if ($bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
    Write-Host "Encoding: UTF-8 with BOM"
} elseif ($bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
    Write-Host "Encoding: UTF-16 LE BOM"
} else {
    Write-Host "Encoding: No BOM (likely UTF-8 without BOM or ANSI)"
}

# Check if SimHei font exists
$fontPath = "C:\Windows\Fonts\simhei.ttf"
if (Test-Path $fontPath) {
    Write-Host "SimHei font: EXISTS at $fontPath"
} else {
    Write-Host "SimHei font: NOT FOUND"
    # List Chinese fonts
    Write-Host "Available Chinese-ish fonts:"
    Get-ChildItem "C:\Windows\Fonts\*" -Include "*.ttf","*.ttc" | Where-Object {
        $_.Name -match "sim|hei|song|kai|yahei|msyh|fang|STX|STH|FZY"
    } | ForEach-Object { Write-Host "  $($_.Name)" }
}
