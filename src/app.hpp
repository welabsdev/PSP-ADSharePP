#pragma once

namespace adshare {

class Application final {
public:
    Application() noexcept = default;
    ~Application() noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    void initialize();
    void shutdown() noexcept;
    void main_loop();

    bool initialized_ = false;
    bool shutdown_done_ = false;
};

} // namespace adshare
