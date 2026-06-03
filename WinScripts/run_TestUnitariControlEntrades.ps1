$appLog = Join-Path $PSScriptRoot "..\LogResults\app.log"

Start-Process `
    -FilePath               "$PSScriptRoot\..\build-win\app.exe" `
    -ArgumentList           "1" `
    -WorkingDirectory       "$PSScriptRoot\.." `
    -RedirectStandardOutput $appLog `
    -NoNewWindow -Wait

Get-Content $appLog | ForEach-Object { Write-Host $_ }
