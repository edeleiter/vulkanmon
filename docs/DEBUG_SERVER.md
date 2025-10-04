# VulkanMon Debug Server Documentation

## Overview

The VulkanMon Debug Server provides a REST API for real-time game state inspection, entity manipulation, and visual debugging. It enables AI-assisted development by allowing Claude to interact with the running game via HTTP requests.

**Key Features:**
- HTTP REST API on `localhost:3000`
- Frame capture to PNG (Vulkan → PNG pipeline)
- Camera control and inspection
- Entity/component queries
- Thread-safe command queue with rate limiting
- Zero performance impact on game loop

## Architecture

### Design Principles

1. **Thread Safety**: HTTP server runs on separate thread, commands queued for main thread execution
2. **Non-Blocking**: Commands execute during frame update, never block rendering
3. **Rate Limiting**: 100ms minimum between commands prevents game loop overload
4. **Dependency Injection**: Application* passed to DebugServer (not singleton)

### Component Diagram

```
┌─────────────────┐         ┌──────────────────┐
│  HTTP Client    │────────>│  Debug Server    │
│  (curl/Claude)  │<────────│  (Port 3000)     │
└─────────────────┘         └──────────────────┘
                                     │
                                     │ Command Queue
                                     │ (Thread-Safe)
                                     ▼
                            ┌──────────────────┐
                            │   Application    │
                            │   (Main Thread)  │
                            └──────────────────┘
                                     │
                         ┌───────────┼───────────┐
                         │           │           │
                    ┌────▼───┐  ┌───▼────┐  ┌──▼─────┐
                    │ World  │  │Renderer│  │ Input  │
                    │  ECS   │  │ Vulkan │  │Handler │
                    └────────┘  └────────┘  └────────┘
```

### Thread Model

- **HTTP Server Thread**: Receives requests, validates commands, queues for execution
- **Main Game Thread**: Processes command queue during `Application::processFrame()`
- **Synchronization**: `std::mutex` protects command queue, no blocking on game thread

## API Reference

### Base URL
```
http://localhost:3000
```

### Authentication
None (localhost-only for security)

---

## State Query Endpoints (GET)

### GET /api/game
**Description**: Get overall game state (FPS, frame time, running status)

**Response**:
```json
{
  "running": true,
  "fps": 3200.5,
  "frame_time_ms": 0.312
}
```

**Example**:
```bash
curl http://localhost:3000/api/game | jq
```

---

### GET /api/entities
**Description**: List all ECS entities with components

**Response**:
```json
{
  "entities": [
    {
      "id": 0,
      "position": [0.0, 1.0, 0.0],
      "rotation": [0.0, 0.0, 0.0, 1.0],
      "scale": [1.0, 1.0, 1.0],
      "mesh": "cube.obj",
      "material_id": 0,
      "visible": true
    },
    {
      "id": 1,
      "position": [5.0, 2.0, 3.0],
      "rotation": [0.0, 0.707, 0.0, 0.707],
      "scale": [2.0, 2.0, 2.0],
      "mesh": "sphere.obj",
      "material_id": 1,
      "visible": true
    }
  ]
}
```

**Example**:
```bash
curl http://localhost:3000/api/entities | jq
```

---

### GET /api/camera
**Description**: Get complete camera state (position, rotation, FOV, basis vectors)

**Response**:
```json
{
  "position": [0.0, 5.0, 10.0],
  "rotation_euler_rad": [0.0, 0.0, 0.0],
  "rotation_euler_deg": [0.0, 0.0, 0.0],
  "forward": [0.0, 0.0, -1.0],
  "up": [0.0, 1.0, 0.0],
  "right": [1.0, 0.0, 0.0],
  "fov": 45.0,
  "near_plane": 0.1,
  "far_plane": 1000.0,
  "aspect_ratio": 1.333,
  "projection_type": "perspective"
}
```

