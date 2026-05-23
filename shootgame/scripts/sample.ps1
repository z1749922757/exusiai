$dest = "c:\Users\yuuka\Desktop\vs\shootgame\frames_sampled"
New-Item -ItemType Directory -Force -Path $dest | Out-Null

# Sample each animation down to reasonable frame counts
$animations = @(
    @{ name = "idle";    src = "anim4"; maxFrames = 12 },
    @{ name = "attack";  src = "anim1"; maxFrames = 8 },
    @{ name = "die";     src = "anim3"; maxFrames = 10 }
)

foreach ($anim in $animations) {
    $srcDir = "c:\Users\yuuka\Desktop\vs\shootgame\frames\$($anim.src)"
    $dstDir = Join-Path $dest $anim.name
    New-Item -ItemType Directory -Force -Path $dstDir | Out-Null

    $frames = Get-ChildItem $srcDir -Filter '*.png' | Sort-Object Name
    $total = $frames.Count
    $step = [Math]::Max(1, [int]($total / $anim.maxFrames))

    $idx = 0
    for ($i = 0; $i -lt $total -and $idx -lt $anim.maxFrames; $i += $step) {
        $srcFile = $frames[$i].FullName
        $dstFile = Join-Path $dstDir ("frame_{0:D4}.png" -f $idx)
        Copy-Item $srcFile $dstFile -Force
        $idx++
    }
    Write-Host "$($anim.name): $idx frames (from $total)"
}

Write-Host "Done!"
