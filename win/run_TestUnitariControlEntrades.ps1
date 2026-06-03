$appLog = Join-Path $PSScriptRoot "build-win\app.log"

Start-Process `
    -FilePath               "$PSScriptRoot\build-win\app.exe" `
    -ArgumentList           "1" `
    -WorkingDirectory       "$PSScriptRoot\build-win" `
    -RedirectStandardOutput $appLog `
    -NoNewWindow -Wait

Get-Content $appLog | ForEach-Object { Write-Host $_ }

Write-Host ""
Write-Host "app.log        → $appLog"
Write-Host "TestUnitariControlEntrades.log → $(Join-Path $PSScriptRoot 'build-win\TestUnitariControlEntrades.log')"