**Example**:
```bash
curl http://localhost:3000/api/camera | jq
```

---

### GET /api/camera/position
**Description**: Get camera position only (lightweight query)

**Response**:
```json
{
  "x": 0.0,
  "y": 5.0,
  "z": 10.0
}
```

**Example**:
```bash
curl http://localhost:3000/api/camera/position | jq
```

---

### GET /api/input
**Description**: Get current input state (keyboard, mouse)

**Response**:
```json
{
  "keys": {
    "W": false,
    "A": false,
    "S": false,
    "D": false
  },
  "mouse": {
    "x": 400,
    "y": 300,
    "left_button": false,
    "right_button": false
  }
}
```

**Example**:
```bash
curl http://localhost:3000/api/input | jq
```

---

## Command Endpoints (POST)

### POST /api/camera/position
**Description**: Set camera position

**Request Body**:
```json
{
  "x": 10.0,
  "y": 5.0,
  "z": 15.0
}
```

**Response**:
```json
{
  "success": true,
  "message": "Camera position command queued"
}
```

**Example**:
```bash
curl -X POST http://localhost:3000/api/camera/position \
  -H "Content-Type: application/json" \
  -d '{"x": 10.0, "y": 5.0, "z": 15.0}'
```

---

### POST /api/camera/teleport
**Description**: Teleport camera to position (alias for /api/camera/position)

**Request Body**:
```json
{
  "x": 0.0,
  "y": 10.0,
  "z": 0.0
}
```

**Response**:
```json
{
  "success": true,
  "message": "Camera teleport command queued"
}
```

**Example**:
```bash
curl -X POST http://localhost:3000/api/camera/teleport \
  -H "Content-Type: application/json" \
  -d '{"x": 0.0, "y": 10.0, "z": 0.0}'
```

---

### POST /api/camera/rotation
**Description**: Set camera rotation (Euler angles in degrees)

**Request Body**:
```json
{
  "pitch": 30.0,
  "yaw": 45.0,
  "roll": 0.0
}
```

**Response**:
```json
{
  "success": true,
  "message": "Camera rotation command queued"
}
```

**Example**:
```bash
curl -X POST http://localhost:3000/api/camera/rotation \
  -H "Content-Type: application/json" \
  -d '{"pitch": 30.0, "yaw": 45.0, "roll": 0.0}'
```

---

### POST /api/camera/fov
**Description**: Set camera field of view

**Request Body**:
```json
{
  "fov": 60.0
}
```

**Response**:
```json
{
  "success": true,
  "message": "Camera FOV command queued"
}
```

**Example**:
```bash
curl -X POST http://localhost:3000/api/camera/fov \
  -H "Content-Type: application/json" \
  -d '{"fov": 60.0}'
```

---

## Frame Capture Endpoint

### GET /api/capture
**Description**: Capture current frame to PNG file

**Query Parameters**:
- `filename` (optional): Output filename (default: `debug_frame.png`)

**Response**:
```json
{
  "success": true,
  "message": "Frame capture command queued",
  "filename": "my_screenshot.png",
  "note": "Frame will be captured on next frame render"
}
```

**Example**:
```bash
# Default filename
curl http://localhost:3000/api/capture

# Custom filename
curl "http://localhost:3000/api/capture?filename=test_frame.png"
```

**Output**: PNG file saved in working directory (typically `build/` directory)

---

## Frame Capture Technical Details

### Vulkan Pipeline

1. **Command Queue**: HTTP GET request queues `capture_frame` command
2. **Main Thread Execution**: Command processed during `Application::processFrame()`
3. **Staging Buffer Creation**:
   - `VK_BUFFER_USAGE_TRANSFER_DST_BIT`
   - `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`
   - Size: `width × height × 4` bytes (RGBA)

4. **Image Layout Transitions**:
   ```
   PRESENT_SRC_KHR → TRANSFER_SRC_OPTIMAL → PRESENT_SRC_KHR
   ```

