$appLog  = Join-Path $PSScriptRoot "build-win\app.log"
$testLog = Join-Path $PSScriptRoot "build-win\test.log"

Start-Process `
    -FilePath               "$PSScriptRoot\build-win\app.exe" `
    -ArgumentList           "1" `
    -RedirectStandardOutput $appLog `
    -RedirectStandardError  $testLog `
    -NoNewWindow -Wait

Get-Content $appLog | ForEach-Object { Write-Host $_ }

Write-Host ""
Write-Host "app.log  → $appLog"
Write-Host "test.log → $testLog"
