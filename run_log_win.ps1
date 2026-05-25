$log = "build-win\app_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
$writer = [System.IO.StreamWriter]::new($log, $false, [System.Text.Encoding]::ASCII)
try {
    .\build-win\app.exe 2 2>&1 | ForEach-Object {
        Write-Host $_
        $writer.WriteLine($_)
        $writer.Flush()
    }
} finally {
    $writer.Close()
}
Write-Host "Log guardat a: $log"