5. **Image Copy**: `vkCmdCopyImageToBuffer` transfers GPU framebuffer to CPU memory
6. **Format Conversion**: BGRA → RGBA if needed (VK_FORMAT_B8G8R8A8_*)
7. **PNG Encoding**: `stb_image_write` saves compressed PNG

### Performance Characteristics

- **Resolution**: 800×600 (default window size)
- **File Size**: ~23KB (PNG compressed)
- **Capture Time**: Sub-millisecond (non-blocking)
- **Memory**: 1.83 MB staging buffer (temporary)

### Supported Formats

- `VK_FORMAT_B8G8R8A8_UNORM` → RGB conversion
- `VK_FORMAT_B8G8R8A8_SRGB` → RGB conversion
- `VK_FORMAT_R8G8B8A8_UNORM` → Direct copy
- `VK_FORMAT_R8G8B8A8_SRGB` → Direct copy

---

## Error Handling

### Rate Limiting

**Error**: Commands rejected if sent faster than 100ms apart
```json
{
  "success": false,
  "error": "Command queue rejected (rate limit or server not ready)"
}
```

### Invalid Command

**Error**: Unknown command type
```bash
HTTP 400 Bad Request
```
```json
{
  "success": false,
  "error": "Invalid command type"
}
```

### JSON Parse Error

**Error**: Malformed JSON in request body
```bash
HTTP 400 Bad Request
```
```json
{
  "success": false,
  "error": "JSON parse error: <details>"
}
```

### Server Not Ready

**Error**: Application or systems not initialized
```bash
HTTP 500 Internal Server Error
```
```json
{
  "success": false,
  "error": "Renderer not available"
}
```

---

## Development Workflow

### Starting the Debug Server

The debug server starts automatically when the application launches:

```bash
cd build
./Debug/vulkanmon.exe

# Console output:
# [INFO] [DebugServer] Started on port 3000
# [INFO] Debug server started on port 3000
# [INFO] [DebugServer] HTTP server listening on localhost:3000
```

### Health Check

Verify server is running:
```bash
curl http://localhost:3000/health
```

Response:
```json
{
  "status": "ok"
}
```

### Debugging with curl

```bash
# Query game state
curl http://localhost:3000/api/game | jq

# List entities
curl http://localhost:3000/api/entities | jq '.entities[] | {id, mesh, visible}'

# Teleport camera
curl -X POST http://localhost:3000/api/camera/teleport \
  -H "Content-Type: application/json" \
  -d '{"x": 0, "y": 20, "z": 30}'

# Capture screenshot
curl http://localhost:3000/api/capture?filename=debug_$(date +%s).png
```

### Python Example

```python
import requests
import json

BASE_URL = "http://localhost:3000"

# Get camera state
response = requests.get(f"{BASE_URL}/api/camera")
camera = response.json()
print(f"Camera position: {camera['position']}")

# Teleport camera
payload = {"x": 10.0, "y": 5.0, "z": 15.0}
response = requests.post(f"{BASE_URL}/api/camera/teleport", json=payload)
print(f"Teleport result: {response.json()}")

# Capture frame
response = requests.get(f"{BASE_URL}/api/capture?filename=test.png")
result = response.json()
print(f"Capture: {result['message']}")
```

---

## Unit Testing

### Test Coverage

**test_DebugServer.cpp** (75 assertions, 8 test cases):
- Command validation (14 valid command types)
- JSON serialization (camera state, entity list, game state)
- Command payload parsing
- Rate limiting logic
- Error handling
- Component queries
- HTTP response formats
- Camera vector math validation

**test_FrameCapture.cpp** (32 assertions, 8 test cases):
- PNG file creation and validation
- RGBA/BGRA format handling
- BGR→RGB conversion correctness
- Error handling (invalid paths, null data, zero dimensions)
- Vulkan format detection
- Memory layout validation
- Performance characteristics

