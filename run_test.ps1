$appLog  = Join-Path $PSScriptRoot "build-win\app.log"
$testLog = Join-Path $PSScriptRoot "build-win\test.log"

$appWriter  = [System.IO.StreamWriter]::new($appLog,  $false, [System.Text.Encoding]::ASCII)
$testWriter = [System.IO.StreamWriter]::new($testLog, $false, [System.Text.Encoding]::ASCII)

$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName               = "$PSScriptRoot\build-win\app.exe"
$psi.Arguments              = "1"
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError  = $true
$psi.UseShellExecute        = $false

$proc = [System.Diagnostics.Process]::new()
$proc.StartInfo = $psi

$proc.add_OutputDataReceived({
    param($s, $e)
    if ($null -ne $e.Data) {
        Write-Host $e.Data
        $appWriter.WriteLine($e.Data); $appWriter.Flush()
    }
})
$proc.add_ErrorDataReceived({
    param($s, $e)
    if ($null -ne $e.Data) {
        Write-Host $e.Data -ForegroundColor Cyan
        $testWriter.WriteLine($e.Data); $testWriter.Flush()
    }
})

try {
    $proc.Start()             | Out-Null
    $proc.BeginOutputReadLine()
    $proc.BeginErrorReadLine()
    $proc.WaitForExit()
} finally {
    $appWriter.Close()
    $testWriter.Close()
}

Write-Host "app.log  → $appLog"
Write-Host "test.log → $testLog"
