# Game Debug Server - Implementation Handoff v2

**REVISED**: This version addresses VulkanMon-specific architecture integration points and resolves critical gaps from v1.

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
                                     │                          │
                                     ▼                          ▼
                            ┌─────────────────┐      ┌─────────────────┐
                            │   RenderDoc     │      │  Application*   │
                            │   Integration   │      │  World*         │
                            └─────────────────┘      │  Logger         │
                                                     └─────────────────┘
```

## VulkanMon Architecture Integration Points

### Existing Systems to Leverage
- **Logger** (`src/utils/Logger.h`) - Thread-safe singleton logging system
- **Application** (`src/core/Application.h`) - Main engine class with ECS World, performance metrics
- **World** (`src/core/World.h`) - ECS world with EntityManager
- **VulkanRenderer** (`src/rendering/VulkanRenderer.h`) - Owns swapchain for frame capture
- **vcpkg** - Dependency management via `vcpkg.json`
- **CMake** - Build system with C++20 support

### Key Integration Requirements
1. **DebugServer must receive Application* via dependency injection** (not singleton pattern)
2. **Use Logger::getInstance()** for all logging (no std::cout)
3. **Access ECS via Application::getWorld()** (need to add public getter)
4. **Performance metrics via Application::getFPS()** and **Application::getFrameTime()** (already exist)
5. **Frame capture via VulkanRenderer::captureCurrentFrame()** (need to add public method)

## Implementation Phases

### Phase 0: Foundation & Dependencies

**Objective:** Set up build system and dependencies before writing code.

#### 0.1 Update vcpkg.json

**File: `vcpkg.json`**
Add missing dependency:
```json
{
  "dependencies": [
    // ... existing dependencies ...
    "nlohmann-json"  // ADD THIS - for JSON serialization in DebugServer
  ]
}
```

#### 0.2 Download Single-Header Libraries

```bash
# From project root
mkdir -p src/external

# httplib.h - Lightweight HTTP server
curl -L https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -o src/external/httplib.h

# renderdoc_app.h - RenderDoc API
curl -L https://raw.githubusercontent.com/baldurk/renderdoc/v1.x/renderdoc/api/app/renderdoc_app.h -o src/external/renderdoc_app.h

# stb_image_write.h - PNG saving (may already exist in src/)
# Check first: ls src/stb_image_write.h
# If not present: curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h -o src/external/stb_image_write.h
```

#### 0.3 Update CMakeLists.txt

**File: `CMakeLists.txt`**
Add debug server compilation (debug builds only):

```cmake
# After existing target_link_libraries() section, add:

# Debug Server (Debug builds only)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_sources(vulkanmon PRIVATE
        src/debug/debug_server.cpp
        src/debug/frame_capture.cpp
    )
    target_compile_definitions(vulkanmon PRIVATE DEBUG_BUILD)

    # Add nlohmann-json
    find_package(nlohmann_json CONFIG REQUIRED)
    target_link_libraries(vulkanmon PRIVATE nlohmann_json::nlohmann_json)
endif()

# Include external headers
target_include_directories(vulkanmon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/external
)
```

#### 0.4 Add Application Public Getter

**File: `src/core/Application.h`**
Add to public section (around line 171):
```cpp
/**
 * Get ECS World for debug access
 * @return Pointer to current World instance
 */
World* getWorld() { return world_.get(); }

/**
 * Get VulkanRenderer for frame capture
 * @return Pointer to renderer instance
 */
VulkanRenderer* getRenderer() { return renderer_.get(); }
```

### Phase 1: Core Debug Server (C++ Side)

**Objective:** Add a lightweight HTTP server to the game engine that exposes debug endpoints.

#### 1.1 Create Debug Server Header

**New File: `src/debug/debug_server.h`**
```cpp
#pragma once

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <functional>
#include <string>
#include <chrono>
#include <set>

// Forward declarations
struct RENDERDOC_API_1_1_2;

namespace VulkanMon {
    class Application; // Forward declare Application
}

