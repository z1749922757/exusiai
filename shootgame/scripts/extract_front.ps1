Get-ChildItem 'C:\Users\yuuka\Downloads' -Recurse -Filter '*.webm' -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host ("{0,10} {1}" -f $_.Length, $_.FullName)
}
