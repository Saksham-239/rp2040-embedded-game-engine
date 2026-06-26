Add-Type -AssemblyName System
$coms = [System.IO.Ports.SerialPort]::GetPortNames()

Write-Host "Found ports: $coms"

foreach ($portName in $coms) {
    Write-Host "Reading $portName..."
    try {
        $port = New-Object System.IO.Ports.SerialPort $portName, 115200, None, 8, One
        $port.DtrEnable = $true
        $port.Open()
        
        $output = ""
        for ($i = 0; $i -lt 5; $i++) {
            Start-Sleep -Seconds 1
            $output += $port.ReadExisting()
        }
        
        $port.Close()
        if ($output.Length -gt 0) {
            Write-Host "--- OUTPUT FROM $portName ---"
            Write-Host $output
            Write-Host "-----------------------------"
            break
        } else {
            Write-Host "$portName returned empty string."
        }
    } catch {
        Write-Host "Could not open $portName"
    }
}