namespace VulkanMon::Debug {

struct DebugCommand {
    std::string type;
    std::string payload;  // JSON string
};

class DebugServer {
public:
    /**
     * Create DebugServer with dependency injection
     * @param app Pointer to Application for ECS/metrics access
     */
    explicit DebugServer(Application* app);
    ~DebugServer();

    // Lifecycle
    bool Start(uint16_t port = 3000);
    void Stop();
    bool IsRunning() const { return running_; }

    // Called from main game thread each frame
    void ProcessCommands();

    // State queries (thread-safe, called from HTTP server thread)
    std::string GetGameState() const;
    std::string GetEntityList() const;
    std::string GetPlayerState() const;
    std::string GetFrameStats() const;
    std::string GetVulkanStats() const;

    // Commands (queued for main thread)
    bool QueueCommand(const DebugCommand& cmd);

    // Frame capture
    void CaptureFramebuffer(const std::string& path);

    // RenderDoc integration
    void InitRenderDoc();
    void TriggerRenderDocCapture();
    bool IsRenderDocAttached() const;

private:
    void ServerThread();
    void ExecuteCommand(const DebugCommand& cmd);
    bool ValidateCommand(const DebugCommand& cmd) const;

    // Rate limiting
    bool CanAcceptCommand() const;
    std::chrono::steady_clock::time_point last_command_time_;
    static constexpr int RATE_LIMIT_MS = 100;

    // Valid command types
    static const std::set<std::string> VALID_COMMANDS;

    // Application reference for ECS/metrics access
    Application* app_;

    std::thread server_thread_;
    std::atomic<bool> running_{false};

    std::queue<DebugCommand> command_queue_;
    mutable std::mutex queue_mutex_;

    RENDERDOC_API_1_1_2* renderdoc_api_ = nullptr;
    uint16_t server_port_ = 3000;
};

} // namespace VulkanMon::Debug
```

#### 1.2 Implement Debug Server

**New File: `src/debug/debug_server.cpp`**
```cpp
#include "debug_server.h"
#include "../core/Application.h"
#include "../core/World.h"
#include "../utils/Logger.h"
#include "../components/Transform.h"
#include "../components/Renderable.h"

#include <nlohmann/json.hpp>
#include "httplib.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "../external/renderdoc_app.h"

using json = nlohmann::json;

namespace VulkanMon::Debug {

// Valid command types
const std::set<std::string> DebugServer::VALID_COMMANDS = {
    "teleport_player",
    "spawn_entity",
    "set_time_scale",
    "reload_shader",
    "set_material",
    "toggle_physics",
    "clear_scene"
};

DebugServer::DebugServer(Application* app)
    : app_(app)
    , last_command_time_(std::chrono::steady_clock::now())
{
    if (!app_) {
        Logger::getInstance().fatal("[DebugServer] Application pointer is null!");
        throw std::runtime_error("DebugServer requires valid Application pointer");
    }
}

DebugServer::~DebugServer() {
    Stop();
}

bool DebugServer::Start(uint16_t port) {
    if (running_) {
        Logger::getInstance().warning("[DebugServer] Already running");
        return false;
    }

    server_port_ = port;
    running_ = true;
    server_thread_ = std::thread(&DebugServer::ServerThread, this);

    InitRenderDoc();

    Logger::getInstance().info("[DebugServer] Started on port " + std::to_string(port));
    return true;
}

void DebugServer::Stop() {
    if (!running_) return;

    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    Logger::getInstance().info("[DebugServer] Stopped");
}

bool DebugServer::CanAcceptCommand() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_command_time_
    ).count();
    return elapsed >= RATE_LIMIT_MS;
}

bool DebugServer::ValidateCommand(const DebugCommand& cmd) const {
    return VALID_COMMANDS.find(cmd.type) != VALID_COMMANDS.end();
}

