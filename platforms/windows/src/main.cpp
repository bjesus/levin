#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
// Require Vista+ for IP Helper v2 (GetIfTable2, NotifyIpInterfaceChange)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <commctrl.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <fstream>
#include <filesystem>

#include "resource.h"
#include "levin_engine.h"
#include "power_monitor.h"
#include "storage_monitor.h"
#include "startup.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "iphlpapi.lib")

// --- Globals ---
static HINSTANCE g_hInstance;
static HWND g_hwndHidden;
static HWND g_hwndStats = nullptr;
static LevinEngine* g_engine = nullptr;
static PowerMonitor* g_power = nullptr;
static StorageMonitor* g_storage = nullptr;
static NOTIFYICONDATAW g_nid{};

// Network change callback handle
static HANDLE g_netNotifyHandle = nullptr;

// --- Formatting helpers ---
static std::string formatBytes(uint64_t bytes) {
    char buf[64];
    if (bytes >= 1073741824ULL)
        snprintf(buf, sizeof(buf), "%.1f GB", bytes / 1073741824.0);
    else if (bytes >= 1048576ULL)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1048576.0);
    else if (bytes >= 1024ULL)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    return buf;
}

static std::string formatRate(int bytes_per_sec) {
    return formatBytes(bytes_per_sec) + "/s";
}

static std::string formatNumber(int n) {
    if (n < 1000) return std::to_string(n);
    char buf[32];
    if (n >= 1000000)
        snprintf(buf, sizeof(buf), "%d,%03d,%03d", n / 1000000, (n / 1000) % 1000, n % 1000);
    else
        snprintf(buf, sizeof(buf), "%d,%03d", n / 1000, n % 1000);
    return buf;
}

static const char* stateString(int state) {
    switch (state) {
    case 0: return "Off";
    case 1: return "Paused";
    case 2: return "Idle";
    case 3: return "Seeding";
    case 4: return "Downloading";
    default: return "Unknown";
    }
}

// --- Network monitoring ---
static void checkNetwork() {
    if (!g_engine) return;
    // Simple check: can we reach the internet?
    DWORD flags = 0;
    // Use NLA / connectivity check
    MIB_IF_TABLE2* table = nullptr;
    bool connected = false;
    if (GetIfTable2(&table) == NO_ERROR) {
        for (ULONG i = 0; i < table->NumEntries; i++) {
            auto& row = table->Table[i];
            if (row.OperStatus == IfOperStatusUp &&
                row.Type != IF_TYPE_SOFTWARE_LOOPBACK &&
                row.MediaConnectState == MediaConnectStateConnected) {
                connected = true;
                break;
            }
        }
        FreeMibTable(table);
    }
    g_engine->updateNetwork(connected);
}

static void WINAPI networkChangeCallback(PVOID /*context*/, PMIB_IPINTERFACE_ROW /*row*/,
                                          MIB_NOTIFICATION_TYPE /*type*/) {
    checkNetwork();
}

// --- Update stats dialog ---
static void updateStatsDialog() {
    if (!g_hwndStats || !g_engine) return;

    auto st = g_engine->getStatus();

    SetDlgItemTextA(g_hwndStats, IDC_STATE_TEXT, stateString(st.state));
    SetDlgItemTextA(g_hwndStats, IDC_DL_RATE, formatRate(st.download_rate).c_str());
    SetDlgItemTextA(g_hwndStats, IDC_UL_RATE, formatRate(st.upload_rate).c_str());
    SetDlgItemTextA(g_hwndStats, IDC_DL_TOTAL, formatBytes(st.total_downloaded).c_str());
    SetDlgItemTextA(g_hwndStats, IDC_UL_TOTAL, formatBytes(st.total_uploaded).c_str());
    SetDlgItemTextA(g_hwndStats, IDC_TORRENTS, formatNumber(st.torrent_count).c_str());
    SetDlgItemTextA(g_hwndStats, IDC_BOOKS, formatNumber(st.file_count).c_str());
    SetDlgItemTextA(g_hwndStats, IDC_PEERS, formatNumber(st.peer_count).c_str());

    auto usage_str = formatBytes(st.disk_usage);
    if (st.over_budget) usage_str += " (!)";
    SetDlgItemTextA(g_hwndStats, IDC_DISK_USAGE, usage_str.c_str());
    SetDlgItemTextA(g_hwndStats, IDC_DISK_BUDGET, formatBytes(st.disk_budget).c_str());

    CheckDlgButton(g_hwndStats, IDC_ENABLE_CHECK, st.enabled ? BST_CHECKED : BST_UNCHECKED);
}

// --- Populate torrents (runs in a thread) ---
struct PopulateCtx {
    HWND dlg;
    LevinEngine* engine;
    int result;
};

static void populateProgress(int current, int total, const char* msg, void* ud) {
    auto* ctx = static_cast<PopulateCtx*>(ud);
    if (ctx->dlg) {
        SendDlgItemMessageA(ctx->dlg, IDC_POPULATE_PROG, PBM_SETRANGE32, 0, total);
        SendDlgItemMessageA(ctx->dlg, IDC_POPULATE_PROG, PBM_SETPOS, current, 0);
        if (msg) SetDlgItemTextA(ctx->dlg, IDC_POPULATE_TEXT, msg);
    }
}

