#pragma once

#define IDI_LEVIN          101
#define IDI_LEVIN_SMALL    102

#define IDD_STATS          200
#define IDD_SETTINGS       201
#define IDD_POPULATE       202

// Stats dialog controls
#define IDC_ENABLE_CHECK   1001
#define IDC_STATE_TEXT     1002
#define IDC_DL_RATE        1003
#define IDC_UL_RATE        1004
#define IDC_DL_TOTAL       1005
#define IDC_UL_TOTAL       1006
#define IDC_TORRENTS       1007
#define IDC_BOOKS          1008
#define IDC_PEERS          1009
#define IDC_DISK_USAGE     1010
#define IDC_DISK_BUDGET    1011
#define IDC_POPULATE_BTN   1012
#define IDC_SETTINGS_BTN   1013
#define IDC_QUIT_BTN       1014

// Settings dialog controls
#define IDC_BATTERY_CHECK  2001
#define IDC_STARTUP_CHECK  2002
#define IDC_DL_LIMIT       2003
#define IDC_UL_LIMIT       2004
#define IDC_MIN_FREE       2005
#define IDC_MAX_STORAGE    2006

// Populate dialog controls
#define IDC_POPULATE_PROG  3001
#define IDC_POPULATE_TEXT  3002

// Tray message
#define WM_TRAYICON        (WM_USER + 1)
#define WM_LEVIN_UPDATE    (WM_USER + 2)

// Tray menu
#define IDM_SHOW           4001
#define IDM_SETTINGS       4002
#define IDM_QUIT           4003