### Running Tests

```bash
cd build/tests_cpp
./Debug/vulkanmon_tests.exe "[DebugServer]"
./Debug/vulkanmon_tests.exe "[FrameCapture]"
```

---

## Security Considerations

### Localhost-Only Binding

Server binds to `127.0.0.1` (localhost) only - not accessible from network:
```cpp
server.listen("127.0.0.1", 3000);
```

### No Authentication

Since the server is localhost-only and intended for development debugging, no authentication is required. **Do not expose this server to the network.**

### Rate Limiting

100ms minimum between commands prevents accidental DoS on game loop.

---

## Future Enhancements

### Planned Features

1. **Entity Spawning**: `POST /api/entities` - Create entities at runtime
2. **Physics Control**: `POST /api/physics` - Toggle physics, set gravity
3. **Scene Manipulation**: `POST /api/scene` - Clear scene, load scenes
4. **RenderDoc Integration**: `POST /api/renderdoc/capture` - GPU frame analysis
5. **Input Simulation**: `POST /api/input/key` - Simulate keyboard/mouse input
6. **Performance Profiling**: `GET /api/performance` - Detailed frame timing data

### MCP Server Integration

**Next Step**: Build Python MCP (Model Context Protocol) server to bridge Claude Desktop with the game HTTP API.

**Benefits**:
- Claude can query game state natively during conversations
- Automatic frame capture when debugging visual issues
- Seamless camera control for inspecting different views
- Entity manipulation without manual curl commands

**Implementation**: See `docs/MCP_SERVER_PLAN.md` (to be created)

---

## Troubleshooting

### Port Already in Use

**Error**: `[ERROR] Failed to start debug server on port 3000`

**Solution**: Check if another process is using port 3000:
```bash
# Windows
netstat -ano | findstr :3000

# Linux/Mac
lsof -i :3000
```

### Commands Not Executing

**Symptom**: HTTP requests return success, but nothing happens in game

**Cause**: Command queue may be full or main thread blocked

**Solution**: Check console logs for command execution messages:
```
[INFO] [DebugServer] Executing: set_camera_position
[INFO] [DebugServer] Camera position set to: (10.0, 5.0, 15.0)
```

### Frame Capture Produces Black Image

**Symptom**: PNG file created but appears black/empty

**Cause**: Capturing before first frame rendered, or during window minimize

**Solution**: Ensure game window is visible and rendering before capture

### High CPU Usage from HTTP Server

**Symptom**: `httplib` thread consuming excessive CPU

**Cause**: Rapid polling in HTTP server thread

**Solution**: This is a known issue with `cpp-httplib`. Consider switching to `Crow` or `Pistache` if problematic.

---

## Related Documentation

- **CLAUDE.md**: Overall project architecture and phase documentation
- **PHASE_7.2_DEBUG_SERVER.md**: Implementation plan and technical decisions
- **TESTING_INFRASTRUCTURE.md**: Unit testing philosophy and coverage
- **Frame Capture Source**: `src/debug/frame_capture.h`, `src/debug/frame_capture.cpp`
- **Debug Server Source**: `src/debug/debug_server.h`, `src/debug/debug_server.cpp`

---

## References

### External Libraries

- **cpp-httplib**: HTTP server library ([GitHub](https://github.com/yhirose/cpp-httplib))
- **nlohmann/json**: JSON parsing library ([GitHub](https://github.com/nlohmann/json))
- **stb_image_write**: PNG encoding library ([GitHub](https://github.com/nothings/stb))

### Vulkan Resources

- **Vulkan Tutorial**: [vulkan-tutorial.com](https://vulkan-tutorial.com/)
- **Vulkan Spec**: [Khronos Vulkan Registry](https://registry.khronos.org/vulkan/)
- **Image Layout Transitions**: [GPUOpen - Vulkan Barriers](https://gpuopen.com/learn/vulkan-barriers-explained/)
