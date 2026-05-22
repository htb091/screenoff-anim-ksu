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

__attribute__((constructor))
static void on_load() {
    LOGI("=== .so LOADED (constructor) ===");
}

static void *gOrigEntry = nullptr;

#if defined(__aarch64__)

static void *do_inline_hook(void *target, void *hook) {
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

#elif defined(__arm__)

static void *do_inline_hook(void *target, void *hook) {
    uint32_t *src = (uint32_t *)target;
    uint32_t *tramp = (uint32_t *)malloc(8 * sizeof(uint32_t));
    memcpy(tramp, src, 8);
    tramp[2] = 0xe51ff004; // LDR PC, [PC, #-4]
    tramp[3] = (uint32_t)((uintptr_t)target + 8);
    __builtin___clear_cache((char *)tramp, (char *)(tramp + 4));

    uintptr_t page = (uintptr_t)target & ~0xFFF;
    mprotect((void *)page, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC);
    uint32_t *patch = (uint32_t *)target;
    patch[0] = 0xe51ff004; // LDR PC, [PC, #-4]
    patch[1] = (uint32_t)(uintptr_t)hook;
    __builtin___clear_cache((char *)target, (char *)target + 8);
    return tramp;
}

#else
#error "Unsupported architecture"
#endif

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
#if defined(__aarch64__)
    for (int off : {0x20, 0x28, 0x30, 0x18, 0x38, 0x40}) {
#else
    for (int off : {0x10, 0x14, 0x18, 0x1c, 0x20, 0x24}) {
#endif
        entry = *(uintptr_t *)((uint8_t *)artMethod + off);
        if (entry > 0x1000
#if defined(__aarch64__)
            && entry < 0x7fffffffffffULL
#else
            && entry < 0xffffffff
#endif
        ) break;
    }
    if (entry == 0 || entry < 0x1000) { LOGE("no entry point"); return; }
    LOGI("ArtMethod=%p entry=0x%lx", artMethod, (unsigned long)entry);

    gOrigEntry = do_inline_hook((void *)entry, (void *)goToSleepHook);
    LOGI(gOrigEntry ? "goToSleep HOOKED" : "hook FAILED");
}

static bool isSystemServer() {
    char cmdline[256] = {0};
    FILE *f = fopen("/proc/self/cmdline", "r");
    if (f) {
        fread(cmdline, 1, sizeof(cmdline) - 1, f);
        fclose(f);
    }
    return strcmp(cmdline, "system_server") == 0;
}

class ScreenOffAnim : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        LOGI("onLoad called! pid=%d", getpid());
        if (isSystemServer()) {
            LOGI("Running in system_server");
            hookPms(env);
        } else {
            LOGI("Running in other process");
        }
    }
    
    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        LOGI("preServerSpecialize called! pid=%d", getpid());
    }
};

REGISTER_ZYGISK_MODULE(ScreenOffAnim)
