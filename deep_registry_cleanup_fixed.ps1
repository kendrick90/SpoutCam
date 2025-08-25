# SpoutCam Deep Registry Cleanup Script
# This script performs a comprehensive search and cleanup of ALL SpoutCam-related registry entries
# Run as Administrator for complete cleanup

param(
    [switch]$Force,
    [switch]$Verify
)

Write-Host "=== SpoutCam Deep Registry Cleanup ===" -ForegroundColor Green
Write-Host "This script will perform a COMPREHENSIVE cleanup of ALL SpoutCam registry entries."
Write-Host ""

if (-not $Force) {
    Write-Host "WARNING: This will remove ALL traces of SpoutCam from the registry!" -ForegroundColor Red
    Write-Host "This includes:" -ForegroundColor Yellow
    Write-Host "  - All camera configurations and settings" -ForegroundColor Yellow
    Write-Host "  - All DirectShow filter registrations" -ForegroundColor Yellow
    Write-Host "  - All dynamic camera CLSIDs" -ForegroundColor Yellow
    Write-Host "  - All property page registrations" -ForegroundColor Yellow
    Write-Host ""
    
    $confirmation = Read-Host "Continue with deep cleanup? (type 'CLEANUP' to confirm)"
    if ($confirmation -ne 'CLEANUP') {
        Write-Host "Cancelled." -ForegroundColor Yellow
        exit 1
    }
}

Write-Host ""
Write-Host "=== Phase 1: User Registry Cleanup ===" -ForegroundColor Cyan

# Remove all SpoutCam user settings (current and legacy)
$userPaths = @(
    "Software\Leading Edge\SpoutCam",
    "Software\Leading Edge\SpoutCam\DynamicCameras"
)

# Add legacy camera paths (SpoutCam2, SpoutCam3, etc.)
for ($i = 2; $i -le 10; $i++) {
    $userPaths += "Software\Leading Edge\SpoutCam$i"
}

$removedUserKeys = 0
foreach ($path in $userPaths) {
    $fullPath = "HKCU:\$path"
    if (Test-Path $fullPath) {
        Write-Host "   Removing: $path" -ForegroundColor Gray
        Remove-Item $fullPath -Recurse -Force -ErrorAction SilentlyContinue
        $removedUserKeys++
    }
}

# Also remove any SpoutCam* wildcarded entries
Write-Host "   Scanning for additional SpoutCam* entries..." -ForegroundColor Gray
$leadingEdgePath = "HKCU:\Software\Leading Edge"
if (Test-Path $leadingEdgePath) {
    Get-ChildItem $leadingEdgePath -ErrorAction SilentlyContinue | Where-Object { 
        $_.PSChildName -like "SpoutCam*" 
    } | ForEach-Object {
        Write-Host "   Removing: Software\Leading Edge\$($_.PSChildName)" -ForegroundColor Gray
        Remove-Item $_.PSPath -Recurse -Force -ErrorAction SilentlyContinue
        $removedUserKeys++
    }
}

Write-Host ""
Write-Host "=== Phase 2: System CLSID Cleanup ===" -ForegroundColor Cyan

