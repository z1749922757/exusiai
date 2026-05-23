$dirs = Get-ChildItem 'C:\Users\yuuka\Downloads\' -Directory
foreach ($d in $dirs) {
    $files = Get-ChildItem $d.FullName -Filter '*.webm' -ErrorAction SilentlyContinue
    if ($files -and $files.Count -gt 3) {
        Write-Host "=== Folder ==="
        $files | Sort-Object Name | ForEach-Object {
            Write-Host ("{0,10} {1}" -f $_.Length, $_.Name)
        }
    }
}
