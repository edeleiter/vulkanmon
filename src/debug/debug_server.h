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

    // Camera debug queries
    std::string GetCameraState() const;
    std::string GetCameraPosition() const;

    // Input debug queries
    std::string GetInputState() const;

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
