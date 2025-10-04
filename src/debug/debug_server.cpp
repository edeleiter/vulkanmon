#include "debug_server.h"
#include "../core/Application.h"
#include "../core/World.h"
#include "../utils/Logger.h"
#include "../components/Transform.h"
#include "../components/Renderable.h"

#include <nlohmann/json.hpp>
#include <httplib.h>

// RenderDoc integration - will be enabled when renderdoc_app.h is available
// #ifdef _WIN32
// #include <windows.h>
// #else
// #include <dlfcn.h>
// #endif
// #include <renderdoc_app.h>

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
    "clear_scene",
    "set_camera_position",
    "set_camera_rotation",
    "set_camera_fov",
    "teleport_camera",
    "simulate_key_press",
    "simulate_mouse_move"
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
            Logger::getInstance().info("[DebugServer] TODO: Implement teleport_player");

        } else if (cmd.type == "spawn_entity") {
            // TODO: Implement entity spawning
            // auto type = payload["entity_type"].get<std::string>();
            // app_->spawnEntity(type);
            Logger::getInstance().info("[DebugServer] TODO: Implement spawn_entity");

        } else if (cmd.type == "reload_shader") {
            // TODO: Trigger shader reload
            // app_->getRenderer()->reloadShaders();
            Logger::getInstance().info("[DebugServer] TODO: Implement reload_shader");

        } else if (cmd.type == "set_camera_position" || cmd.type == "teleport_camera") {
            // Set camera position
            auto* world = app_->getWorld();
            if (world) {
                auto& em = world->getEntityManager();
                auto* cameraSystem = world->getSystem<CameraSystem>();

                if (cameraSystem) {
                    EntityID camEntity = cameraSystem->getActiveCameraEntity();
                    if (camEntity != INVALID_ENTITY && em.hasComponent<Transform>(camEntity)) {
                        auto& transform = em.getComponent<Transform>(camEntity);

                        if (payload.contains("x")) transform.position.x = payload["x"].get<float>();
                        if (payload.contains("y")) transform.position.y = payload["y"].get<float>();
                        if (payload.contains("z")) transform.position.z = payload["z"].get<float>();

                        Logger::getInstance().info(
                            "[DebugServer] Camera position set to: (" +
                            std::to_string(transform.position.x) + ", " +
                            std::to_string(transform.position.y) + ", " +
                            std::to_string(transform.position.z) + ")"
                        );
                    }
                }
            }

        } else if (cmd.type == "set_camera_rotation") {
            // Set camera rotation (expects Euler angles in degrees: pitch, yaw, roll)
            auto* world = app_->getWorld();
            if (world) {
                auto& em = world->getEntityManager();
                auto* cameraSystem = world->getSystem<CameraSystem>();

                if (cameraSystem) {
                    EntityID camEntity = cameraSystem->getActiveCameraEntity();
                    if (camEntity != INVALID_ENTITY && em.hasComponent<Transform>(camEntity)) {
                        auto& transform = em.getComponent<Transform>(camEntity);

                        // Get current Euler angles or use defaults
                        glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));

                        if (payload.contains("pitch")) euler.x = payload["pitch"].get<float>();
                        if (payload.contains("yaw")) euler.y = payload["yaw"].get<float>();
                        if (payload.contains("roll")) euler.z = payload["roll"].get<float>();

                        // Set rotation from Euler angles
                        transform.setRotationEuler(euler.x, euler.y, euler.z);

                        Logger::getInstance().info(
                            "[DebugServer] Camera rotation set to (pitch, yaw, roll): (" +
                            std::to_string(euler.x) + ", " +
                            std::to_string(euler.y) + ", " +
                            std::to_string(euler.z) + ") degrees"
                        );
                    }
                }
            }

        } else if (cmd.type == "set_camera_fov") {
            // Set camera FOV
            auto* world = app_->getWorld();
            if (world) {
                auto& em = world->getEntityManager();
                auto* cameraSystem = world->getSystem<CameraSystem>();

                if (cameraSystem) {
                    EntityID camEntity = cameraSystem->getActiveCameraEntity();
                    if (camEntity != INVALID_ENTITY && em.hasComponent<Camera>(camEntity)) {
                        auto& camera = em.getComponent<Camera>(camEntity);

                        if (payload.contains("fov")) {
                            camera.fov = payload["fov"].get<float>();
                            camera.updateProjectionMatrix();

                            Logger::getInstance().info(
                                "[DebugServer] Camera FOV set to: " + std::to_string(camera.fov)
                            );
                        }
                    }
                }
            }
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

    // Get all entities with Transform component (most common component)
    const auto& entityIds = em.getEntitiesWithComponent<Transform>();

    for (auto entityId : entityIds) {
        json entity;
        entity["id"] = entityId;

        // Get Transform component (we know it exists)
        if (em.hasComponent<Transform>(entityId)) {
            const auto& transform = em.getComponent<Transform>(entityId);
            entity["position"] = {
                transform.position.x,
                transform.position.y,
                transform.position.z
            };
            entity["rotation"] = {
                transform.rotation.x,
                transform.rotation.y,
                transform.rotation.z
            };
            entity["scale"] = {
                transform.scale.x,
                transform.scale.y,
                transform.scale.z
            };
        }

        // Check for Renderable component
        if (em.hasComponent<Renderable>(entityId)) {
            const auto& renderable = em.getComponent<Renderable>(entityId);
            entity["mesh"] = renderable.meshPath;
            entity["material_id"] = renderable.materialId;
            entity["visible"] = renderable.isVisible;
        }

        result["entities"].push_back(entity);
    }

    result["count"] = entityIds.size();
    return result.dump(2);
}

