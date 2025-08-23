# Multiple Virtual Webcams Support

SpoutCam now supports creating multiple virtual webcam instances, allowing you to receive from different Spout senders simultaneously.

## Configuration

By default, SpoutCam creates **4 virtual webcams**:
- **SpoutCam** (original)
- **SpoutCam2** 
- **SpoutCam3**
- **SpoutCam4**

### Changing the Number of Cameras

To change the number of virtual cameras, modify `MAX_SPOUT_CAMERAS` in `source/cam.h`:

```cpp
#define MAX_SPOUT_CAMERAS 4  // Change this value (1-8 recommended)
```

## How It Works

Each virtual webcam:
1. Has a **unique CLSID** (Class ID) for Windows registration
2. Uses **separate registry settings** for independent configuration
3. Can receive from **different Spout senders**
4. Appears as a separate camera device in applications

## Registry Settings

Each camera uses its own registry path:
- **SpoutCam**: `HKEY_CURRENT_USER\Software\Leading Edge\SpoutCam`
- **SpoutCam2**: `HKEY_CURRENT_USER\Software\Leading Edge\SpoutCam2`  
- **SpoutCam3**: `HKEY_CURRENT_USER\Software\Leading Edge\SpoutCam3`
- **SpoutCam4**: `HKEY_CURRENT_USER\Software\Leading Edge\SpoutCam4`

This allows each camera to have independent settings for:
- **fps** - Frame rate (10, 15, 25, 30, 50, 60 fps)
- **resolution** - Output resolution or "use sender resolution"
- **mirror** - Mirror image horizontally
- **swap** - Swap RGB ↔ BGR
- **flip** - Flip image vertically
- **senderstart** - Lock to specific sender name
- **sendername** - Currently active sender (auto-saved)

## Usage Examples

1. **OBS Studio**: All cameras appear in the Video Capture Device source dropdown
2. **Zoom/Teams**: Each camera shows up as a separate webcam option
3. **Streaming Software**: Use different cameras for different scenes/overlays
4. **Multi-camera Setups**: Route different Spout senders to different applications

## Technical Details

### CLSID Generation
Each camera gets a unique CLSID by incrementing the last byte:
- SpoutCam: `{8E14549A-DB61-4309-AFA1-3578E927E933}`
- SpoutCam2: `{8E14549A-DB61-4309-AFA1-3578E927E934}`
- SpoutCam3: `{8E14549A-DB61-4309-AFA1-3578E927E935}`
- etc.

### Factory Functions
Each camera has its own factory function (`CreateCamera0`, `CreateCamera1`, etc.) to ensure proper instantiation with the correct configuration.

## Building and Registration

1. **Build**: Compile the project - all cameras are built into the single `.ax` file
2. **Register**: Use `regsvr32 SpoutCam64.ax` - registers all cameras at once  
3. **Unregister**: Use `regsvr32 /u SpoutCam64.ax` - unregisters all cameras

## Compatibility

- **Windows 10/11**: Full support
- **Applications**: Works with any DirectShow-compatible software
- **Spout SDK**: Compatible with Spout 2.007+
- **Performance**: Minimal overhead per additional camera

## Troubleshooting

- **Cameras not appearing**: Ensure registration was successful
- **Same image on all cameras**: Check that different senders are running or configure different starting senders
- **Settings not saving**: Verify registry permissions
- **Performance issues**: Reduce number of active cameras or lower resolution/fps

## Notes

- Each virtual webcam runs independently
- Settings are stored per-camera in the registry
- All cameras share the same executable but have separate configurations
- Original SpoutCam behavior is preserved for backward compatibility