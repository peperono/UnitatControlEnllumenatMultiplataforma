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
        foreach ($chip in $knownChips) {
            if ($line -match $chip) {
                # Extreu el BUSID (format X-Y al principi de la línia)
                if ($line -match "^(\d+-\d+)") {
                    return $matches[1]
                }
            }
        }
    }
    return $null
}

# --- Troba o valida el BUSID ---
if ($BusId -eq "") {
    Write-Host "Cercant dispositiu ESP32/FTDI/CP210x..." -ForegroundColor Cyan
    $BusId = Find-EspDevice
    if ($null -eq $BusId) {
        Write-Host "No s'ha trobat cap dispositiu compatible." -ForegroundColor Red
        Write-Host ""
        Write-Host "Dispositius disponibles:"
        usbipd list
        exit 1
    }
    Write-Host "Trobat: BUSID $BusId" -ForegroundColor Green
}

# --- Bind (si encara no s'ha fet) ---
$state = (usbipd list 2>&1 | Select-String $BusId)
if ($state -match "Not shared") {
    Write-Host "Fent bind de $BusId (cal admin la primera vegada)..." -ForegroundColor Yellow
    usbipd bind --busid $BusId
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error al bind. Obre PowerShell com a administrador i torna a executar." -ForegroundColor Red
        exit 1
    }
}

# --- Attach a WSL2 ---
Write-Host "Connectant $BusId a WSL2..." -ForegroundColor Cyan
usbipd attach --wsl --busid $BusId
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error al attach." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Fet! El dispositiu ja és accessible des de WSL2." -ForegroundColor Green
Write-Host "Comprova amb:  wsl -- ls /dev/ttyUSB*" -ForegroundColor Gray