static DWORD WINAPI populateThread(LPVOID param) {
    auto* ctx = static_cast<PopulateCtx*>(param);
    ctx->result = ctx->engine->populateTorrents(populateProgress, ctx);
    PostMessage(ctx->dlg, WM_CLOSE, 0, 0);
    return 0;
}

static INT_PTR CALLBACK PopulateDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_CLOSE:
        EndDialog(hwnd, 0);
        return TRUE;
    }
    return FALSE;
}

static void doPopulate() {
    PopulateCtx ctx{};
    ctx.engine = g_engine;
    ctx.result = -1;

    // Show modeless progress dialog
    ctx.dlg = CreateDialogParam(g_hInstance, MAKEINTRESOURCE(IDD_POPULATE),
                                 g_hwndStats ? g_hwndStats : g_hwndHidden,
                                 PopulateDlgProc, 0);
    ShowWindow(ctx.dlg, SW_SHOW);

    // Run populate in a thread
    HANDLE hThread = CreateThread(nullptr, 0, populateThread, &ctx, 0, nullptr);

    // Pump messages while thread runs
    MSG m;
    while (WaitForSingleObject(hThread, 0) == WAIT_TIMEOUT) {
        while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (!IsDialogMessage(ctx.dlg, &m)) {
                TranslateMessage(&m);
                DispatchMessage(&m);
            }
        }
        Sleep(50);
    }
    CloseHandle(hThread);

    if (IsWindow(ctx.dlg)) DestroyWindow(ctx.dlg);

    if (ctx.result >= 0) {
        char msg_buf[128];
        snprintf(msg_buf, sizeof(msg_buf), "Downloaded %d torrent files.", ctx.result);
        MessageBoxA(g_hwndStats ? g_hwndStats : g_hwndHidden, msg_buf, "Levin", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(g_hwndStats ? g_hwndStats : g_hwndHidden,
                     "Failed to fetch torrents. Check your internet connection.",
                     "Levin", MB_OK | MB_ICONWARNING);
    }
}

// Forward declaration
static INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

// --- Stats dialog proc ---
static INT_PTR CALLBACK StatsDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG:
        g_hwndStats = hwnd;
        updateStatsDialog();
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_ENABLE_CHECK:
            if (g_engine) {
                bool checked = IsDlgButtonChecked(hwnd, IDC_ENABLE_CHECK) == BST_CHECKED;
                g_engine->setEnabled(checked);
            }
            return TRUE;

        case IDC_POPULATE_BTN:
            doPopulate();
            return TRUE;

        case IDC_SETTINGS_BTN:
            // Show settings dialog (modal)
            DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SETTINGS), hwnd,
                           SettingsDlgProc, 0);
            return TRUE;

        case IDC_QUIT_BTN:
            DestroyWindow(hwnd);
            PostQuitMessage(0);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        g_hwndStats = nullptr;
        DestroyWindow(hwnd);
        return TRUE;
    }
    return FALSE;
}

// --- Settings dialog proc ---
static INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        // Load current values
        CheckDlgButton(hwnd, IDC_BATTERY_CHECK,
                       g_engine ? (g_engine->getStatus().enabled ? BST_CHECKED : BST_UNCHECKED) : BST_UNCHECKED);
        // Read from settings file
        auto loadInt = [](const char* key, int def) -> int {
            // Re-use the engine's settings loading
            std::string path = LevinEngine::appDataDir() + "\\settings.ini";
            std::ifstream in(path);
            std::string line, prefix = std::string(key) + "=";
            while (std::getline(in, line)) {
                if (line.compare(0, prefix.size(), prefix) == 0)
                    try { return std::stoi(line.substr(prefix.size())); } catch (...) { return def; }
            }
            return def;
        };
        auto loadDbl = [](const char* key, double def) -> double {
            std::string path = LevinEngine::appDataDir() + "\\settings.ini";
            std::ifstream in(path);
            std::string line, prefix = std::string(key) + "=";
            while (std::getline(in, line)) {
                if (line.compare(0, prefix.size(), prefix) == 0)
                    try { return std::stod(line.substr(prefix.size())); } catch (...) { return def; }
            }
            return def;
        };

        CheckDlgButton(hwnd, IDC_BATTERY_CHECK,
                       loadInt("run_on_battery", 0) ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_STARTUP_CHECK,
                       Startup::isEnabled() ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(hwnd, IDC_DL_LIMIT, loadInt("max_download_kbps", 0), FALSE);
        SetDlgItemInt(hwnd, IDC_UL_LIMIT, loadInt("max_upload_kbps", 0), FALSE);

        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", loadDbl("min_free_gb", 2.0));
        SetDlgItemTextA(hwnd, IDC_MIN_FREE, buf);
        snprintf(buf, sizeof(buf), "%.1f", loadDbl("max_storage_gb", 0.0));
        SetDlgItemTextA(hwnd, IDC_MAX_STORAGE, buf);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK: {
            // Save settings
            bool runOnBattery = IsDlgButtonChecked(hwnd, IDC_BATTERY_CHECK) == BST_CHECKED;
            bool startup = IsDlgButtonChecked(hwnd, IDC_STARTUP_CHECK) == BST_CHECKED;
            int dlLimit = GetDlgItemInt(hwnd, IDC_DL_LIMIT, nullptr, FALSE);
            int ulLimit = GetDlgItemInt(hwnd, IDC_UL_LIMIT, nullptr, FALSE);

            char buf[64];
            GetDlgItemTextA(hwnd, IDC_MIN_FREE, buf, sizeof(buf));
            double minFree = atof(buf);
            GetDlgItemTextA(hwnd, IDC_MAX_STORAGE, buf, sizeof(buf));
            double maxStorage = atof(buf);

            if (g_engine) {
                g_engine->setRunOnBattery(runOnBattery);
                g_engine->setDownloadLimit(dlLimit);
                g_engine->setUploadLimit(ulLimit);
                g_engine->setDiskLimits(minFree, maxStorage);
            }
            Startup::setEnabled(startup);

            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// --- Show stats dialog ---
static void showStats() {
    if (g_hwndStats) {
        SetForegroundWindow(g_hwndStats);
        return;
    }
    // Use modeless dialog
    g_hwndStats = CreateDialogParam(g_hInstance, MAKEINTRESOURCE(IDD_STATS),
                                     nullptr, StatsDlgProc, 0);
    ShowWindow(g_hwndStats, SW_SHOW);
}

// --- Show settings dialog (modal from stats, or standalone) ---
static void showSettings() {
    DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_SETTINGS),
                   g_hwndStats ? g_hwndStats : g_hwndHidden,
                   SettingsDlgProc, 0);
}

