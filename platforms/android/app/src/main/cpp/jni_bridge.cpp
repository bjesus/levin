#include <jni.h>
#include <string>
#include <android/log.h>

#include "liblevin.h"

#define LOG_TAG "LevinJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Helper: get a std::string from a jstring (handles null)
static std::string jstring_to_string(JNIEnv* env, jstring js) {
    if (!js) return {};
    const char* utf = env->GetStringUTFChars(js, nullptr);
    std::string result(utf);
    env->ReleaseStringUTFChars(js, utf);
    return result;
}

extern "C" {

// --- Lifecycle ---

JNIEXPORT jlong JNICALL
Java_com_yoavmoshe_levin_LevinNative_create(
        JNIEnv* env, jobject /* this */,
        jstring watchDir, jstring dataDir, jstring stateDir,
        jlong minFreeBytes, jdouble minFreePercentage, jlong maxStorageBytes,
        jboolean runOnBattery, jboolean runOnCellular,
        jint diskCheckIntervalSecs, jint maxDownloadKbps, jint maxUploadKbps) {

    levin_config_t config = {};

    std::string watch = jstring_to_string(env, watchDir);
    std::string data = jstring_to_string(env, dataDir);
    std::string state = jstring_to_string(env, stateDir);

    config.watch_directory = watch.c_str();
    config.data_directory = data.c_str();
    config.state_directory = state.c_str();
    config.min_free_bytes = static_cast<uint64_t>(minFreeBytes);
    config.min_free_percentage = static_cast<double>(minFreePercentage);
    config.max_storage_bytes = static_cast<uint64_t>(maxStorageBytes);
    config.run_on_battery = runOnBattery ? 1 : 0;
    config.run_on_cellular = runOnCellular ? 1 : 0;
    config.disk_check_interval_secs = static_cast<int>(diskCheckIntervalSecs);
    config.max_download_kbps = static_cast<int>(maxDownloadKbps);
    config.max_upload_kbps = static_cast<int>(maxUploadKbps);
    config.stun_server = "stun.l.google.com:19302";

    levin_t* ctx = levin_create(&config);
    if (!ctx) {
        LOGE("levin_create failed");
        return 0;
    }

    LOGI("levin_create succeeded: handle=%p", ctx);
    return reinterpret_cast<jlong>(ctx);
}

JNIEXPORT void JNICALL
Java_com_yoavmoshe_levin_LevinNative_destroy(
        JNIEnv* /* env */, jobject /* this */, jlong handle) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);
    if (ctx) {
        levin_destroy(ctx);
        LOGI("levin_destroy: handle=%p", ctx);
    }
}

JNIEXPORT jint JNICALL
Java_com_yoavmoshe_levin_LevinNative_start(
        JNIEnv* /* env */, jobject /* this */, jlong handle) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);
    if (!ctx) return -1;
    int result = levin_start(ctx);
    LOGI("levin_start: result=%d", result);
    return result;
}

JNIEXPORT void JNICALL
Java_com_yoavmoshe_levin_LevinNative_stop(
        JNIEnv* /* env */, jobject /* this */, jlong handle) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);
    if (ctx) {
        levin_stop(ctx);
        LOGI("levin_stop");
    }
}

JNIEXPORT void JNICALL
Java_com_yoavmoshe_levin_LevinNative_tick(
        JNIEnv* /* env */, jobject /* this */, jlong handle) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);
    if (ctx) {
        levin_tick(ctx);
    }
}

// --- Condition Updates ---

JNIEXPORT void JNICALL
Java_com_yoavmoshe_levin_LevinNative_setEnabled(
        JNIEnv* /* env */, jobject /* this */, jlong handle, jboolean enabled) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);
    if (ctx) {
        levin_set_enabled(ctx, enabled ? 1 : 0);
    }
}

JNIEXPORT void JNICALL
Java_com_yoavmoshe_levin_LevinNative_updateBattery(
        JNIEnv* /* env */, jobject /* this */, jlong handle, jboolean onAcPower) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);
    if (ctx) {
        levin_update_battery(ctx, onAcPower ? 1 : 0);
    }
}

JNIEXPORT void JNICALL
Java_com_yoavmoshe_levin_LevinNative_updateNetwork(
        JNIEnv* /* env */, jobject /* this */, jlong handle,
        jboolean hasWifi, jboolean hasCellular) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);
    if (ctx) {
        levin_update_network(ctx, hasWifi ? 1 : 0, hasCellular ? 1 : 0);
    }
}

JNIEXPORT void JNICALL
Java_com_yoavmoshe_levin_LevinNative_updateStorage(
        JNIEnv* /* env */, jobject /* this */, jlong handle,
        jlong fsTotal, jlong fsFree) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);
    if (ctx) {
        levin_update_storage(ctx,
                             static_cast<uint64_t>(fsTotal),
                             static_cast<uint64_t>(fsFree));
    }
}

// --- Status ---

JNIEXPORT jobject JNICALL
Java_com_yoavmoshe_levin_LevinNative_getStatus(
        JNIEnv* env, jobject /* this */, jlong handle) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);

    // Find the StatusData class
    jclass cls = env->FindClass("com/yoavmoshe/levin/LevinNative$StatusData");
    if (!cls) {
        LOGE("getStatus: StatusData class not found");
        return nullptr;
    }

    jmethodID ctor = env->GetMethodID(cls, "<init>", "(IIIIIIJJJZ)V");
    if (!ctor) {
        LOGE("getStatus: StatusData constructor not found");
        return nullptr;
    }

    if (!ctx) {
        // Return zeroed status
        return env->NewObject(cls, ctor,
                              0, 0, 0, 0, 0, 0,
                              (jlong)0, (jlong)0, (jlong)0,
                              (jboolean)false);
    }

    levin_status_t status = levin_get_status(ctx);

    return env->NewObject(cls, ctor,
                          (jint)status.state,
                          (jint)status.torrent_count,
                          (jint)status.peer_count,
                          (jint)status.download_rate,
                          (jint)status.upload_rate,
                          (jint)0, // reserved
                          (jlong)status.total_downloaded,
                          (jlong)status.total_uploaded,
                          (jlong)status.disk_usage,
                          (jboolean)(status.over_budget != 0));
}

// --- Settings ---

JNIEXPORT void JNICALL
Java_com_yoavmoshe_levin_LevinNative_setDownloadLimit(
        JNIEnv* /* env */, jobject /* this */, jlong handle, jint kbps) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);
    if (ctx) {
        levin_set_download_limit(ctx, kbps);
    }
}

JNIEXPORT void JNICALL
Java_com_yoavmoshe_levin_LevinNative_setUploadLimit(
        JNIEnv* /* env */, jobject /* this */, jlong handle, jint kbps) {
    auto* ctx = reinterpret_cast<levin_t*>(handle);
    if (ctx) {
        levin_set_upload_limit(ctx, kbps);
    }
}

} // extern "C"
