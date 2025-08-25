Write-Host "=== SpoutCam Registration Test ===" -ForegroundColor Green

# Test 1: Check for SpoutCam CLSIDs
Write-Host "`n1. Scanning for registered SpoutCam CLSIDs..." -ForegroundColor Yellow

$clsids = Get-ChildItem "HKLM:\SOFTWARE\Classes\CLSID" -ErrorAction SilentlyContinue | ForEach-Object {
    $clsid = $_.PSChildName
    $defaultValue = (Get-ItemProperty $_.PSPath -Name "(default)" -ErrorAction SilentlyContinue).'(default)'
    
    if ($defaultValue -match "SpoutCam|Settings") {
        Write-Host "   Found CLSID: $clsid = `"$defaultValue`"" -ForegroundColor Cyan
        $clsid
    }
}

if (-not $clsids) {
    Write-Host "   No SpoutCam CLSIDs found" -ForegroundColor Red
}

# Test 2: Check DirectShow filter registration
Write-Host "`n2. Checking DirectShow filter registration..." -ForegroundColor Yellow

$directShowPath = "HKLM:\SOFTWARE\Classes\CLSID\{083863F1-70DE-11D0-BD40-00A0C911CE86}\Instance"
if (Test-Path $directShowPath) {
    Get-ChildItem $directShowPath -ErrorAction SilentlyContinue | ForEach-Object {
        $instanceId = $_.PSChildName
        $friendlyName = (Get-ItemProperty $_.PSPath -Name "FriendlyName" -ErrorAction SilentlyContinue).FriendlyName
        
        if ($friendlyName -match "SpoutCam") {
            Write-Host "   DirectShow Filter: $instanceId = `"$friendlyName`"" -ForegroundColor Cyan
        }
    }
} else {
    Write-Host "   DirectShow Instance registry path not found" -ForegroundColor Red
}

# Test 3: Check if DLL exists and can be loaded
Write-Host "`n3. Testing DLL availability..." -ForegroundColor Yellow

$dllPath64 = "binaries\SPOUTCAM\SpoutCam64\SpoutCam64.ax"
$dllPath32 = "binaries\SPOUTCAM\SpoutCam32\SpoutCam32.ax"

if (Test-Path $dllPath64) {
    $fileInfo = Get-Item $dllPath64
    Write-Host "   ✓ Found SpoutCam64.ax (Size: $($fileInfo.Length) bytes, Modified: $($fileInfo.LastWriteTime))" -ForegroundColor Green
} else {
    Write-Host "   ✗ SpoutCam64.ax not found" -ForegroundColor Red
}

if (Test-Path $dllPath32) {
    $fileInfo = Get-Item $dllPath32
    Write-Host "   ✓ Found SpoutCam32.ax (Size: $($fileInfo.Length) bytes, Modified: $($fileInfo.LastWriteTime))" -ForegroundColor Green
} else {
    Write-Host "   ✗ SpoutCam32.ax not found" -ForegroundColor Red
}

# Test 4: Check for dynamic camera registry entries
Write-Host "`n4. Checking dynamic camera registry entries..." -ForegroundColor Yellow

$dynamicCameraPath = "HKCU:\Software\Leading Edge\SpoutCam"
if (Test-Path $dynamicCameraPath) {
    $cameras = Get-ChildItem $dynamicCameraPath -ErrorAction SilentlyContinue | Where-Object { $_.PSChildName -match "SpoutCam" }
    if ($cameras) {
        Write-Host "   Found $($cameras.Count) dynamic camera(s):" -ForegroundColor Green
        $cameras | ForEach-Object {
            $cameraName = $_.PSChildName
            Write-Host "     - $cameraName" -ForegroundColor Cyan
        }
    } else {
        Write-Host "   No dynamic cameras found in registry" -ForegroundColor Yellow
    }
} else {
    Write-Host "   Dynamic camera registry path not found" -ForegroundColor Red
}

# Test 5: Try to test registration functions by checking exports
Write-Host "`n5. Testing DLL function exports..." -ForegroundColor Yellow

try {
    if (Test-Path $dllPath64) {
        # Use dumpbin to check exports if available, otherwise skip
        $dumpbin = Get-Command "dumpbin.exe" -ErrorAction SilentlyContinue
        if ($dumpbin) {
            Write-Host "   Checking exports with dumpbin..." -ForegroundColor Gray
            $exports = & dumpbin /exports $dllPath64 2>$null | Select-String "RegisterCameraByName|UnregisterCameraByName"
            if ($exports) {
                Write-Host "   ✓ Registration functions found in exports" -ForegroundColor Green
            } else {
                Write-Host "   ? Could not verify registration function exports" -ForegroundColor Yellow
            }
        } else {
            Write-Host "   Skipping export check (dumpbin not available)" -ForegroundColor Gray
        }
    }
} catch {
    Write-Host "   Export check failed: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host "`n=== Test Complete ===" -ForegroundColor Green
Write-Host "Press any key to continue..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")