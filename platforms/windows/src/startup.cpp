#include "startup.h"
#include <windows.h>
#include <string>

static const char* REG_KEY = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char* REG_VALUE = "Levin";

bool Startup::isEnabled() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD type;
    DWORD size = 0;
    bool exists = (RegQueryValueExA(hKey, REG_VALUE, nullptr, &type, nullptr, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return exists;
}

void Startup::setEnabled(bool enabled) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return;

    if (enabled) {
        // Get current executable path
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);

        // Quote the path to handle spaces
        std::string quoted = std::string("\"") + path + "\"";
        RegSetValueExA(hKey, REG_VALUE, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(quoted.c_str()),
                       static_cast<DWORD>(quoted.size() + 1));
    } else {
        RegDeleteValueA(hKey, REG_VALUE);
    }

    RegCloseKey(hKey);
}
