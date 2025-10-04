# Game Debug Server - Implementation Handoff

## Project Context

We're building a **Pokemon Legends: Arceus**-style game with a custom Vulkan engine. Currently, debugging is disjointed because Claude (me) cannot inspect the running game when making changes. We need a debug bridge.

**Design Philosophy:**
- Simple is powerful
- Test as we go
- Document often

## Goal

Create an **MCP (Model Context Protocol) server** that allows Claude to:
1. Inspect live game state (entities, player, scene graph)
2. Query performance metrics (FPS, Vulkan stats, ECS timings)
3. Send live commands (spawn entities, teleport player, reload assets)
4. Capture and analyze rendered frames
5. Trigger RenderDoc captures for GPU debugging

## Architecture Overview

```
┌─────────────────┐         ┌──────────────────┐         ┌─────────────┐
│  Claude (MCP)   │ ◄─────► │   MCP Server     │ ◄─────► │  Game (C++) │
│                 │  Tools  │   (Python)       │  HTTP   │  Vulkan     │
└─────────────────┘         └──────────────────┘         └─────────────┘
                                     │
                                     ▼
                            ┌─────────────────┐
                            │   RenderDoc     │
                            │   Integration   │
                            └─────────────────┘
```

## Implementation Phases

### Phase 1: Core Debug Server (C++ Side)

**Objective:** Add a lightweight HTTP server to the game engine that exposes debug endpoints.

#### Files to Create/Modify

**New File: `src/debug/debug_server.h`**
```cpp
#pragma once
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <functional>
#include <string>

// Forward declarations
struct RENDERDOC_API_1_1_2;

namespace Debug {

struct DebugCommand {
    std::string type;
    std::string payload;  // JSON string
};

class DebugServer {
public:
    DebugServer();
    ~DebugServer();
    
    // Lifecycle
    bool Start(uint16_t port = 3000);
    void Stop();
    bool IsRunning() const { return running_; }
    
    // Called from main game thread each frame
    void ProcessCommands();
    
    // State queries (thread-safe, called from server thread)
    std::string GetGameState() const;
    std::string GetEntityList() const;
    std::string GetPlayerState() const;
    std::string GetFrameStats() const;
    std::string GetVulkanStats() const;
    
    // Commands (queued for main thread)
    void QueueCommand(const DebugCommand& cmd);
    
    // Frame capture
    void CaptureFramebuffer(const std::string& path);
    void SetDebugOverlays(uint32_t flags);
    
    // RenderDoc integration
    void InitRenderDoc();
    void TriggerRenderDocCapture();
    bool IsRenderDocAttached() const;
    
private:
    void ServerThread();
    void HandleRequest(/* HTTP request params */);
    
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    
    std::queue<DebugCommand> command_queue_;
    mutable std::mutex queue_mutex_;
    
    RENDERDOC_API_1_1_2* renderdoc_api_ = nullptr;
    uint32_t debug_overlay_flags_ = 0;
};

} // namespace Debug
```

