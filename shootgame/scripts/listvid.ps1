$dirs = Get-ChildItem 'C:\Users\yuuka\Downloads\' -Directory
foreach ($d in $dirs) {
    $files = Get-ChildItem $d.FullName -Filter '*.webm' -ErrorAction SilentlyContinue
    if ($files) {
        Write-Host "=== $($d.Name) ==="
        $files | ForEach-Object {
            Write-Host ("{0,10} {1}" -f $_.Length, $_.Name)
        }
    }
}