// --- Tray icon context menu ---
static void showContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_SHOW, L"Show Levin");
    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"Settings...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_QUIT, L"Quit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}

// --- Hidden window proc ---
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == WM_LBUTTONDBLCLK) {
            showStats();
        } else if (LOWORD(lp) == WM_RBUTTONUP) {
            showContextMenu(hwnd);
        }
        return 0;

    case WM_LEVIN_UPDATE:
        updateStatsDialog();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_SHOW:
            showStats();
            return 0;
        case IDM_SETTINGS:
            showSettings();
            return 0;
        case IDM_QUIT:
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_POWERBROADCAST:
        if (g_power) g_power->onPowerChange();
        return TRUE;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// --- Tray icon setup ---
static void createTrayIcon(HWND hwnd) {
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_LEVIN_SMALL));
    wcscpy_s(g_nid.szTip, L"Levin");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void removeTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

// --- Entry point ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_hInstance = hInstance;

    // Init common controls for progress bar etc.
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&icc);

    // Register hidden window class
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LevinHidden";
    RegisterClassExW(&wc);

    g_hwndHidden = CreateWindowExW(0, L"LevinHidden", L"Levin",
                                    0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hInstance, nullptr);

    // Create tray icon
    createTrayIcon(g_hwndHidden);

    // Create engine
    g_engine = new LevinEngine(g_hwndHidden, WM_LEVIN_UPDATE);

    // Power monitor
    g_power = new PowerMonitor([](bool onAC) {
        if (g_engine) g_engine->updateBattery(onAC);
    });
    g_power->start();

    // Ensure directories exist before storage monitor polls
    std::error_code ec;
    std::filesystem::create_directories(LevinEngine::dataDir(), ec);

    // Storage monitor
    g_storage = new StorageMonitor(LevinEngine::dataDir(), [](uint64_t total, uint64_t free_bytes) {
        if (g_engine) g_engine->updateStorage(total, free_bytes);
    });
    g_storage->start();

    // Network monitor
    checkNetwork();
    NotifyIpInterfaceChange(AF_UNSPEC, networkChangeCallback, nullptr, FALSE, &g_netNotifyHandle);

    // Start engine
    g_engine->start();

    // First-run: prompt to populate if no torrents
    if (!g_engine->hasExistingTorrents()) {
        // Use a timer to show the dialog after the message loop starts
        SetTimer(g_hwndHidden, 1, 500, [](HWND hwnd, UINT, UINT_PTR id, DWORD) {
            KillTimer(hwnd, id);
            showStats();
            // Give UI time to appear
            SetTimer(hwnd, 2, 300, [](HWND hwnd2, UINT, UINT_PTR id2, DWORD) {
                KillTimer(hwnd2, id2);
                int r = MessageBoxA(g_hwndStats ? g_hwndStats : g_hwndHidden,
                    "No torrent files found. Would you like to fetch them from Anna's Archive?",
                    "Levin", MB_YESNO | MB_ICONQUESTION);
                if (r == IDYES) doPopulate();
            });
        });
    }

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (g_hwndStats && IsDialogMessage(g_hwndStats, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    if (g_netNotifyHandle) CancelMibChangeNotify2(g_netNotifyHandle);
    g_engine->stop();
    delete g_storage;
    delete g_power;
    delete g_engine;
    removeTrayIcon();

    return 0;
}
