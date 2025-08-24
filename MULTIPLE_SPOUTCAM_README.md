# Multiple SpoutCam Setup Guide

This guide explains how to build and use multiple SpoutCam instances (4 cameras) to receive from different Spout senders simultaneously.

## Overview

SpoutCam can be configured to create multiple virtual cameras by changing the camera name and CLSID. This allows you to have:
- **SpoutCam1** - Virtual camera #1
- **SpoutCam2** - Virtual camera #2  
- **SpoutCam3** - Virtual camera #3
- **SpoutCam4** - Virtual camera #4

Each camera can connect to different Spout senders and appear as separate video sources in applications like OBS, Zoom, Teams, etc.

## Quick Start

### Step 1: Build All SpoutCam Instances
```batch
# Run this from the project root directory
build_multiple_spoutcams.bat
```

This will create 8 files in `binaries\MULTIPLE_SPOUTCAM\`:
- `SpoutCam1_64.ax` and `SpoutCam1_32.ax` (64-bit and 32-bit)
- `SpoutCam2_64.ax` and `SpoutCam2_32.ax` (64-bit and 32-bit)
- `SpoutCam3_64.ax` and `SpoutCam3_32.ax` (64-bit and 32-bit)  
- `SpoutCam4_64.ax` and `SpoutCam4_32.ax` (64-bit and 32-bit)

### Step 2: Register All Instances
```batch
# Right-click and "Run as Administrator"
register_all_spoutcams.bat
```

### Step 3: Configure Each Camera
Use `SpoutCamSettings.exe` to configure:
- Frame rate (30fps recommended)
- Resolution (or "Active Sender" to match source)
- Starting sender name (to lock each camera to specific sender)
- Mirror/Flip/Swap options

### Step 4: Use in Applications
The cameras will appear as:
- **SpoutCam1**
- **SpoutCam2** 
- **SpoutCam3**
- **SpoutCam4**

## Requirements

- **Visual Studio 2022** (Community, Professional, or Enterprise)
- **Windows 10/11** (64-bit recommended)
- **Administrator rights** for registration
- **Spout-enabled applications** sending content

## Technical Details

### How It Works

Each SpoutCam instance has:
1. **Unique Name**: Defined in `cam.h` as `SPOUTCAMNAME`
2. **Unique CLSID**: Defined in `dll.cpp`, last byte incremented (0x33→0x34→0x35→0x36)
3. **Separate .ax File**: Built and registered independently

### CLSID Values Used
```cpp
// SpoutCam1 (original)
DEFINE_GUID(CLSID_SpoutCam, 0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x33);

// SpoutCam2  
DEFINE_GUID(CLSID_SpoutCam, 0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x34);

// SpoutCam3
DEFINE_GUID(CLSID_SpoutCam, 0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x35);

// SpoutCam4
DEFINE_GUID(CLSID_SpoutCam, 0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x36);
```

## Configuration Tips

### Locking Cameras to Specific Senders

1. Open **SpoutCamSettings.exe**
2. Set **Starting Sender** name for each camera:
   - SpoutCam1 → "Sender1"  
   - SpoutCam2 → "Sender2"
   - SpoutCam3 → "Sender3"
   - SpoutCam4 → "Sender4"
3. Each camera will only connect to its designated sender

### Performance Optimization

- Use **30fps** for best compatibility
- Set **fixed resolution** if sender resolution varies
- Enable only the cameras you need
- Consider 64-bit builds for better performance

## Troubleshooting

### Build Issues
- Ensure Visual Studio 2022 is installed
- Check that all dependencies are built (BaseClasses, SpoutDX)
- Run build script from project root directory

### Registration Issues  
- **Must run as Administrator** for registration
- Unregister old instances before rebuilding
- Check Windows Event Viewer for detailed errors

### Camera Not Appearing
- Verify registration completed successfully
- Restart the application looking for cameras
- Check if antivirus is blocking .ax files

### No Video/Static Display
- Ensure Spout sender is running first
- Check sender name matches in SpoutCamSettings
- Verify sender is actually sending data

## Uninstalling

To remove all SpoutCam instances:
```batch
# Right-click and "Run as Administrator"  
unregister_all_spoutcams.bat
```

## Manual Building (Advanced)

If you need to build manually:

1. **Edit source files for each instance:**
   ```cpp
   // In cam.h
   #define SPOUTCAMNAME "SpoutCam1"  // Change name
   
   // In dll.cpp  
   DEFINE_GUID(CLSID_SpoutCam, 0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x33);
   // Increment last byte: 0x33 → 0x34 → 0x35 → 0x36
   ```

2. **Build with Visual Studio 2022:**
   ```batch
   msbuild SpoutCamDX.sln /p:Configuration=Release /p:Platform=x64
   ```

3. **Copy and rename output:**
   ```batch
   copy "x64\Release\SpoutCam64.ax" "SpoutCam1_64.ax"
   ```

4. **Register manually:**
   ```batch
   regsvr32.exe "SpoutCam1_64.ax"
   ```

## License & Credits

Based on the original SpoutCam project with modifications for multiple instance support.
See main README.md for full credits and license information.