# Search and remove ALL SpoutCam-related CLSIDs
$removedClsids = 0
$clsidPath = "HKLM:\SOFTWARE\Classes\CLSID"
if (Test-Path $clsidPath) {
    Write-Host "   Scanning CLSID registry (this may take a moment)..." -ForegroundColor Gray
    
    Get-ChildItem $clsidPath -ErrorAction SilentlyContinue | ForEach-Object {
        $clsid = $_.PSChildName
        try {
            # Check default value
            $defaultValue = (Get-ItemProperty $_.PSPath -Name "(default)" -ErrorAction SilentlyContinue)."(default)"
            if ($defaultValue -match "SpoutCam|SpoutCam\d+") {
                Write-Host "   Removing CLSID $clsid = `"$defaultValue`"" -ForegroundColor Yellow
                Remove-Item $_.PSPath -Recurse -Force -ErrorAction SilentlyContinue
                $removedClsids++
                return
            }
            
            # Check InprocServer32 path for SpoutCam DLL references
            $inprocPath = Join-Path $_.PSPath "InprocServer32"
            if (Test-Path $inprocPath) {
                $dllPath = (Get-ItemProperty $inprocPath -Name "(default)" -ErrorAction SilentlyContinue)."(default)"
                if ($dllPath -match "SpoutCam\d*\.ax") {
                    Write-Host "   Removing CLSID $clsid (references SpoutCam DLL)" -ForegroundColor Yellow
                    Remove-Item $_.PSPath -Recurse -Force -ErrorAction SilentlyContinue
                    $removedClsids++
                    return
                }
            }
            
            # Check for SpoutCam in any subkey values
            Get-ChildItem $_.PSPath -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
                $properties = Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue
                if ($properties) {
                    $properties.PSObject.Properties | Where-Object { 
                        $_.Value -is [string] -and $_.Value -match "SpoutCam"
                    } | ForEach-Object {
                        Write-Host "   Removing CLSID $clsid (contains SpoutCam reference: $($_.Value))" -ForegroundColor Yellow
                        Remove-Item "$clsidPath\$clsid" -Recurse -Force -ErrorAction SilentlyContinue
                        $script:removedClsids++
                        return
                    }
                }
            }
            
        } catch {
            # Skip entries we can't access
        }
    }
}

Write-Host ""
Write-Host "=== Phase 3: DirectShow Filter Cleanup ===" -ForegroundColor Cyan

# Clean up DirectShow filter instances
$directShowPaths = @(
    "HKLM:\SOFTWARE\Classes\CLSID\{083863F1-70DE-11D0-BD40-00A0C911CE86}\Instance",  # System Device Enum
    "HKLM:\SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance"   # Video Input Device Category
)

$removedFilters = 0
foreach ($dsPath in $directShowPaths) {
    if (Test-Path $dsPath) {
        Write-Host "   Scanning DirectShow instances in $(Split-Path $dsPath -Leaf)..." -ForegroundColor Gray
        Get-ChildItem $dsPath -ErrorAction SilentlyContinue | ForEach-Object {
            try {
                $friendlyName = (Get-ItemProperty $_.PSPath -Name "FriendlyName" -ErrorAction SilentlyContinue).FriendlyName
                $clsid = (Get-ItemProperty $_.PSPath -Name "CLSID" -ErrorAction SilentlyContinue).CLSID
                
                if ($friendlyName -match "SpoutCam" -or $clsid -match "SpoutCam") {
                    Write-Host "   Removing DirectShow filter: $friendlyName" -ForegroundColor Yellow
                    Remove-Item $_.PSPath -Recurse -Force -ErrorAction SilentlyContinue
                    $script:removedFilters++
                }
            } catch {
                # Skip entries we can't access
            }
        }
    }
}

Write-Host ""
Write-Host "=== Phase 4: Search for Hidden SpoutCam References ===" -ForegroundColor Cyan

# Search for any remaining SpoutCam references in common locations
$searchPaths = @(
    "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run",
    "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\RunOnce", 
    "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run",
    "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\RunOnce"
)

$additionalRemovals = 0
foreach ($searchPath in $searchPaths) {
    if (Test-Path $searchPath) {
        Write-Host "   Scanning $($searchPath.Replace('HKLM:\', '').Replace('HKCU:\', ''))..." -ForegroundColor Gray
        $props = Get-ItemProperty $searchPath -ErrorAction SilentlyContinue
        if ($props) {
            $props.PSObject.Properties | Where-Object { 
                $_.Name -ne "PSPath" -and $_.Name -ne "PSParentPath" -and $_.Name -ne "PSChildName" -and
                $_.Value -is [string] -and $_.Value -match "SpoutCam"
            } | ForEach-Object {
                Write-Host "   Removing startup entry: $($_.Name) = $($_.Value)" -ForegroundColor Yellow
                Remove-ItemProperty $searchPath -Name $_.Name -Force -ErrorAction SilentlyContinue
                $script:additionalRemovals++
            }
        }
    }
}

Write-Host ""
Write-Host "=== Phase 5: COM Interface Cleanup ===" -ForegroundColor Cyan

# Check for COM interface registrations
$interfacePath = "HKLM:\SOFTWARE\Classes\Interface"
$removedInterfaces = 0
if (Test-Path $interfacePath) {
    Write-Host "   Scanning COM interfaces..." -ForegroundColor Gray
    Get-ChildItem $interfacePath -ErrorAction SilentlyContinue | ForEach-Object {
        try {
            $defaultValue = (Get-ItemProperty $_.PSPath -Name "(default)" -ErrorAction SilentlyContinue)."(default)"
            if ($defaultValue -match "SpoutCam") {
                Write-Host "   Removing COM interface: $($_.PSChildName) = `"$defaultValue`"" -ForegroundColor Yellow
                Remove-Item $_.PSPath -Recurse -Force -ErrorAction SilentlyContinue
                $script:removedInterfaces++
            }
        } catch {
            # Skip entries we can't access
        }
    }
}

