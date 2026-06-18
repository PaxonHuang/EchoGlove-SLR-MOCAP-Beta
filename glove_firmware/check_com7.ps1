try {
    $p = New-Object System.IO.Ports.SerialPort 'COM7', 115200
    $p.Open()
    Write-Host "COM7 OPENED OK"
    $p.Close()
    exit 0
} catch {
    Write-Host "COM7 ERROR: $($_.Exception.Message)"
    exit 1
}
