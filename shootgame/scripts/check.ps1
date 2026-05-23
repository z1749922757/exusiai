Add-Type -AssemblyName System.Drawing
Get-ChildItem 'c:\Users\yuuka\Desktop\vs\shootgame\assets\*.png' | ForEach-Object {
    $img = [System.Drawing.Image]::FromFile($_.FullName)
    Write-Host ($_.Name + ': ' + $img.Width + 'x' + $img.Height)
    $img.Dispose()
}
