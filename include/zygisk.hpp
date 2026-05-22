#pragma once
#include <jni.h>
#include <cstdint>
#include <cstring>

namespace zygisk {
struct AppProcessArgs {
    const char *niceName;
    jboolean *isChildZygote;
    jboolean *isExternal;
};

class ModuleBase {
public:
    virtual void onAppProcess(JNIEnv *env, AppProcessArgs *args) {}
    virtual ~ModuleBase() = default;
};
}

#define REGISTER_ZYGISK_MODULE(cls, ...) \
    static cls _zygisk_module_##cls; \
    extern "C" [[gnu::visibility("default")]] uint32_t zygisk_module_api_version(void) { return 1; } \
    extern "C" [[gnu::visibility("default")]] void zygisk_module_onAppProcess(JNIEnv *env, zygisk::AppProcessArgs *args) { \
        _zygisk_module_##cls.onAppProcess(env, args); \
    }