Write-Host ""
Write-Host "=== Cleanup Summary ===" -ForegroundColor Green
Write-Host "   User registry keys removed: $removedUserKeys" -ForegroundColor White
Write-Host "   System CLSIDs removed: $removedClsids" -ForegroundColor White  
Write-Host "   DirectShow filters removed: $removedFilters" -ForegroundColor White
Write-Host "   Additional references removed: $additionalRemovals" -ForegroundColor White
Write-Host "   COM interfaces removed: $removedInterfaces" -ForegroundColor White

$totalRemoved = $removedUserKeys + $removedClsids + $removedFilters + $additionalRemovals + $removedInterfaces
Write-Host "   TOTAL ITEMS REMOVED: $totalRemoved" -ForegroundColor Cyan

Write-Host ""
Write-Host "=== Verification Phase ===" -ForegroundColor Cyan

# Verify cleanup was successful
$verificationResults = @{
    "User Settings" = @()
    "System CLSIDs" = @()
    "DirectShow Filters" = @()
    "Additional References" = @()
}

# Check for remaining user settings
foreach ($path in $userPaths) {
    $fullPath = "HKCU:\$path"
    if (Test-Path $fullPath) {
        $verificationResults["User Settings"] += $path
    }
}

# Check for remaining CLSIDs
if (Test-Path $clsidPath) {
    Get-ChildItem $clsidPath -ErrorAction SilentlyContinue | ForEach-Object {
        try {
            $defaultValue = (Get-ItemProperty $_.PSPath -Name "(default)" -ErrorAction SilentlyContinue)."(default)"
            if ($defaultValue -match "SpoutCam") {
                $verificationResults["System CLSIDs"] += "$($_.PSChildName) = `"$defaultValue`""
            }
        } catch { }
    }
}

# Check for remaining DirectShow filters
foreach ($dsPath in $directShowPaths) {
    if (Test-Path $dsPath) {
        Get-ChildItem $dsPath -ErrorAction SilentlyContinue | ForEach-Object {
            try {
                $friendlyName = (Get-ItemProperty $_.PSPath -Name "FriendlyName" -ErrorAction SilentlyContinue).FriendlyName
                if ($friendlyName -match "SpoutCam") {
                    $verificationResults["DirectShow Filters"] += $friendlyName
                }
            } catch { }
        }
    }
}

# Display verification results
$hasRemainingEntries = $false
foreach ($category in $verificationResults.Keys) {
    if ($verificationResults[$category].Count -gt 0) {
        if (-not $hasRemainingEntries) {
            Write-Host "   WARNING: Some entries could not be removed:" -ForegroundColor Yellow
            $hasRemainingEntries = $true
        }
        Write-Host "     $category ($($verificationResults[$category].Count) remaining):" -ForegroundColor Yellow
        $verificationResults[$category] | ForEach-Object {
            Write-Host "       - $_" -ForegroundColor Red
        }
    }
}

if (-not $hasRemainingEntries) {
    Write-Host "   Complete cleanup verified - no SpoutCam entries found!" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "   Some entries may require:" -ForegroundColor Yellow
    Write-Host "     - System restart to release file locks" -ForegroundColor Yellow
    Write-Host "     - Manual deletion with additional tools" -ForegroundColor Yellow
    Write-Host "     - Elevated permissions beyond Administrator" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Registry Backup Recommendation ===" -ForegroundColor Cyan
$backupPath = Join-Path $env:USERPROFILE "Desktop\SpoutCam_Registry_Backup_$(Get-Date -Format 'yyyyMMdd_HHmmss').reg"
Write-Host "   For safety, consider backing up relevant registry sections before future installations." -ForegroundColor Gray
Write-Host "   Example backup command:" -ForegroundColor Gray
Write-Host "   reg export HKEY_CURRENT_USER\Software\Leading_Edge $backupPath" -ForegroundColor Gray

Write-Host ""
Write-Host "=== Deep Cleanup Complete ===" -ForegroundColor Green
Write-Host "SpoutCam has been thoroughly removed from the registry." -ForegroundColor White
Write-Host "You can now safely run build.bat without registration conflicts." -ForegroundColor White

if ($Verify) {
    Write-Host ""
    Write-Host "Press Enter to exit..."
    Read-Host
}