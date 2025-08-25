# Dynamic Virtual Webcam System

SpoutCam now features a **fully dynamic virtual webcam system** with **unlimited cameras** and **custom naming**. No more hard-coded limits or index-based restrictions!

## Key Features

✨ **Unlimited Cameras**: Create as many virtual webcams as needed  
🏷️ **Custom Names**: Use descriptive names like "OBS Camera", "Zoom Background", "Stream Overlay"  
⚡ **Dynamic Creation**: Add/remove cameras on-demand through the UI  
🔄 **No Restart Required**: Changes take effect immediately  
📝 **Name-Based Management**: All operations use human-readable camera names

## How It Works

The system automatically:
1. **Generates unique CLSIDs** for each camera based on its name
2. **Creates independent registry settings** for each camera
3. **Manages DirectShow registration** dynamically
4. **Persists camera configurations** across reboots

## Camera Management

### Creating New Cameras

1. **Launch SpoutCamSettings** (run as Administrator)
2. **Click "Add New Camera"** button
3. **Enter a descriptive name** (e.g., "OBS Main Camera", "Zoom Virtual Cam")
4. **Click "Activate"** to make it available to applications

### Camera Naming Rules

✅ **Valid**: Letters, numbers, spaces, hyphens, underscores  
✅ **Examples**: "SpoutCam", "OBS Camera", "Stream-Overlay", "Zoom_Background"  
❌ **Invalid**: Special characters like @#$%^&*()  
📏 **Length**: 1-64 characters

### Registry Structure

Each camera gets its own registry path based on name:
```
HKEY_CURRENT_USER\Software\Leading Edge\SpoutCam\[CameraName]
├── fps          # Frame rate setting
├── resolution   # Output resolution
├── mirror       # Horizontal mirroring
├── flip         # Vertical flipping  
├── swap         # RGB/BGR color swap
└── sendername   # Active Spout sender
```

This allows each camera to have independent settings for:
- **fps** - Frame rate (10, 15, 25, 30, 50, 60 fps)
- **resolution** - Output resolution or "use sender resolution"
- **mirror** - Mirror image horizontally
- **swap** - Swap RGB ↔ BGR
- **flip** - Flip image vertically
- **senderstart** - Lock to specific sender name
- **sendername** - Currently active sender (auto-saved)

## Enhanced Configuration UI

Each virtual camera now has an **improved settings dialog** with:

### 🎛️ **New UI Features:**
- **Camera Identification** - Shows which camera you're configuring (SpoutCam, SpoutCam2, etc.)
- **Available Senders List** - Dropdown showing currently running Spout senders
- **Refresh Button** - Update the senders list in real-time
- **Source Selection** - Choose different senders for each camera:
  - **Auto (Active Sender)** - Use the currently active sender
  - **Specific Sender** - Lock to a particular sender by name
  - **Custom Name** - Type any sender name manually

### 📋 **SpoutCamSettings UI:**
```
SpoutCam Dynamic Camera Manager
┌──────────────────────────────────────────┐
│ Camera List:                             │
│ ┌──────────────────────────────────────┐ │
│ │ Name             │ Status   │ Settings││ │
│ │ OBS Main Camera  │ Active   │ Yes     ││ │
│ │ Zoom Background  │ Inactive │ Yes     ││ │
│ │ Stream Overlay   │ Active   │ No      ││ │
│ └──────────────────────────────────────┘ │
├──────────────────────────────────────────┤
│ [Add New Camera] [Activate] [Deactivate] │
│ [Properties] [Remove] [Refresh List]     │
└──────────────────────────────────────────┘
```

## Usage Examples

1. **OBS Studio**: All cameras appear in the Video Capture Device source dropdown
2. **Zoom/Teams**: Each camera shows up as a separate webcam option  
3. **Streaming Software**: Use different cameras for different scenes/overlays
4. **Multi-camera Setups**: Route different Spout senders to different applications

### 🎬 **Multi-Camera Workflow:**
1. **Create cameras with descriptive names** ("OBS Main", "Zoom BG", "Stream Overlay")
2. **Activate desired cameras** through SpoutCamSettings
3. **Configure each camera independently** - different FPS, resolution, Spout senders
4. **Use in applications** - each appears as separate webcam device
5. **Right-click camera properties** to change Spout sender or settings

## Technical Details

### Dynamic CLSID Generation
Each camera gets a **unique CLSID generated from its name**:
- Uses hash-based generation for consistency
- Same name always produces same CLSID
- Eliminates fixed slot limitations
- Examples:
  - "SpoutCam" → `{8E14549A-DB61-4309-AFA1-3578E927E933}`
  - "OBS Camera" → `{8E14549A-DB61-4309-AFA1-[unique-hash]}`
  - "Stream Overlay" → `{8E14549A-DB61-4309-AFA1-[unique-hash]}`

### Dynamic Factory System
No more hard-coded factory functions:
- **Single dynamic factory** resolves cameras by CLSID
- **Name-based instantiation** throughout the system
- **Runtime camera discovery** from registry
- **Unlimited scalability** - no MAX_CAMERAS limit

## Building and Registration

1. **Build**: `./build.bat` - compiles single `.ax` file with dynamic system
2. **Register**: `regsvr32 SpoutCam64.ax` - registers the dynamic framework  
3. **Create Cameras**: Use SpoutCamSettings UI to create/activate cameras
4. **Unregister**: `regsvr32 /u SpoutCam64.ax` - removes all camera registrations

## Compatibility

- **Windows 10/11**: Full support
- **Applications**: Works with any DirectShow-compatible software
- **Spout SDK**: Compatible with Spout 2.007+
- **Performance**: Minimal overhead per additional camera

## Advanced Usage

### API Integration
Developers can use the C++ API:
```cpp
auto manager = SpoutCam::DynamicCameraManager::GetInstance();

// Create camera
auto camera = manager->CreateCamera("My Custom Camera");

// Activate/deactivate
manager->ActivateCamera("My Custom Camera");
manager->DeactivateCamera("My Custom Camera");

// List cameras
auto names = manager->GetCameraNames();
auto active = manager->GetActiveCameraNames();

// Validate names
bool valid = manager->ValidateCameraName("New Camera Name");
```

### Migration from Legacy System
- **Existing cameras preserved**: "SpoutCam", "SpoutCam2", etc. still work
- **Registry compatibility**: Old settings automatically migrated
- **Gradual transition**: Can mix legacy numbered and new named cameras

## Troubleshooting

- **Camera not appearing**: Check if activated in SpoutCamSettings
- **Permission errors**: Run SpoutCamSettings as Administrator
- **Name conflicts**: Use unique, descriptive names
- **Performance**: Limit active cameras based on system capabilities
- **Registry cleanup**: Use provided PowerShell cleanup scripts if needed

## Benefits Over Legacy System

✅ **No MAX_CAMERAS limit** - create unlimited cameras  
✅ **Descriptive naming** - "OBS Camera" vs "SpoutCam4"  
✅ **Dynamic management** - add/remove without restart  
✅ **Cleaner codebase** - no hard-coded factory functions  
✅ **Better UX** - intuitive camera management UI  
✅ **Future-proof** - scalable architecture for new features