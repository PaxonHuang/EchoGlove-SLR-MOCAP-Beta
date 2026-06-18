# Reset ESP32-S3 via DTR/RTS pulse and capture setup() output
$portName = "COM6"
$baud = 115200
$captureSeconds = 60

Write-Host "Opening $portName at $baud baud..."
$port = New-Object System.IO.Ports.SerialPort $portName, $baud, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$port.Handshake = [System.IO.Ports.Handshake]::None
$port.ReadTimeout = 500
$port.Open()

# ESP32 auto-reset: DTR=LOW + RTS=HIGH enters bootloader, DTR=HIGH + RTS=LOW normal run
$port.DtrEnable = $false
$port.RtsEnable = $true
Start-Sleep -Milliseconds 100
$port.DtrEnable = $false
$port.RtsEnable = $false
Start-Sleep -Milliseconds 50
$port.DtrEnable = $true
$port.RtsEnable = $false
Start-Sleep -Milliseconds 50

Write-Host "=== Serial capture start ==="
$start = Get-Date
$buf = ""
while (((Get-Date) - $start).TotalSeconds -lt $captureSeconds) {
    try {
        $n = $port.BytesToRead
        if ($n -gt 0) {
            $data = $port.ReadExisting()
            $buf += $data
            # Flush by writing line-by-line
            while ($buf.Contains("`n")) {
                $idx = $buf.IndexOf("`n")
                $line = $buf.Substring(0, $idx).TrimEnd("`r")
                Write-Host $line
                $buf = $buf.Substring($idx + 1)
            }
        } else {
            Start-Sleep -Milliseconds 20
        }
    } catch {
        Write-Host "[err] $_"
        break
    }
}
if ($buf.Length -gt 0) { Write-Host $buf }

$port.Close()
Write-Host "=== Capture ended ==="
