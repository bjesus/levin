#pragma once

// Manage "Run at startup" via Windows Registry
namespace Startup {
    bool isEnabled();
    void setEnabled(bool enabled);
}