**New File: `src/debug/debug_server.cpp`**
```cpp
#include "debug_server.h"
#include <iostream>
// TODO: Add lightweight HTTP library (httplib.h recommended)
// #include "httplib.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Download this single header from RenderDoc repo
#include "renderdoc_app.h"

namespace Debug {

DebugServer::DebugServer() = default;

DebugServer::~DebugServer() {
    Stop();
}

bool DebugServer::Start(uint16_t port) {
    if (running_) return false;
    
    running_ = true;
    server_thread_ = std::thread(&DebugServer::ServerThread, this);
    
    InitRenderDoc();
    
    std::cout << "[DEBUG] Debug server started on port " << port << std::endl;
    return true;
}

void DebugServer::Stop() {
    if (!running_) return;
    
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    std::cout << "[DEBUG] Debug server stopped" << std::endl;
}

void DebugServer::ProcessCommands() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    while (!command_queue_.empty()) {
        auto cmd = command_queue_.front();
        command_queue_.pop();
        
        // TODO: Execute commands based on type
        // - "teleport_player"
        // - "spawn_entity"
        // - "set_time_scale"
        // - "reload_shader"
        // etc.
        
        std::cout << "[DEBUG] Executing: " << cmd.type << std::endl;
    }
}

void DebugServer::QueueCommand(const DebugCommand& cmd) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    command_queue_.push(cmd);
}

// State queries - implement based on your game's ECS/architecture
std::string DebugServer::GetGameState() const {
    // TODO: Serialize current game state to JSON
    return R"({"state":"running","frame":12345})";
}

std::string DebugServer::GetEntityList() const {
    // TODO: Query ECS for all entities
    return R"({"entities":[]})";
}

std::string DebugServer::GetPlayerState() const {
    // TODO: Get player position, stats, inventory
    return R"({"position":[0,0,0],"health":100})";
}

std::string DebugServer::GetFrameStats() const {
    // TODO: Get FPS, frame time, draw calls
    return R"({"fps":60,"frame_time_ms":16.6})";
}

std::string DebugServer::GetVulkanStats() const {
    // TODO: Get Vulkan memory usage, pipeline stats
    return R"({"memory_mb":512})";
}

// RenderDoc Integration
void DebugServer::InitRenderDoc() {
    #ifdef _WIN32
        if(HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    #else
        if(void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOCLOSE))
    #endif
    {
        #ifdef _WIN32
            pRENDERDOC_GetAPI RENDERDOC_GetAPI = 
                (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        #else
            pRENDERDOC_GetAPI RENDERDOC_GetAPI = 
                (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        #endif
        
        if (RENDERDOC_GetAPI) {
            RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, 
                           (void**)&renderdoc_api_);
        }
        
        if (renderdoc_api_) {
            std::cout << "[DEBUG] RenderDoc API attached!" << std::endl;
        }
    }
}

void DebugServer::TriggerRenderDocCapture() {
    if (renderdoc_api_) {
        renderdoc_api_->TriggerCapture();
        std::cout << "[DEBUG] RenderDoc frame captured!" << std::endl;
    } else {
        std::cout << "[DEBUG] RenderDoc not attached" << std::endl;
    }
}

bool DebugServer::IsRenderDocAttached() const {
    return renderdoc_api_ != nullptr;
}

void DebugServer::CaptureFramebuffer(const std::string& path) {
    // TODO: Implement Vulkan framebuffer capture
    // 1. vkCmdCopyImageToBuffer
    // 2. Map buffer memory
    // 3. Save to PNG with stb_image_write
    std::cout << "[DEBUG] Framebuffer capture to: " << path << std::endl;
}

void DebugServer::ServerThread() {
    // TODO: Implement HTTP server using httplib.h
    // 
    // Endpoints:
    // GET  /api/state          -> GetGameState()
    // GET  /api/entities       -> GetEntityList()
    // GET  /api/player         -> GetPlayerState()
    // GET  /api/stats/frame    -> GetFrameStats()
    // GET  /api/stats/vulkan   -> GetVulkanStats()
    // POST /api/command        -> QueueCommand()
    // POST /api/capture        -> CaptureFramebuffer()
    // POST /api/renderdoc      -> TriggerRenderDocCapture()
    
    while (running_) {
        // Server loop
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace Debug
```

**Modify: Your main game loop**
```cpp
// In your Game or Engine class
#include "debug/debug_server.h"

class Game {
    Debug::DebugServer debug_server_;
    
public:
    void Init() {
        // ... existing init code ...
        
        #ifdef DEBUG_BUILD
        debug_server_.Start(3000);
        #endif
    }
    
    void Update() {
        // ... existing update code ...
        
        #ifdef DEBUG_BUILD
        debug_server_.ProcessCommands();
        #endif
    }
    
    void Shutdown() {
        #ifdef DEBUG_BUILD
        debug_server_.Stop();
        #endif
        
        // ... existing shutdown code ...
    }
};
```

### Phase 2: MCP Server (Python Side)

**Objective:** Create a Python MCP server that bridges Claude with the game.

#### Files to Create

