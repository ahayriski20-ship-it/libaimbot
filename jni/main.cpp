#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

#define TAG "riski_aimbot"
#define EXPORT __attribute__((visibility("default")))

// OFFSET ALYN SAMP (ARM 32-BIT)
#define OFF_PED_POOL           0x958E44u
#define OFF_CAMERA             0x95B060u
#define OFF_UPDATE_AIMING      0x44D97Cu 
#define OFF_PROCESS_AIMING     0x43DB20u

struct RwV3d { float x, y, z; };
struct CPool {
    void* m_pObjects;       
    uint8_t* m_byteMap;     
    int32_t m_nSize;        
};

typedef void (*fn_UpdateAimingCoors)(void* cam, RwV3d* target, float, float, float, bool);
static fn_UpdateAimingCoors gUpdateAimingCoors = nullptr;

static CPool** g_pPedPool = nullptr; 
static uintptr_t g_Camera = 0;
static bool g_ready = false;

float GetDistance(RwV3d a, RwV3d b) {
    return sqrt(pow(b.x - a.x, 2) + pow(b.y - a.y, 2) + pow(b.z - a.z, 2));
}

uintptr_t GetClosestPlayer() {
    if (!g_pPedPool || !(*g_pPedPool)) return 0;
    CPool* pool = *g_pPedPool;
    uintptr_t localPed = (uintptr_t)pool->m_pObjects; 
    if (!localPed) return 0;

    RwV3d localPos = *(RwV3d*)(localPed + 0x04);
    uintptr_t target = 0;
    float minDist = 60.0f; // Radius 60 meter

    for (int i = 1; i < pool->m_nSize; i++) {
        if (pool->m_byteMap[i] & 0x80) continue; 
        uintptr_t ped = (uintptr_t)pool->m_pObjects + (i * 0x7C4);
        if (ped < 0x100000) continue;

        RwV3d pedPos = *(RwV3d*)(ped + 0x04);
        float dist = GetDistance(localPos, pedPos);
        if (dist < minDist) {
            minDist = dist;
            target = ped;
        }
    }
    return target;
}

typedef void (*fn_CamProcess)(void* self);
static fn_CamProcess gOCamProcess = nullptr;

void hook_CamProcess(void* self) {
    if (gOCamProcess) gOCamProcess(self);
    if (!g_ready) return;

    uintptr_t target = GetClosestPlayer();
    if (target) {
        RwV3d head;
        uintptr_t pMatrix = *(uintptr_t*)(target + 0x14);
        // Pointer Guard (Fix SIGSEGV 0x3F800030)
        if (pMatrix > 0x40000000 && (pMatrix % 4 == 0)) {
            head = *(RwV3d*)(pMatrix + 0x30);
        } else {
            head = *(RwV3d*)(target + 0x04);
        }
        head.z += 0.75f; // Kunci ke kepala

        if (gUpdateAimingCoors && g_Camera) {
            gUpdateAimingCoors((void*)g_Camera, &head, 0.0f, 0.0f, 0.0f, true);
        }
    }
}

static int find_base(struct dl_phdr_info *info, size_t size, void *data) {
    if (strstr(info->dlpi_name, "libGTASA.so")) {
        *(uintptr_t *)data = info->dlpi_addr;
        return 1;
    }
    return 0;
}

static void* init_thread(void*) {
    uintptr_t base = 0;
    while (base == 0) {
        dl_iterate_phdr(find_base, &base);
        sleep(1);
    }
    sleep(8);

    void* hDobby = dlopen("libdobby.so", RTLD_NOW | RTLD_GLOBAL);
    if (!hDobby) return nullptr;
    auto dobbyHook = (int(*)(void*,void*,void**)) dlsym(hDobby, "DobbyHook");

    g_pPedPool = (CPool**)(base + OFF_PED_POOL);
    g_Camera   = (base + OFF_CAMERA);
    gUpdateAimingCoors = (fn_UpdateAimingCoors)((base + OFF_UPDATE_AIMING) | 1u);

    dobbyHook((void*)((base + OFF_PROCESS_AIMING) | 1u), (void*)hook_CamProcess, (void**)&gOCamProcess);
    g_ready = true;
    return nullptr;
}

extern "C" {
    EXPORT void* __GetModInfo() { return (void*)"riski_aimbot|1.1|Aimbot Head Fix|ahayriski"; }
    EXPORT void OnModLoad() {
        pthread_t t;
        pthread_create(&t, nullptr, init_thread, nullptr);
        pthread_detach(t);
    }
}