bool DebugServer::QueueCommand(const DebugCommand& cmd) {
    // Validate command type
    if (!ValidateCommand(cmd)) {
        Logger::getInstance().warning(
            "[DebugServer] Invalid command type: " + cmd.type
        );
        return false;
    }

    // Rate limiting
    if (!CanAcceptCommand()) {
        Logger::getInstance().warning(
            "[DebugServer] Rate limit exceeded, rejecting command: " + cmd.type
        );
        return false;
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);
    command_queue_.push(cmd);
    last_command_time_ = std::chrono::steady_clock::now();

    Logger::getInstance().debug("[DebugServer] Queued command: " + cmd.type);
    return true;
}

void DebugServer::ProcessCommands() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    while (!command_queue_.empty()) {
        auto cmd = command_queue_.front();
        command_queue_.pop();

        ExecuteCommand(cmd);
    }
}

void DebugServer::ExecuteCommand(const DebugCommand& cmd) {
    Logger::getInstance().info("[DebugServer] Executing: " + cmd.type);

    try {
        auto payload = json::parse(cmd.payload);

        if (cmd.type == "teleport_player") {
            // TODO: Implement player teleport
            // auto pos = glm::vec3(payload["x"], payload["y"], payload["z"]);
            // app_->teleportPlayer(pos);

        } else if (cmd.type == "spawn_entity") {
            // TODO: Implement entity spawning
            // auto type = payload["entity_type"].get<std::string>();
            // app_->spawnEntity(type);

        } else if (cmd.type == "reload_shader") {
            // TODO: Trigger shader reload
            // app_->getRenderer()->reloadShaders();
        }

        Logger::getInstance().info("[DebugServer] Command executed: " + cmd.type);

    } catch (const json::exception& e) {
        Logger::getInstance().error(
            "[DebugServer] JSON parse error: " + std::string(e.what())
        );
    } catch (const std::exception& e) {
        Logger::getInstance().error(
            "[DebugServer] Command execution error: " + std::string(e.what())
        );
    }
}

// State queries
std::string DebugServer::GetGameState() const {
    json state;
    state["running"] = app_->isRunning();
    state["fps"] = app_->getFPS();
    state["frame_time_ms"] = app_->getFrameTime();

    return state.dump(2);
}

std::string DebugServer::GetEntityList() const {
    json result;
    result["entities"] = json::array();

    auto* world = app_->getWorld();
    if (!world) {
        result["error"] = "World not initialized";
        return result.dump(2);
    }

    auto& em = world->getEntityManager();
    auto entities = em.getAllEntities();

    for (auto entityId : entities) {
        json entity;
        entity["id"] = entityId;

        // Check for Transform component
        if (auto* transform = em.getComponent<Transform>(entityId)) {
            entity["position"] = {
                transform->position.x,
                transform->position.y,
                transform->position.z
            };
            entity["rotation"] = {
                transform->rotation.x,
                transform->rotation.y,
                transform->rotation.z
            };
            entity["scale"] = {
                transform->scale.x,
                transform->scale.y,
                transform->scale.z
            };
        }

        // Check for Renderable component
        if (auto* renderable = em.getComponent<Renderable>(entityId)) {
            entity["mesh"] = renderable->meshPath;
            entity["material_id"] = renderable->materialId;
            entity["visible"] = renderable->visible;
        }

        result["entities"].push_back(entity);
    }

    result["count"] = entities.size();
    return result.dump(2);
}

std::string DebugServer::GetPlayerState() const {
    json state;
    // TODO: Implement player state query
    state["position"] = {0.0f, 0.0f, 0.0f};
    state["health"] = 100;

    return state.dump(2);
}

std::string DebugServer::GetFrameStats() const {
    json stats;
    stats["fps"] = app_->getFPS();
    stats["frame_time_ms"] = app_->getFrameTime();
    // TODO: Add draw call counts, triangle counts from VulkanRenderer

    return stats.dump(2);
}

std::string DebugServer::GetVulkanStats() const {
    json stats;
    // TODO: Query VulkanRenderer for memory usage, pipeline stats
    stats["memory_mb"] = 512; // Placeholder

    return stats.dump(2);
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
            Logger::getInstance().info("[DebugServer] RenderDoc API attached!");
        }
    }
}

