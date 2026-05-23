$ffmpegPath = "C:\Users\yuuka\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-8.1-full_build\bin\ffmpeg.exe"
$src = "c:\Users\yuuka\Desktop\vs\shootgame\frames_sampled"
$dst = "c:\Users\yuuka\Desktop\vs\shootgame\frames_flipped"

$anims = @("idle", "attack", "die")
foreach ($anim in $anims) {
    $srcDir = Join-Path $src $anim
    $dstDir = Join-Path $dst $anim
    New-Item -ItemType Directory -Force -Path $dstDir | Out-Null

    Get-ChildItem $srcDir -Filter '*.png' | Sort-Object Name | ForEach-Object {
        $out = Join-Path $dstDir $_.Name
        & $ffmpegPath -i $_.FullName -vf "hflip" $out -y 2>&1 | Out-Null
    }
    $count = (Get-ChildItem $dstDir -Filter '*.png').Count
    Write-Host "$anim -> $count frames flipped"
}
Write-Host "Done!"
