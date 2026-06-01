$appLog = Join-Path $PSScriptRoot "build-win\app.log"

Start-Process `
    -FilePath               "$PSScriptRoot\build-win\app.exe" `
    -ArgumentList           "1" `
    -RedirectStandardOutput $appLog `
    -NoNewWindow -Wait

Get-Content $appLog | ForEach-Object { Write-Host $_ }

Write-Host ""
Write-Host "app.log        → $appLog"
Write-Host "test_result.log → $(Join-Path $PSScriptRoot 'test_result.log')"