std::string DebugServer::GetPlayerState() const {
    json state;
    // TODO: Implement player state query
    state["position"] = {0.0f, 0.0f, 0.0f};
    state["health"] = 100;
    state["note"] = "Player state query not yet implemented";

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
    stats["memory_mb"] = "not_yet_implemented";
    stats["note"] = "Vulkan stats query not yet implemented";

    return stats.dump(2);
}

// Camera debug queries
std::string DebugServer::GetCameraState() const {
    json camera;

    auto* world = app_->getWorld();
    if (!world) {
        camera["error"] = "World not initialized";
        return camera.dump(2);
    }

    auto& em = world->getEntityManager();
    auto* cameraSystem = world->getSystem<CameraSystem>();

    if (!cameraSystem) {
        camera["error"] = "Camera system not available";
        return camera.dump(2);
    }

    EntityID activeCamEntity = cameraSystem->getActiveCameraEntity();
    if (activeCamEntity == INVALID_ENTITY) {
        camera["error"] = "No active camera";
        return camera.dump(2);
    }

    // Get camera transform
    if (em.hasComponent<Transform>(activeCamEntity)) {
        const auto& transform = em.getComponent<Transform>(activeCamEntity);
        camera["position"] = {
            transform.position.x,
            transform.position.y,
            transform.position.z
        };

        // Get Euler angles from quaternion for debugging
        glm::vec3 euler = glm::eulerAngles(transform.rotation);
        camera["rotation_euler_rad"] = {
            euler.x,
            euler.y,
            euler.z
        };

        // Get direction vectors
        glm::vec3 forward = transform.getForward();
        glm::vec3 up = transform.getUp();
        glm::vec3 right = transform.getRight();

        camera["forward"] = { forward.x, forward.y, forward.z };
        camera["up"] = { up.x, up.y, up.z };
        camera["right"] = { right.x, right.y, right.z };
    }

    // Get camera component
    if (em.hasComponent<Camera>(activeCamEntity)) {
        const auto& cam = em.getComponent<Camera>(activeCamEntity);
        camera["fov"] = cam.fov;
        camera["near_plane"] = cam.nearPlane;
        camera["far_plane"] = cam.farPlane;
        camera["aspect_ratio"] = cam.aspectRatio;
        camera["is_active"] = cam.isActive;
        camera["priority"] = cam.priority;
        camera["type"] = (cam.type == Camera::Type::PERSPECTIVE) ? "perspective" : "orthographic";
    }

    camera["entity_id"] = activeCamEntity;

    return camera.dump(2);
}

std::string DebugServer::GetCameraPosition() const {
    json pos;

    auto* world = app_->getWorld();
    if (!world) {
        pos["error"] = "World not initialized";
        return pos.dump(2);
    }

    auto& em = world->getEntityManager();
    auto* cameraSystem = world->getSystem<CameraSystem>();

    if (!cameraSystem) {
        pos["error"] = "Camera system not available";
        return pos.dump(2);
    }

    EntityID activeCamEntity = cameraSystem->getActiveCameraEntity();
    if (activeCamEntity == INVALID_ENTITY) {
        pos["error"] = "No active camera";
        return pos.dump(2);
    }

    if (em.hasComponent<Transform>(activeCamEntity)) {
        const auto& transform = em.getComponent<Transform>(activeCamEntity);
        pos["x"] = transform.position.x;
        pos["y"] = transform.position.y;
        pos["z"] = transform.position.z;
    }

    return pos.dump(2);
}

// Input debug queries
std::string DebugServer::GetInputState() const {
    json input;

    // TODO: Add InputHandler state tracking
    // For now, return basic info
    input["note"] = "Input state monitoring requires InputHandler integration";
    input["controls"] = {
        {"WASD", "Camera/Player movement"},
        {"Mouse", "Camera look"},
        {"M", "Cycle materials"},
        {"1-6", "Lighting controls"},
        {"I", "Toggle inspector"},
        {"R", "Reload shaders"}
    };

    return input.dump(2);
}

// RenderDoc Integration (stubbed for now)
void DebugServer::InitRenderDoc() {
    // TODO: Implement when renderdoc_app.h is available
    Logger::getInstance().info("[DebugServer] RenderDoc integration not yet implemented");
}

void DebugServer::TriggerRenderDocCapture() {
    if (renderdoc_api_) {
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
    Logger::getInstance().info("[DebugServer] Framebuffer capture to: " + path + " (not yet implemented)");
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
        response["success"] = false;
        response["message"] = "Frame capture not yet implemented";
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

    // Camera debug endpoints
    server.Get("/api/camera", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(GetCameraState(), "application/json");
    });

    server.Get("/api/camera/position", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(GetCameraPosition(), "application/json");
    });

    server.Post("/api/camera/position", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            DebugCommand cmd;
            cmd.type = "set_camera_position";
            cmd.payload = req.body;

            bool queued = QueueCommand(cmd);

            json response;
            response["success"] = queued;
            response["message"] = queued ? "Camera position command queued" : "Command rejected";
            res.set_content(response.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 400;
        }
    });

    server.Post("/api/camera/teleport", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            DebugCommand cmd;
            cmd.type = "teleport_camera";
            cmd.payload = req.body;

            bool queued = QueueCommand(cmd);

            json response;
            response["success"] = queued;
            response["message"] = queued ? "Camera teleport command queued" : "Command rejected";
            res.set_content(response.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 400;
        }
    });

    // Input debug endpoints
    server.Get("/api/input", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(GetInputState(), "application/json");
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
