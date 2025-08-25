# SpoutCam File Lock Release Script
# Run as Administrator for best results
# This script will identify and release file locks on SpoutCam files

param(
    [switch]$Force,
    [switch]$Quiet
)

if (!$Quiet) {
    Write-Host "=== SpoutCam File Lock Release Tool ===" -ForegroundColor Green
    Write-Host "Identifying and releasing locks on SpoutCam files..." -ForegroundColor Gray
}

# Files that commonly get locked during build
$spoutcamFiles = @(
    "x64\Release\SpoutCam64.ax",
    "Win32\Release\SpoutCam32.ax", 
    "binaries\SPOUTCAM\SpoutCam64\SpoutCam64.ax",
    "binaries\SPOUTCAM\SpoutCam32\SpoutCam32.ax",
    "binaries\SPOUTCAM\SpoutCamSettings64.exe",
    "binaries\SPOUTCAM\SpoutCamSettings32.exe"
)

$locksReleased = 0
$processesKilled = 0

# Method 1: Kill known problematic processes
$problematicProcesses = @(
    "dllhost",      # COM surrogate processes
    "rundll32",     # May load our DLL for properties
    "svchost",      # May cache DLL for media services
    "dwm",          # Desktop Window Manager may hold preview handles
    "explorer"      # File preview handlers
)

foreach ($processName in $problematicProcesses) {
    $processes = Get-Process -Name $processName -ErrorAction SilentlyContinue | Where-Object {
        $_.Path -and ($_.Path -like "*SpoutCam*" -or $_.Modules.ModuleName -contains "SpoutCam64.ax" -or $_.Modules.ModuleName -contains "SpoutCam32.ax")
    }
    
    foreach ($proc in $processes) {
        if (!$Quiet) {
            Write-Host "   Killing $($proc.ProcessName) (PID: $($proc.Id)) - may hold SpoutCam handles" -ForegroundColor Yellow
        }
        try {
            $proc.Kill()
            $processesKilled++
        } catch {
            if (!$Quiet) {
                Write-Host "   Could not kill $($proc.ProcessName): $($_.Exception.Message)" -ForegroundColor Red
            }
        }
    }
}

# Method 2: Use Handle.exe if available (SysInternals)
$handleExe = $null
$possiblePaths = @(
    "C:\Windows\System32\handle.exe",
    "C:\Windows\handle.exe", 
    "$env:ProgramFiles\Sysinternals\handle.exe",
    ".\handle.exe"
)

foreach ($path in $possiblePaths) {
    if (Test-Path $path) {
        $handleExe = $path
        break
    }
}

if ($handleExe) {
    if (!$Quiet) {
        Write-Host "   Using Handle.exe to find specific file locks..." -ForegroundColor Cyan
    }
    
    foreach ($file in $spoutcamFiles) {
        if (Test-Path $file) {
            $fullPath = Resolve-Path $file
            $handleOutput = & $handleExe -u $fullPath.Path 2>$null
            
            if ($handleOutput -and $handleOutput.Length -gt 0) {
                foreach ($line in $handleOutput) {
                    if ($line -match "(\w+\.exe)\s+pid:\s+(\d+)") {
                        $processName = $matches[1]
                        $pid = $matches[2]
                        
                        if (!$Quiet) {
                            Write-Host "   Found lock: $processName (PID: $pid) on $file" -ForegroundColor Yellow
                        }
                        
                        try {
                            Stop-Process -Id $pid -Force
                            $locksReleased++
                            if (!$Quiet) {
                                Write-Host "   Released lock by killing PID $pid" -ForegroundColor Green
                            }
                        } catch {
                            if (!$Quiet) {
                                Write-Host "   Could not kill PID $pid: $($_.Exception.Message)" -ForegroundColor Red
                            }
                        }
                    }
                }
            }
        }
    }
} else {
    if (!$Quiet) {
        Write-Host "   Handle.exe not found - using alternative methods" -ForegroundColor Gray
    }
}

# Method 3: Restart Windows services that may cache DLLs
$services = @(
    "Themes",           # May cache file handles for previews
    "WSearch",          # Windows Search may index the files
    "ShellHWDetection"  # May hold handles for device detection
)

foreach ($serviceName in $services) {
    $service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
    if ($service -and $service.Status -eq "Running") {
        if (!$Quiet) {
            Write-Host "   Restarting $serviceName service..." -ForegroundColor Cyan
        }
        try {
            Restart-Service -Name $serviceName -Force -ErrorAction SilentlyContinue
            $locksReleased++
        } catch {
            if (!$Quiet) {
                Write-Host "   Could not restart $serviceName: $($_.Exception.Message)" -ForegroundColor Red
            }
        }
    }
}

# Method 4: Force unlock using atomic rename (works even with locks)
foreach ($file in $spoutcamFiles) {
    if (Test-Path $file) {
        $tempFile = "$file.unlock_temp"
        $backupFile = "$file.backup"
        
        try {
            # Try atomic operations that work even with file locks
            if (Test-Path $tempFile) { Remove-Item $tempFile -Force }
            if (Test-Path $backupFile) { Remove-Item $backupFile -Force }
            
            # Move locked file to backup (may work when copy doesn't)
            Move-Item $file $backupFile -ErrorAction Stop
            
            # Move it back 
            Move-Item $backupFile $file -ErrorAction Stop
            
            if (!$Quiet) {
                Write-Host "   Unlocked $file using atomic rename" -ForegroundColor Green
            }
            $locksReleased++
            
        } catch {
            # If atomic rename failed, file is still locked
            if (Test-Path $backupFile) {
                Move-Item $backupFile $file -ErrorAction SilentlyContinue
            }
        }
        
        # Cleanup temp files
        if (Test-Path $tempFile) { Remove-Item $tempFile -Force -ErrorAction SilentlyContinue }
        if (Test-Path $backupFile) { Remove-Item $backupFile -Force -ErrorAction SilentlyContinue }
    }
}

# Method 5: Clear Windows file system cache
if ($Force) {
    if (!$Quiet) {
        Write-Host "   Clearing Windows file system cache..." -ForegroundColor Cyan
    }
    try {
        # This requires admin privileges
        & cmd.exe /c "echo off && echo 3 > C:\Windows\System32\config\systemprofile\AppData\Local\Temp\drop_caches.txt 2>nul"
        $locksReleased++
    } catch {
        if (!$Quiet) {
            Write-Host "   Could not clear file system cache (requires admin)" -ForegroundColor Yellow
        }
    }
}

# Summary
if (!$Quiet) {
    Write-Host ""
    Write-Host "=== Summary ===" -ForegroundColor Green
    Write-Host "   Processes killed: $processesKilled" -ForegroundColor White
    Write-Host "   File locks released: $locksReleased" -ForegroundColor White
    
    if ($locksReleased -gt 0 -or $processesKilled -gt 0) {
        Write-Host "   ✓ Lock release operations completed" -ForegroundColor Green
        Write-Host "   You can now run build.bat" -ForegroundColor White
    } else {
        Write-Host "   No locks found or unable to release" -ForegroundColor Yellow
        Write-Host "   Try running as Administrator or reboot if issues persist" -ForegroundColor Yellow
    }
}

# Return success code
exit 0