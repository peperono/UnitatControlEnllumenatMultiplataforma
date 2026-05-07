# Connecta un dispositiu USB (FTDI/CP210x/CH340) a WSL2 via usbipd.
# Executa des de PowerShell (no cal admin si el bind ja s'ha fet).
#
# Ús:
#   .\flash-attach-usb.ps1            → detecta automàticament
#   .\flash-attach-usb.ps1 -BusId 1-3 → força un BUSID concret

param(
    [string]$BusId = ""
)

$knownChips = @("FT2232", "CP210", "CH340", "CH341", "UART", "Serial")

function Find-EspDevice {
    $lines = usbipd list 2>&1
    foreach ($line in $lines) {
        $lineStr = $line.ToString().Trim()
        # El BUSID té format X-Y al principi de la línia
        if ($lineStr -match "^(\d+-\d+)\s") {
            $busid = $matches[1]
            foreach ($chip in $knownChips) {
                if ($lineStr -match $chip) {
                    return $busid
                }
            }
        }
    }
    return $null
}

# --- Troba o valida el BUSID ---
if ($BusId -eq "") {
    Write-Host "Cercant dispositiu ESP32/FTDI/CP210x..." -ForegroundColor Cyan
    Write-Host "Dispositius detectats per usbipd:" -ForegroundColor Gray
    usbipd list
    Write-Host ""
    $BusId = Find-EspDevice
    if ([string]::IsNullOrEmpty($BusId)) {
        Write-Host "No s'ha trobat cap dispositiu compatible." -ForegroundColor Red
        Write-Host "Usa:  .\flash-attach-usb.ps1 -BusId X-Y" -ForegroundColor Yellow
        exit 1
    }
    Write-Host "Trobat: BUSID $BusId" -ForegroundColor Green
}

# --- Bind (si encara no s'ha fet) ---
$listOut = usbipd list 2>&1 | Out-String
if ($listOut -match "$BusId\s.*Not shared") {
    Write-Host "Fent bind de $BusId (cal admin la primera vegada)..." -ForegroundColor Yellow
    usbipd bind --busid $BusId
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error al bind. Obre PowerShell com a administrador i torna a executar." -ForegroundColor Red
        exit 1
    }
}

# --- Detach previ si ja estava attached ---
$listOut2 = usbipd list 2>&1 | Out-String
if ($listOut2 -match "$BusId\s.*Attached") {
    Write-Host "Desconnectant $BusId per re-enganxar..." -ForegroundColor Yellow
    usbipd detach --busid $BusId 2>&1 | Out-Null
    Start-Sleep -Milliseconds 500
}

# --- Carrega el driver FTDI ABANS de l'attach ---
Write-Host "Carregant driver ftdi_sio a docker-desktop..." -ForegroundColor Cyan
wsl -d docker-desktop modprobe ftdi_sio 2>&1 | Out-Null

# --- Attach a docker-desktop ---
Write-Host "Connectant $BusId a docker-desktop..." -ForegroundColor Cyan
$attachOut = usbipd attach --wsl docker-desktop --busid $BusId 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error al attach: $attachOut" -ForegroundColor Red
    exit 1
}
Start-Sleep -Seconds 1

# --- Crea els nodes de dispositiu al contenidor Docker ---
Write-Host "Creant nodes /dev/ttyUSB* al contenidor..." -ForegroundColor Cyan
docker exec -u root (docker ps -q) sh -c "mknod /dev/ttyUSB0 c 188 0 2>/dev/null; mknod /dev/ttyUSB1 c 188 1 2>/dev/null; chmod 666 /dev/ttyUSB0 /dev/ttyUSB1" 2>&1 | Out-Null

Write-Host ""
Write-Host "Fet! /dev/ttyUSB0 i /dev/ttyUSB1 accessibles al contenidor." -ForegroundColor Green
