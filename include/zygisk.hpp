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

struct AppSpecializeArgs {
    const char *niceName;
    const char *instructionSet;
    const char *appDataDir;
    const char *packageName;
    jint uid;
    jint gid;
    jintArray gids;
    jint runtimeFlags;
    jint mountExternal;
    jboolean isTopApp;
    jobjectArray pkgDataInfoList;
    jobjectArray whitelistedDataInfoList;
    booleanArray bindMountAppDataDirs;
    booleanArray bindMountAppStorageDirs;
};

struct ServerSpecializeArgs {
    jint uid;
    jint gid;
    jintArray gids;
    jint runtimeFlags;
    jlong permittedCapabilities;
    jlong effectiveCapabilities;
};

class Api {
public:
    virtual void setOption(int option) = 0;
    virtual ~Api() = default;
};

class ModuleBase {
public:
    virtual void onLoad(Api *api, JNIEnv *env) {}
    virtual void preAppSpecialize(AppSpecializeArgs *args) {}
    virtual void postAppSpecialize(const AppSpecializeArgs *args) {}
    virtual void preServerSpecialize(ServerSpecializeArgs *args) {}
    virtual void postServerSpecialize(const ServerSpecializeArgs *args) {}
    virtual ~ModuleBase() = default;
};

namespace internal {

struct api_table {
    void *impl;
    void (*setOption)(void *impl, int option);
    void (*preAppSpecialize)(void *impl, AppSpecializeArgs *args);
    void (*postAppSpecialize)(void *impl, const AppSpecializeArgs *args);
    void (*preServerSpecialize)(void *impl, ServerSpecializeArgs *args);
    void (*postServerSpecialize)(void *impl, const ServerSpecializeArgs *args);
};

template <typename T>
void entry_impl(api_table *table, JNIEnv *env) {
    static T module;
    static api_table saved_table;
    saved_table = *table;
    
    table->setOption = [](void *impl, int option) {
        static_cast<T&>(module).setOption(option);
    };
    table->preAppSpecialize = [](void *impl, AppSpecializeArgs *args) {
        static_cast<T&>(module).preAppSpecialize(args);
    };
    table->postAppSpecialize = [](void *impl, const AppSpecializeArgs *args) {
        static_cast<T&>(module).postAppSpecialize(args);
    };
    table->preServerSpecialize = [](void *impl, ServerSpecializeArgs *args) {
        static_cast<T&>(module).preServerSpecialize(args);
    };
    table->postServerSpecialize = [](void *impl, const ServerSpecializeArgs *args) {
        static_cast<T&>(module).postServerSpecialize(args);
    };
    
    static_cast<T&>(module).onLoad(nullptr, env);
}

} // namespace internal
} // namespace zygisk

#define REGISTER_ZYGISK_MODULE(cls) \
    extern "C" [[gnu::visibility("default")]] \
    void zygisk_module_entry(zygisk::internal::api_table *table, JNIEnv *env) { \
        zygisk::internal::entry_impl<cls>(table, env); \
    }
