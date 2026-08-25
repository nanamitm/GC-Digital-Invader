# Build GC DIGITAL INVADER for Arduboy (arduino:leonardo / ATmega32u4)
# Requires arduino-cli in PATH.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
    Write-Error "arduino-cli not found. Install it first (winget install ArduinoSA.CLI)."
}

arduino-cli core update-index
if (-not (arduino-cli core list | Select-String 'arduino:avr')) {
    arduino-cli core install arduino:avr
}
foreach ($lib in @('Arduboy2', 'ArduboyTones')) {
    if (-not (arduino-cli lib list | Select-String $lib)) {
        arduino-cli lib install $lib
    }
}

arduino-cli compile --fqbn arduino:leonardo `
    --output-dir (Join-Path $root 'build') `
    (Join-Path $root 'DigitalInvader')

Write-Host "`nhex: $(Join-Path $root 'build\DigitalInvader.ino.hex')"
