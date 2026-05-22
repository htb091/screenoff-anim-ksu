#include <jni.h>
#include <string>
#include <unistd.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>

#include "zygisk.hpp"

#define TAG "ScreenOffAnim"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static void *gOrigEntry = nullptr;

static void *arm64_inline_hook(void *target, void *hook) {
    uint32_t *src = (uint32_t *)target;
    uint32_t *tramp = (uint32_t *)malloc(20 * sizeof(uint32_t));
    memcpy(tramp, src, 16);
    int64_t back_off = ((uint8_t *)target + 16) - (uint8_t *)(tramp + 4);
    tramp[4] = 0x14000000 | ((back_off >> 2) & 0x03FFFFFF);
    __builtin___clear_cache((char *)tramp, (char *)(tramp + 5));

    uintptr_t page = (uintptr_t)target & ~0xFFF;
    mprotect((void *)page, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC);
    uint32_t *patch = (uint32_t *)target;
    patch[0] = 0x58000050;
    patch[1] = 0xD61F0200;
    *(uint64_t *)(patch + 2) = (uint64_t)hook;
    __builtin___clear_cache((char *)target, (char *)target + 32);
    return tramp;
}

static void goToSleepHook(JNIEnv *env, jobject thiz, jlong eventTime, jint reason, jint flags) {
    LOGI("goToSleep(reason=%d) -> delay 500ms", reason);
    usleep(500000);
    if (gOrigEntry) {
        auto orig = (void(*)(JNIEnv *, jobject, jlong, jint, jint))gOrigEntry;
        orig(env, thiz, eventTime, reason, flags);
    }
}

static void hookPms(JNIEnv *env) {
    jclass pmsClass = env->FindClass("com/android/server/power/PowerManagerService");
    if (env->ExceptionCheck()) { env->ExceptionClear(); LOGE("PMS not found"); return; }
    jmethodID method = env->GetMethodID(pmsClass, "goToSleep", "(JII)V");
    if (method == nullptr) { LOGE("goToSleep not found"); return; }

    void *artMethod = *(void **)method;
    uintptr_t entry = 0;
    for (int off : {0x20, 0x28, 0x30, 0x18, 0x38, 0x40}) {
        entry = *(uintptr_t *)((uint8_t *)artMethod + off);
        if (entry > 0x1000 && entry < 0x7fffffffffffULL) break;
    }
    if (entry == 0 || entry < 0x1000) { LOGE("no entry point"); return; }
    LOGI("ArtMethod=%p entry=0x%lx", artMethod, (unsigned long)entry);

    gOrigEntry = arm64_inline_hook((void *)entry, (void *)goToSleepHook);
    LOGI(gOrigEntry ? "goToSleep HOOKED" : "hook FAILED");
}

class ScreenOffAnim : public zygisk::ModuleBase {
public:
    void onAppProcess(JNIEnv *env, zygisk::AppProcessArgs *args) override {
        if (strcmp(args->niceName, "system_server") != 0) return;
        LOGI("system_server detected");
        hookPms(env);
    }
};

REGISTER_ZYGISK_MODULE(ScreenOffAnim)