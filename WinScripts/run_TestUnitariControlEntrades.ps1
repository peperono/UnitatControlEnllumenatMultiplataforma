$appLog = Join-Path $PSScriptRoot "..\LogResults\app.log"

Start-Process `
    -FilePath               "$PSScriptRoot\..\build-win\app.exe" `
    -ArgumentList           "TEST_UNITARI" `
    -WorkingDirectory       "$PSScriptRoot\.." `
    -RedirectStandardOutput $appLog `
    -NoNewWindow -Wait

Get-Content $appLog | ForEach-Object { Write-Host $_ }
