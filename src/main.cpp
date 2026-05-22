#include <jni.h>
#include <string>
#include <unistd.h>
#include <android/log.h>
#include <dlfcn.h>

#include "dobby.h"

#define TAG "ScreenOffAnim"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static void *gOrigGoToSleep = nullptr;

// C ABI hook 函数 - 对应 PowerManagerService.goToSleep(JII)V
// ART 编译后的 entry point 签名: void(JNIEnv*, jobject, jlong, jint, jint)
static void goToSleepHook(JNIEnv *env, jobject thiz,
                          jlong eventTime, jint reason, jint flags) {
    LOGI("goToSleep(reason=%d) -> delay 500ms", reason);
    usleep(500000);
    if (gOrigGoToSleep) {
        auto orig = (void(*)(JNIEnv*, jobject, jlong, jint, jint))gOrigGoToSleep;
        orig(env, thiz, eventTime, reason, flags);
    }
}

static void hookSystemServer(JNIEnv *env) {
    LOGI("Finding PowerManagerService.goToSleep(JII)V");
    jclass pmsClass = env->FindClass("com/android/server/power/PowerManagerService");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("PMS class not found");
        return;
    }
    jmethodID method = env->GetMethodID(pmsClass, "goToSleep", "(JII)V");
    if (method == nullptr) {
        LOGE("goToSleep method not found");
        return;
    }

    // jmethodID -> ArtMethod* (Android ART)
    void *artMethod = *(void **)method;

    // Entry point offset in ArtMethod for Android 15 (may need adjustment)
    // Common offsets: Android 14 = 0x20, Android 15 = 0x20
    uintptr_t entryPoint = *(uintptr_t *)((uint8_t *)artMethod + 0x20);

    if (entryPoint == 0) {
        // Try alternate offsets
        for (int off : {0x18, 0x20, 0x28, 0x30}) {
            entryPoint = *(uintptr_t *)((uint8_t *)artMethod + off);
            if (entryPoint > 0x1000 && entryPoint < 0x7fffffffffffULL) break;
        }
    }

    if (entryPoint == 0 || entryPoint < 0x1000) {
        LOGE("Cannot find entry point (offset may differ on Flyme)");
        return;
    }

    LOGI("ArtMethod=%p entryPoint=0x%lx", artMethod, (unsigned long)entryPoint);

    if (DobbyHook((void *)entryPoint, (void *)goToSleepHook,
                  (void **)&gOrigGoToSleep) == 0) {
        LOGI("goToSleep hooked via Dobby!");
    } else {
        LOGE("DobbyHook failed");
    }
}

static void onAppProcess(JNIEnv *env, const char *processName) {
    if (strcmp(processName, "system_server") != 0) return;
    LOGI("system_server detected");
    hookSystemServer(env);
}

REGISTER_ZYGISK_MODULE(ScreenOffAnim, onAppProcess)