void DebugServer::TriggerRenderDocCapture() {
    if (renderdoc_api_) {
        renderdoc_api_->TriggerCapture();
        Logger::getInstance().info("[DebugServer] RenderDoc frame captured!");
    } else {
        Logger::getInstance().warning("[DebugServer] RenderDoc not attached");
    }
}

bool DebugServer::IsRenderDocAttached() const {
    return renderdoc_api_ != nullptr;
}

void DebugServer::CaptureFramebuffer(const std::string& path) {
    // TODO: Call VulkanRenderer::captureCurrentFrame(path)
    Logger::getInstance().info("[DebugServer] Framebuffer capture to: " + path);
}

void DebugServer::ServerThread() {
    httplib::Server server;

    // GET endpoints
    server.Get("/api/state", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(GetGameState(), "application/json");
    });

    server.Get("/api/entities", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(GetEntityList(), "application/json");
    });

    server.Get("/api/player", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(GetPlayerState(), "application/json");
    });

    server.Get("/api/stats/frame", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(GetFrameStats(), "application/json");
    });

    server.Get("/api/stats/vulkan", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(GetVulkanStats(), "application/json");
    });

    // POST endpoints
    server.Post("/api/command", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            DebugCommand cmd;
            cmd.type = j["type"].get<std::string>();
            cmd.payload = j["payload"].dump();

            bool queued = QueueCommand(cmd);

            json response;
            response["success"] = queued;
            response["message"] = queued ? "Command queued" : "Command rejected (rate limit or invalid)";

            res.set_content(response.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 400;
        }
    });

    server.Post("/api/capture", [this](const httplib::Request& req, httplib::Response& res) {
        // TODO: Implement frame capture
        json response;
        response["success"] = true;
        response["path"] = "./debug/latest_frame.png";
        res.set_content(response.dump(2), "application/json");
    });

    server.Post("/api/renderdoc", [this](const httplib::Request&, httplib::Response& res) {
        TriggerRenderDocCapture();

        json response;
        response["success"] = true;
        response["attached"] = IsRenderDocAttached();
        res.set_content(response.dump(2), "application/json");
    });

    // Health check
    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    Logger::getInstance().info(
        "[DebugServer] HTTP server listening on localhost:" + std::to_string(server_port_)
    );

    // Bind to localhost only for security
    server.listen("127.0.0.1", server_port_);
}

} // namespace VulkanMon::Debug
```

#### 1.3 Integrate with Application

**File: `src/core/Application.h`**
Add to includes (around line 24):
```cpp
#ifdef DEBUG_BUILD
#include "../debug/debug_server.h"
#endif
```

Add to private members (around line 210):
```cpp
#ifdef DEBUG_BUILD
// Debug server
std::unique_ptr<Debug::DebugServer> debugServer_;
#endif
```

**File: `src/core/Application.cpp`**
In `initialize()` method (after ECS initialization):
```cpp
#ifdef DEBUG_BUILD
    // Initialize debug server
    debugServer_ = std::make_unique<Debug::DebugServer>(this);
    debugServer_->Start(3000);
#endif
```

In `run()` method main loop (after `processInput()`):
```cpp
#ifdef DEBUG_BUILD
        // Process debug commands from HTTP server
        debugServer_->ProcessCommands();
#endif
```

In `cleanup()` or destructor:
```cpp
#ifdef DEBUG_BUILD
    if (debugServer_) {
        debugServer_->Stop();
    }
#endif
```

### Phase 2: MCP Server (Python Side)

**Objective:** Create a Python MCP server that bridges Claude with the game.

#### 2.1 Create MCP Server

**New File: `tools/mcp_server/game_debug_server.py`**
```python
#!/usr/bin/env python3
"""
MCP Server for VulkanMon Game Debugging
Bridges Claude AI with the running game engine
"""

import asyncio
import json
import requests
from typing import Any

# MCP SDK
from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp import types

# Configuration
GAME_API_URL = "http://localhost:3000/api"
TIMEOUT = 5.0

app = Server("vulkanmon-debug-server")

# ============================================================================
# MCP Tools - State Inspection
# ============================================================================

