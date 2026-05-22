#include <jni.h>
#include <string>
#include <unistd.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>

#define TAG "ScreenOffAnim"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Minimal arm64 inline hook - replaces first 4 instructions with LDR+BR to hook,
// saves originals in a trampoline that jumps back.
static void *gOrigGoToSleep = nullptr;
static void *gOrigEntry = nullptr;
static uint32_t gSaved[5]; // 4 saved instructions + branch back to original+16

static void make_writable(void *addr, size_t len) {
    uintptr_t page = (uintptr_t)addr & ~0xFFF;
    mprotect((void *)page, len + ((uintptr_t)addr - page), PROT_READ | PROT_WRITE | PROT_EXEC);
}

static void *arm64_inline_hook(void *target, void *hook) {
    // Build trampoline: saved instructions + B back to target+16
    uint32_t *src = (uint32_t *)target;
    uint32_t *tramp = (uint32_t *)malloc(20 * sizeof(uint32_t));  // won't free, module lifetime
    memcpy(tramp, src, 16);  // save first 4 instructions

    // Branch back from trampoline to target+16
    int64_t back_off = ((uint8_t *)target + 16) - (uint8_t *)(tramp + 4);
    tramp[4] = 0x14000000 | ((back_off >> 2) & 0x03FFFFFF);

    __builtin___clear_cache(tramp, tramp + 5);

    // Patch target to jump to hook
    // LDR X16, #8 ; BR X16 ; .quad hook_addr
    make_writable(target, 32);
    uint32_t *patch = (uint32_t *)target;
    patch[0] = 0x58000050; // LDR X16, #8 (PC-relative load)
    patch[1] = 0xD61F0200; // BR X16
    *(uint64_t *)(patch + 2) = (uint64_t)hook;

    __builtin___clear_cache(target, (uint8_t *)target + 32);
    return tramp;
}

// Hook function matching goToSleep(JII)V ART-compiled entry: void(JNIEnv*, jobject, jlong, jint, jint)
static void goToSleepHook(JNIEnv *env, jobject thiz, jlong eventTime, jint reason, jint flags) {
    LOGI("goToSleep(reason=%d) -> delay 500ms", reason);
    usleep(500000);
    if (gOrigEntry) {
        auto orig = (void(*)(JNIEnv *, jobject, jlong, jint, jint))gOrigEntry;
        orig(env, thiz, eventTime, reason, flags);
    }
}

static void hookSystemServer(JNIEnv *env) {
    LOGI("Finding PowerManagerService.goToSleep(JII)V");
    jclass pmsClass = env->FindClass("com/android/server/power/PowerManagerService");
    if (env->ExceptionCheck()) { env->ExceptionClear(); LOGE("PMS class not found"); return; }
    jmethodID method = env->GetMethodID(pmsClass, "goToSleep", "(JII)V");
    if (method == nullptr) { LOGE("goToSleep method not found"); return; }

    void *artMethod = *(void **)method;
    // Try common entry point offsets in ArtMethod for Android 15
    uintptr_t entry = 0;
    for (int off : {0x20, 0x28, 0x30, 0x18, 0x38}) {
        entry = *(uintptr_t *)((uint8_t *)artMethod + off);
        if (entry > 0x1000 && entry < 0x7fffffffffffULL) break;
    }
    if (entry == 0 || entry < 0x1000) {
        LOGE("Cannot find entry point");
        return;
    }
    LOGI("ArtMethod=%p entry=0x%lx", artMethod, (unsigned long)entry);

    gOrigEntry = arm64_inline_hook((void *)entry, (void *)goToSleepHook);
    if (gOrigEntry) {
        LOGI("goToSleep hooked via arm64 inline hook!");
    } else {
        LOGE("Hook failed");
    }
}

static void onAppProcess(JNIEnv *env, const char *processName) {
    if (strcmp(processName, "system_server") != 0) return;
    LOGI("system_server detected");
    hookSystemServer(env);
}

REGISTER_ZYGISK_MODULE(ScreenOffAnim, onAppProcess)