**New File: `tools/mcp_server/game_debug_server.py`**
```python
#!/usr/bin/env python3
"""
MCP Server for Game Debugging
Bridges Claude AI with the running game engine
"""

import asyncio
import json
import requests
import base64
from pathlib import Path
from typing import Optional, Dict, Any

# MCP SDK
from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp import types

# Configuration
GAME_API_URL = "http://localhost:3000/api"
TIMEOUT = 5.0

app = Server("game-debug-server")

# ============================================================================
# MCP Tools - State Inspection
# ============================================================================

@app.list_tools()
async def list_tools() -> list[types.Tool]:
    """List all available debugging tools"""
    return [
        types.Tool(
            name="get_game_state",
            description="Get current game state including frame count, running status, etc.",
            inputSchema={
                "type": "object",
                "properties": {},
            },
        ),
        types.Tool(
            name="list_entities",
            description="List all active entities in the scene",
            inputSchema={
                "type": "object",
                "properties": {},
            },
        ),
        types.Tool(
            name="get_player_state",
            description="Get player position, health, inventory, and stats",
            inputSchema={
                "type": "object",
                "properties": {},
            },
        ),
        types.Tool(
            name="get_frame_stats",
            description="Get performance metrics: FPS, frame time, draw calls",
            inputSchema={
                "type": "object",
                "properties": {},
            },
        ),
        types.Tool(
            name="get_vulkan_stats",
            description="Get Vulkan-specific stats: memory usage, pipeline switches",
            inputSchema={
                "type": "object",
                "properties": {},
            },
        ),
        types.Tool(
            name="capture_frame",
            description="Capture current frame as screenshot",
            inputSchema={
                "type": "object",
                "properties": {
                    "overlays": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "Debug overlays to enable: wireframe, normals, bounds, fps",
                    },
                },
            },
        ),
        types.Tool(
            name="send_command",
            description="Send a command to the game (teleport, spawn, etc.)",
            inputSchema={
                "type": "object",
                "properties": {
                    "command": {
                        "type": "string",
                        "description": "Command type",
                    },
                    "params": {
                        "type": "object",
                        "description": "Command parameters as JSON object",
                    },
                },
                "required": ["command"],
            },
        ),
        types.Tool(
            name="trigger_renderdoc_capture",
            description="Trigger a RenderDoc frame capture for detailed GPU analysis",
            inputSchema={
                "type": "object",
                "properties": {},
            },
        ),
    ]


@app.call_tool()
async def call_tool(name: str, arguments: Any) -> list[types.TextContent]:
    """Handle tool calls from Claude"""
    
    try:
        if name == "get_game_state":
            response = requests.get(f"{GAME_API_URL}/state", timeout=TIMEOUT)
            response.raise_for_status()
            return [types.TextContent(type="text", text=json.dumps(response.json(), indent=2))]
        
        elif name == "list_entities":
            response = requests.get(f"{GAME_API_URL}/entities", timeout=TIMEOUT)
            response.raise_for_status()
            return [types.TextContent(type="text", text=json.dumps(response.json(), indent=2))]
        
        elif name == "get_player_state":
            response = requests.get(f"{GAME_API_URL}/player", timeout=TIMEOUT)
            response.raise_for_status()
            return [types.TextContent(type="text", text=json.dumps(response.json(), indent=2))]
        
        elif name == "get_frame_stats":
            response = requests.get(f"{GAME_API_URL}/stats/frame", timeout=TIMEOUT)
            response.raise_for_status()
            return [types.TextContent(type="text", text=json.dumps(response.json(), indent=2))]
        
        elif name == "get_vulkan_stats":
            response = requests.get(f"{GAME_API_URL}/stats/vulkan", timeout=TIMEOUT)
            response.raise_for_status()
            return [types.TextContent(type="text", text=json.dumps(response.json(), indent=2))]
        
        elif name == "capture_frame":
            overlays = arguments.get("overlays", [])
            response = requests.post(
                f"{GAME_API_URL}/capture",
                json={"overlays": overlays},
                timeout=TIMEOUT
            )
            response.raise_for_status()
            
            # The game saves to ./debug/latest_frame.png
            # We could return the base64 image here if needed
            return [types.TextContent(
                type="text",
                text=f"Frame captured with overlays: {overlays}\nSaved to: ./debug/latest_frame.png"
            )]
        
        elif name == "send_command":
            command = arguments["command"]
            params = arguments.get("params", {})
            
            response = requests.post(
                f"{GAME_API_URL}/command",
                json={"type": command, "payload": json.dumps(params)},
                timeout=TIMEOUT
            )
            response.raise_for_status()
            return [types.TextContent(type="text", text=f"Command '{command}' sent successfully")]
        
        elif name == "trigger_renderdoc_capture":
            response = requests.post(f"{GAME_API_URL}/renderdoc", timeout=TIMEOUT)
            response.raise_for_status()
            result = response.json()
            
            if result.get("attached"):
                return [types.TextContent(
                    type="text",
                    text="RenderDoc frame captured! Check RenderDoc UI for detailed analysis."
                )]
            else:
                return [types.TextContent(
                    type="text",
                    text="RenderDoc not attached. Launch the game through RenderDoc first."
                )]
        
        else:
            return [types.TextContent(type="text", text=f"Unknown tool: {name}")]
    
    except requests.exceptions.ConnectionError:
        return [types.TextContent(
            type="text",
            text="Error: Could not connect to game. Is the debug server running?"
        )]
    except requests.exceptions.Timeout:
        return [types.TextContent(
            type="text",
            text="Error: Request timed out. Game may be frozen."
        )]
    except Exception as e:
        return [types.TextContent(type="text", text=f"Error: {str(e)}")]


async def main():
    async with stdio_server() as (read_stream, write_stream):
        await app.run(
            read_stream,
            write_stream,
            app.create_initialization_options()
        )


if __name__ == "__main__":
    asyncio.run(main())
```

**New File: `tools/mcp_server/pyproject.toml`**
```toml
[project]
name = "game-debug-server"
version = "0.1.0"
description = "MCP server for game debugging"
dependencies = [
    "mcp>=0.9.0",
    "requests>=2.31.0",
]

[project.scripts]
game-debug-server = "game_debug_server:main"
```