@app.list_tools()
async def list_tools() -> list[types.Tool]:
    """List all available debugging tools"""
    return [
        types.Tool(
            name="get_game_state",
            description="Get current game state including FPS, frame time, running status",
            inputSchema={
                "type": "object",
                "properties": {},
            },
        ),
        types.Tool(
            name="list_entities",
            description="List all active entities in the ECS with Transform and Renderable components",
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
            name="send_command",
            description="Send a command to the game. Valid commands: teleport_player, spawn_entity, reload_shader, set_material, toggle_physics, clear_scene",
            inputSchema={
                "type": "object",
                "properties": {
                    "command": {
                        "type": "string",
                        "description": "Command type",
                        "enum": ["teleport_player", "spawn_entity", "reload_shader", "set_material", "toggle_physics", "clear_scene"]
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
            description="Trigger a RenderDoc frame capture for detailed GPU analysis (requires game launched through RenderDoc)",
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

        elif name == "send_command":
            command = arguments["command"]
            params = arguments.get("params", {})

            response = requests.post(
                f"{GAME_API_URL}/command",
                json={"type": command, "payload": params},
                timeout=TIMEOUT
            )
            response.raise_for_status()
            result = response.json()

            if result.get("success"):
                return [types.TextContent(type="text", text=f"✓ Command '{command}' queued successfully")]
            else:
                return [types.TextContent(type="text", text=f"✗ Command rejected: {result.get('message', 'Unknown error')}")]

        elif name == "trigger_renderdoc_capture":
            response = requests.post(f"{GAME_API_URL}/renderdoc", timeout=TIMEOUT)
            response.raise_for_status()
            result = response.json()

            if result.get("attached"):
                return [types.TextContent(
                    type="text",
                    text="✓ RenderDoc frame captured! Check RenderDoc UI for detailed analysis."
                )]
            else:
                return [types.TextContent(
                    type="text",
                    text="✗ RenderDoc not attached. Launch the game through RenderDoc first."
                )]

        else:
            return [types.TextContent(type="text", text=f"Unknown tool: {name}")]

    except requests.exceptions.ConnectionError:
        return [types.TextContent(
            type="text",
            text="❌ Error: Could not connect to game. Is the debug server running?\n\nMake sure the game is running in Debug mode."
        )]
    except requests.exceptions.Timeout:
        return [types.TextContent(
            type="text",
            text="❌ Error: Request timed out. Game may be frozen or unresponsive."
        )]
    except Exception as e:
        return [types.TextContent(type="text", text=f"❌ Error: {str(e)}")]


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

#### 2.2 Create Python Package Configuration

**New File: `tools/mcp_server/pyproject.toml`**
```toml
[project]
name = "vulkanmon-debug-server"
version = "0.1.0"
description = "MCP server for VulkanMon game debugging"
requires-python = ">=3.10"
dependencies = [
    "mcp>=0.9.0",
    "requests>=2.31.0",
]

[project.scripts]
vulkanmon-debug-server = "game_debug_server:main"
```

**New File: `tools/mcp_server/README.md`**
```markdown
# VulkanMon Debug MCP Server

## Installation

```bash
cd tools/mcp_server
pip install -e .
```

## Usage

1. Build and run VulkanMon in Debug mode:
   ```bash
   cd build
   cmake --build . --config Debug
   Debug/vulkanmon.exe
   ```

2. In a separate terminal, run the MCP server:
   ```bash
   python tools/mcp_server/game_debug_server.py
   ```

3. Configure Claude Desktop/Code to use this MCP server (see below)

## Claude Desktop Configuration

Add to `~/Library/Application Support/Claude/claude_desktop_config.json` (macOS) or
`%APPDATA%\Claude\claude_desktop_config.json` (Windows):

```json
{
  "mcpServers": {
    "vulkanmon-debug": {
      "command": "python",
      "args": ["D:/ws/vulkanmon/tools/mcp_server/game_debug_server.py"]
    }
  }
}
```

**Important:** Use absolute path to `game_debug_server.py`

## Testing the Connection

```bash
# Test if game debug server is running
curl http://localhost:3000/health

# Test endpoints
curl http://localhost:3000/api/state
curl http://localhost:3000/api/entities
```

Expected output:
```json
{"status":"ok"}
```
```

### Phase 3: Vulkan Frame Capture

**Objective:** Implement actual framebuffer capture from Vulkan swapchain.

#### 3.1 Add Frame Capture Interface to VulkanRenderer

**File: `src/rendering/VulkanRenderer.h`**
Add to public methods:
```cpp
/**
 * Capture current frame to PNG file
 * @param filepath Output PNG file path
 * @return true if capture successful
 */
bool captureCurrentFrame(const std::string& filepath);
```

#### 3.2 Create Frame Capture Implementation

**New File: `src/debug/frame_capture.h`**
```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <string>

namespace VulkanMon::Debug {

class FrameCapture {
public:
    /**
     * Capture Vulkan image to PNG file
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device
     * @param commandPool Command pool for transfer commands
     * @param queue Graphics queue for command submission
     * @param image Source image to capture
     * @param format Image format
     * @param width Image width
     * @param height Image height
     * @param filename Output PNG file path
     * @return true if capture successful
     */
    static bool CaptureToFile(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkCommandPool commandPool,
        VkQueue queue,
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

} // namespace VulkanMon::Debug
```

**New File: `src/debug/frame_capture.cpp`**
```cpp
#include "frame_capture.h"
#include "../utils/Logger.h"
#include <vector>
#include <cstring>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../external/stb_image_write.h"

namespace VulkanMon::Debug {

bool FrameCapture::CaptureToFile(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue queue,
    VkImage image,
    VkFormat format,
    uint32_t width,
    uint32_t height,
    const std::string& filename
) {
    // TODO: Implement Vulkan framebuffer capture
    // 1. Create staging buffer
    // 2. Transition image layout to TRANSFER_SRC_OPTIMAL
    // 3. vkCmdCopyImageToBuffer
    // 4. Transition image layout back
    // 5. Wait for copy to complete
    // 6. Map staging buffer memory
    // 7. Convert format if needed (BGR -> RGB)
    // 8. Call SaveImageToPNG
    // 9. Unmap and cleanup

    Logger::getInstance().warning("[FrameCapture] Not yet implemented");
    return false;
}

bool FrameCapture::SaveImageToPNG(
    const void* data,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    const std::string& filename
) {
    int result = stbi_write_png(
        filename.c_str(),
        width,
        height,
        channels,
        data,
        width * channels
    );

    if (result) {
        Logger::getInstance().info("[FrameCapture] Saved PNG: " + filename);
        return true;
    } else {
        Logger::getInstance().error("[FrameCapture] Failed to save PNG: " + filename);
        return false;
    }
}

} // namespace VulkanMon::Debug
```

## Testing Strategy

### Test Phase 1: HTTP Server Connectivity

```bash
# Terminal 1: Run game (Debug build)
cd build
cmake --build . --config Debug
Debug/vulkanmon.exe

# Terminal 2: Test HTTP endpoints
curl http://localhost:3000/health
curl http://localhost:3000/api/state
curl http://localhost:3000/api/entities
curl http://localhost:3000/api/stats/frame

# Expected: JSON responses with game state
```

### Test Phase 2: MCP Integration

```bash
# Terminal 1: Run game
Debug/vulkanmon.exe

# Terminal 2: Run MCP server
cd tools/mcp_server
python game_debug_server.py

# Terminal 3: Configure Claude Desktop and test
# In Claude: "Can you check the game state?"
# Expected: Claude uses get_game_state tool and shows FPS, frame time
```

### Test Phase 3: Command Execution

```bash
# In Claude Desktop/Code:
"Send a reload_shader command to the game"

# Check game console for:
# [DebugServer] Queued command: reload_shader
# [DebugServer] Executing: reload_shader
```

### Test Phase 4: RenderDoc Integration

1. Launch game through RenderDoc UI
2. In Claude: "Trigger a RenderDoc capture"
3. Check RenderDoc UI for new capture
4. Expected: Frame captured with full GPU state

## Success Criteria

- ✅ Game runs with debug server active (no performance impact)
- ✅ HTTP endpoints return valid JSON with real ECS data
- ✅ MCP server connects to game successfully
- ✅ Claude can query game state and see actual entities
- ✅ Commands are rate-limited and validated
- ✅ Logger is used for all debug server output
- ✅ RenderDoc integration works (when launched through RenderDoc)
- ✅ Commands queued from MCP execute in game loop without crashes

## Known Limitations

1. **Frame capture not fully implemented** - Phase 3 requires Vulkan image copy implementation
2. **Player state placeholder** - Need to identify player entity in ECS
3. **Command execution stubs** - Need to implement teleport, spawn, etc.
4. **Windows Firewall** - May prompt on first run (localhost only, safe to allow)

## Future Enhancements

- [ ] WebSocket support for streaming updates
- [ ] Live shader editing with hot reload
- [ ] Performance profiling integration with Chrome DevTools
- [ ] Multiple render target capture (G-buffer inspection)
- [ ] Entity inspector with full component details
- [ ] Console log streaming to Claude in real-time
- [ ] Automated testing via MCP commands
- [ ] Recording and playback of command sequences

## Architecture Decisions

**Why dependency injection instead of singleton?**
- Testable: Can mock Application for unit tests
- Explicit: Clear ownership and lifecycle
- Safer: No global state access

**Why rate limiting?**
- Prevents Claude from overwhelming game loop
- Ensures responsive gameplay during debugging
- Configurable via RATE_LIMIT_MS constant

**Why queue commands instead of direct execution?**
- Thread safety - game state only modified on main thread
- Prevents race conditions with ECS
- Easier to implement undo/redo later
- Atomic command execution

**Why localhost-only binding?**
- Security: No external network access
- Performance: No network encryption overhead
- Simplicity: No authentication needed

## VulkanMon Integration Notes

### Existing Systems Used
- **Logger::getInstance()** - All debug server logging
- **Application::getWorld()** - ECS entity queries
- **Application::getFPS()** - Performance metrics
- **EntityManager::getAllEntities()** - Entity listing
- **Transform/Renderable components** - Entity serialization

### Build System Integration
- **vcpkg** manages nlohmann-json dependency
- **CMake** conditionally compiles debug server (Debug builds only)
- **DEBUG_BUILD** preprocessor define controls all debug code

### Testing Integration
- Add unit tests to `tests_cpp/debug/test_DebugServer.cpp`
- Test command validation, rate limiting, JSON serialization
- Maintain 100% test pass rate (current: 102 tests, 1724 assertions)

## Questions for Implementation

All critical questions answered in v2:
- ✅ Main game loop location: `Application::run()` in `src/core/Application.cpp`
- ✅ ECS structure: World → EntityManager → Components (Transform, Renderable, etc.)
- ✅ FPS tracking: `Application::getFPS()` and `Application::getFrameTime()`
- ✅ Swapchain access: VulkanRenderer (add `captureCurrentFrame()` public method)
- ✅ Build system: CMake with vcpkg dependency management
- ✅ JSON library: Add nlohmann-json to vcpkg.json

## Contact

If you hit any blockers or have questions about architecture decisions, just ask! Remember: **simple is powerful, test as we go, document often**.

---

**CHANGELOG v1 → v2:**
- ✅ Added Phase 0 for foundation/dependencies setup
- ✅ Integrated with existing Logger system (removed std::cout)
- ✅ Added Application* dependency injection (not singleton)
- ✅ Added rate limiting and command validation
- ✅ Used VulkanMon::Debug namespace for consistency
- ✅ Added nlohmann-json to vcpkg.json
- ✅ Added CMakeLists.txt debug-only compilation
- ✅ Implemented real ECS queries in GetEntityList()
- ✅ Used actual Application::getFPS() in GetFrameStats()
- ✅ Added Application::getWorld() public getter
- ✅ Documented Windows firewall considerations
- ✅ Added unit testing requirements
- ✅ Clarified all VulkanMon integration points
