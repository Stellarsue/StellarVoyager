#pragma once

#include <functional>
#include <memory>

struct SystemSnapshot;

// Native macOS menu bar readout. Only compiled on Apple platforms.
class MacMenuBarStatus
{
public:
    MacMenuBarStatus(std::function<void()> toggleDashboard,
                     std::function<void()> showWindow,
                     std::function<void()> installFonts,
                     std::function<void()> quit,
                     bool fontAvailable);
    ~MacMenuBarStatus();

    MacMenuBarStatus(const MacMenuBarStatus &) = delete;
    MacMenuBarStatus &operator=(const MacMenuBarStatus &) = delete;

    void update(const SystemSnapshot &snapshot);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