**New File: `tools/mcp_server/README.md`**
```markdown
# Game Debug MCP Server

## Installation

```bash
cd tools/mcp_server
pip install -e .
```

## Usage

1. Start your game (debug build)
2. In a separate terminal: `python game_debug_server.py`
3. Configure Claude Desktop/Code to use this MCP server

## Claude Desktop Configuration

Add to `~/Library/Application Support/Claude/claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "game-debug": {
      "command": "python",
      "args": ["/path/to/tools/mcp_server/game_debug_server.py"]
    }
  }
}
```
```

### Phase 3: Vulkan Frame Capture

**Objective:** Implement actual framebuffer capture from Vulkan.

**New File: `src/debug/frame_capture.h`**
```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <string>

namespace Debug {

class FrameCapture {
public:
    static bool CaptureToFile(
        VkDevice device,
        VkImage image,
        VkFormat format,
        uint32_t width,
        uint32_t height,
        const std::string& filename
    );
    
private:
    static bool SaveImageToPNG(
        const void* data,
        uint32_t width,
        uint32_t height,
        uint32_t channels,
        const std::string& filename
    );
};

} // namespace Debug
```

**Implementation notes for `frame_capture.cpp`:**
```cpp
// 1. Create staging buffer
// 2. vkCmdCopyImageToBuffer
// 3. Wait for copy to complete
// 4. Map staging buffer memory
// 5. Convert format if needed (BGR -> RGB, etc.)
// 6. Use stb_image_write to save PNG
// 7. Unmap and cleanup
```

## Dependencies to Add

### C++ Side
1. **httplib** - Single header HTTP server
   - Download: https://github.com/yhirose/cpp-httplib
   - Copy `httplib.h` to `src/external/`

2. **renderdoc_app.h** - RenderDoc API
   - Download: https://github.com/baldurk/renderdoc/blob/v1.x/renderdoc/api/app/renderdoc_app.h
   - Copy to `src/external/`

3. **stb_image_write.h** - PNG saving
   - Download: https://github.com/nothings/stb
   - Copy `stb_image_write.h` to `src/external/`

4. **nlohmann/json** - JSON serialization (if not already present)
   - Download: https://github.com/nlohmann/json
   - Or use your existing JSON library

### Python Side
```bash
pip install mcp requests
```

## Testing Strategy

### Test Phase 1: Basic Connection
```bash
# Terminal 1: Run game
./build/game

# Terminal 2: Test endpoints
curl http://localhost:3000/api/state
curl http://localhost:3000/api/player
```

### Test Phase 2: MCP Integration
```bash
# Run MCP server
python tools/mcp_server/game_debug_server.py

# In Claude: Try commands
"Can you check the game state?"
"List all entities"
"Capture the current frame"
```

### Test Phase 3: RenderDoc
1. Launch game through RenderDoc
2. In Claude: "Trigger a RenderDoc capture"
3. Check RenderDoc UI for captured frame

## Success Criteria

- [ ] Game runs with debug server active (no performance impact)
- [ ] HTTP endpoints return valid JSON
- [ ] MCP server connects to game successfully
- [ ] Claude can query game state
- [ ] Frame capture produces valid PNG files
- [ ] RenderDoc integration works (when launched through RenderDoc)
- [ ] Commands queued from MCP execute in game loop

## Known Issues & TODOs

1. **Thread Safety**: Ensure all state queries are thread-safe
2. **Performance**: Debug server adds ~1ms overhead - acceptable for debug builds
3. **Security**: Server only listens on localhost (no external access)
4. **Error Handling**: Add comprehensive error messages for all failure modes

## Future Enhancements

- [ ] WebSocket support for streaming updates
- [ ] Live shader editing
- [ ] Performance profiling integration
- [ ] Multiple render target capture
- [ ] Entity inspector with component details
- [ ] Console log streaming to Claude

## Architecture Decisions

**Why HTTP instead of WebSocket?**
- Simpler implementation
- Request/response pattern fits our use case
- Can upgrade to WebSocket later if needed

**Why separate MCP server?**
- Keeps game engine language-agnostic
- Easy to iterate on Python side
- Can run MCP server remotely if needed

**Why queue commands instead of direct execution?**
- Thread safety - game state only modified on main thread
- Prevents race conditions
- Easier to implement undo/redo later

## Questions for Implementation

1. **Where is your main game loop?** (Need to add `debug_server_.ProcessCommands()`)
2. **How is your ECS structured?** (Need to query entities)
3. **Where do you track FPS?** (Need to expose metrics)
4. **How do you access the swapchain image?** (For frame capture)
5. **What's your build system?** (CMake, Makefile, etc.)

## Contact

If you hit any blockers or have questions about architecture decisions, just ask! Remember: **simple is powerful, test as we go, document often**.
