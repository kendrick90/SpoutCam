# SpoutCam Complete Registry and DirectShow Cleanup Script
# Run as Administrator

Write-Host "=== SpoutCam Complete Cleanup ===" -ForegroundColor Green
Write-Host "This script will remove ALL SpoutCam registrations and registry entries."
Write-Host "WARNING: This will unregister all SpoutCam cameras!" -ForegroundColor Yellow
Write-Host ""

$confirmation = Read-Host "Continue? (y/N)"
if ($confirmation -ne 'y' -and $confirmation -ne 'Y') {
    Write-Host "Cancelled." -ForegroundColor Yellow
    exit
}

Write-Host "`n1. Cleaning up user registry entries..." -ForegroundColor Cyan

# Clean up current consolidated camera data
Write-Host "   Removing consolidated camera data..." -ForegroundColor Gray
Remove-Item "HKCU:\Software\Leading Edge\SpoutCam\SpoutCam*" -Recurse -Force -ErrorAction SilentlyContinue

# Clean up old dynamic cameras folder
Write-Host "   Removing old DynamicCameras folder..." -ForegroundColor Gray
Remove-Item "HKCU:\Software\Leading Edge\SpoutCam\DynamicCameras" -Recurse -Force -ErrorAction SilentlyContinue

# Clean up old legacy camera settings (SpoutCam2, SpoutCam3, etc.)
Write-Host "   Removing legacy camera registry keys..." -ForegroundColor Gray
for ($i = 2; $i -le 8; $i++) {
    Remove-Item "HKCU:\Software\Leading Edge\SpoutCam$i" -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "`n2. Cleaning up DirectShow registrations (requires admin)..." -ForegroundColor Cyan

# Remove specific SpoutCam CLSIDs - now only need to remove the primary CLSID
$removedClsids = 0
$primaryClsids = @(
    "{8E14549A-DB61-4309-AFA1-3578E927E933}",  # Primary SpoutCam CLSID
    "{CD7780B7-40D2-4F33-80E2-B02E009CE633}"   # Primary SpoutCam Property Page CLSID
)

foreach ($clsid in $primaryClsids) {
    $clsidPath = "HKLM:\SOFTWARE\Classes\CLSID\$clsid"
    if (Test-Path $clsidPath) {
        try {
            $name = (Get-ItemProperty $clsidPath -Name "(default)" -ErrorAction SilentlyContinue)."(default)"
            Write-Host "   Removing primary CLSID: $clsid = `"$name`"" -ForegroundColor Gray
            Remove-Item $clsidPath -Recurse -Force -ErrorAction SilentlyContinue
            $removedClsids++
        } catch {
            # Skip entries we can't access
        }
    }
}

# Legacy cleanup - scan for any remaining SpoutCam references
Get-ChildItem "HKLM:\SOFTWARE\Classes\CLSID" -ErrorAction SilentlyContinue | ForEach-Object {
    $clsid = $_.PSChildName
    try {
        $name = (Get-ItemProperty $_.PSPath -Name "(default)" -ErrorAction SilentlyContinue)."(default)"
        if ($name -match "SpoutCam") {
            Write-Host "   Removing legacy CLSID: $clsid = `"$name`"" -ForegroundColor Gray
            Remove-Item $_.PSPath -Recurse -Force -ErrorAction SilentlyContinue
            $removedClsids++
        }
    } catch {
        # Skip entries we can't access
    }
}

# Clean up DirectShow filter instances
$removedFilters = 0
$directShowPath = "HKLM:\SOFTWARE\Classes\CLSID\{083863F1-70DE-11D0-BD40-00A0C911CE86}\Instance"
if (Test-Path $directShowPath) {
    Get-ChildItem $directShowPath -ErrorAction SilentlyContinue | ForEach-Object {
        try {
            $friendlyName = (Get-ItemProperty $_.PSPath -Name "FriendlyName" -ErrorAction SilentlyContinue).FriendlyName
            if ($friendlyName -match "SpoutCam") {
                Write-Host "   Removing DirectShow filter: $friendlyName" -ForegroundColor Gray
                Remove-Item $_.PSPath -Recurse -Force -ErrorAction SilentlyContinue
                $removedFilters++
            }
        } catch {
            # Skip entries we can't access
        }
    }
}

Write-Host "`n3. Cleanup Summary:" -ForegroundColor Green
Write-Host "   Removed CLSIDs: $removedClsids" -ForegroundColor White
Write-Host "   Removed DirectShow filters: $removedFilters" -ForegroundColor White
Write-Host "   User registry entries: Cleaned" -ForegroundColor White

Write-Host "`n4. Verifying cleanup..." -ForegroundColor Cyan

# Check for remaining SpoutCam entries
$remainingClsids = Get-ChildItem "HKLM:\SOFTWARE\Classes\CLSID" -ErrorAction SilentlyContinue | ForEach-Object {
    $name = (Get-ItemProperty $_.PSPath -Name "(default)" -ErrorAction SilentlyContinue)."(default)"
    if ($name -match "SpoutCam") { $_ }
}

$remainingFilters = @()
if (Test-Path $directShowPath) {
    $remainingFilters = Get-ChildItem $directShowPath -ErrorAction SilentlyContinue | ForEach-Object {
        $friendlyName = (Get-ItemProperty $_.PSPath -Name "FriendlyName" -ErrorAction SilentlyContinue).FriendlyName
        if ($friendlyName -match "SpoutCam") { $_ }
    }
}

if ($remainingClsids -or $remainingFilters) {
    Write-Host "   WARNING: Some entries could not be removed (may require restart)" -ForegroundColor Yellow
    if ($remainingClsids) {
        Write-Host "   Remaining CLSIDs: $($remainingClsids.Count)" -ForegroundColor Yellow
    }
    if ($remainingFilters) {
        Write-Host "   Remaining DirectShow filters: $($remainingFilters.Count)" -ForegroundColor Yellow
    }
} else {
    Write-Host "   ✓ All SpoutCam registrations removed successfully!" -ForegroundColor Green
}

Write-Host "`n=== Cleanup Complete ===" -ForegroundColor Green
Write-Host "You can now run build.bat without registration conflicts." -ForegroundColor White
Write-Host "The UI will start with a clean slate (no auto-created cameras)." -ForegroundColor White

Write-Host "`nPress any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")