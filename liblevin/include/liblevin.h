#ifndef LIBLEVIN_H
#define LIBLEVIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Types --- */

typedef enum {
    LEVIN_STATE_OFF        = 0,
    LEVIN_STATE_PAUSED     = 1,
    LEVIN_STATE_IDLE       = 2,
    LEVIN_STATE_SEEDING    = 3,
    LEVIN_STATE_DOWNLOADING = 4
} levin_state_t;

typedef struct {
    const char* watch_directory;
    const char* data_directory;
    const char* state_directory;
    uint64_t    min_free_bytes;
    double      min_free_percentage;   /* 0.05 = 5% */
    uint64_t    max_storage_bytes;     /* 0 = unlimited */
    int         run_on_battery;        /* default: 0 */
    int         run_on_cellular;       /* default: 0 */
    int         disk_check_interval_secs; /* default: 60 */
    int         max_download_kbps;     /* 0 = unlimited */
    int         max_upload_kbps;       /* 0 = unlimited */
    const char* stun_server;           /* default: "stun.l.google.com:19302" */
} levin_config_t;

typedef struct {
    levin_state_t state;
    int           torrent_count;
    int           peer_count;
    int           download_rate;    /* bytes/sec */
    int           upload_rate;      /* bytes/sec */
    uint64_t      total_downloaded;
    uint64_t      total_uploaded;
    uint64_t      disk_usage;
    uint64_t      disk_budget;
    int           over_budget;
} levin_status_t;

typedef struct {
    char          info_hash[41];    /* hex string, null-terminated */
    const char*   name;
    uint64_t      size;
    uint64_t      downloaded;
    uint64_t      uploaded;
    int           download_rate;
    int           upload_rate;
    int           num_peers;
    double        progress;         /* 0.0 to 1.0 */
    int           is_seed;
} levin_torrent_t;

typedef struct levin_ctx levin_t;

/* --- Callbacks --- */
typedef void (*levin_state_cb)(levin_state_t old_state, levin_state_t new_state, void* userdata);
typedef void (*levin_progress_cb)(int current, int total, const char* message, void* userdata);

/* --- Lifecycle --- */
levin_t* levin_create(const levin_config_t* config);
void     levin_destroy(levin_t* ctx);
int      levin_start(levin_t* ctx);
void     levin_stop(levin_t* ctx);
void     levin_tick(levin_t* ctx);

/* --- Condition Updates (called by platform shell) --- */
void levin_update_battery(levin_t* ctx, int on_ac_power);
void levin_update_network(levin_t* ctx, int has_wifi, int has_cellular);
void levin_update_storage(levin_t* ctx, uint64_t fs_total, uint64_t fs_free);

/* --- Torrent Management --- */
int  levin_add_torrent(levin_t* ctx, const char* torrent_path);
void levin_remove_torrent(levin_t* ctx, const char* info_hash);

/* --- Status --- */
levin_status_t    levin_get_status(levin_t* ctx);
levin_torrent_t*  levin_get_torrents(levin_t* ctx, int* count);
void              levin_free_torrents(levin_torrent_t* list, int count);

/* --- Settings (runtime) --- */
void levin_set_enabled(levin_t* ctx, int enabled);
void levin_set_download_limit(levin_t* ctx, int kbps);
void levin_set_upload_limit(levin_t* ctx, int kbps);
void levin_set_run_on_battery(levin_t* ctx, int run_on_battery);
void levin_set_run_on_cellular(levin_t* ctx, int run_on_cellular);

/* --- Anna's Archive --- */
int levin_populate_torrents(levin_t* ctx, levin_progress_cb cb, void* userdata);

/* --- Callbacks --- */
void levin_set_state_callback(levin_t* ctx, levin_state_cb cb, void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* LIBLEVIN_H */
