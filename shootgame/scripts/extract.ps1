$ffmpegPath = "C:\Users\yuuka\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-8.1-full_build\bin\ffmpeg.exe"
$dest = "c:\Users\yuuka\Desktop\vs\shootgame\frames"
New-Item -ItemType Directory -Force -Path $dest | Out-Null

# Find the 立绘 directory
$dirs = Get-ChildItem 'C:\Users\yuuka\Downloads\' -Directory
$lihuiDir = $null
foreach ($d in $dirs) {
    $files = Get-ChildItem $d.FullName -Filter '*.webm' -ErrorAction SilentlyContinue
    if ($files -and $files.Count -gt 3) {
        $lihuiDir = $d.FullName
        break
    }
}

if (-not $lihuiDir) {
    Write-Host "Directory not found!"
    exit 1
}

Write-Host "Found directory: $lihuiDir"

# Copy valid files to simple paths
$counter = 0
Get-ChildItem $lihuiDir -Filter '*.webm' | Where-Object { $_.Length -gt 1000 } | ForEach-Object {
    $shortName = "anim$counter"
    $destPath = "c:\Users\yuuka\Desktop\vs\shootgame\$shortName.webm"
    Copy-Item $_.FullName $destPath -Force
    Write-Host "Copied: $($_.Name) -> $shortName.webm ($($_.Length) bytes)"

    # Extract frames
    $frameDir = Join-Path $dest $shortName
    New-Item -ItemType Directory -Force -Path $frameDir | Out-Null
    & $ffmpegPath -i $destPath -vf "scale=128:128:force_original_aspect_ratio=decrease,pad=128:128:(ow-iw)/2:(oh-ih)/2" "$frameDir\frame_%04d.png" -y 2>&1 | Out-Null
    $frameCount = (Get-ChildItem $frameDir -Filter '*.png').Count
    Write-Host "  Extracted $frameCount frames to $frameDir"
    $counter++
}

Write-Host "Done!"
