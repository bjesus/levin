#ifndef LEVIN_LOG_H
#define LEVIN_LOG_H

#if defined(__ANDROID__)
#include <android/log.h>
#define LEVIN_LOG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "LevinCore", fmt, ##__VA_ARGS__)
#else
#define LEVIN_LOG(fmt, ...) do {} while(0)
#endif

#endif // LEVIN_LOG_H
