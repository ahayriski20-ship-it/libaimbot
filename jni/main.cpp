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
#define OFF_GET_BONE_POS       0x5E4280u 

struct RwV3d { float x, y, z; };
struct CPool {
    void* m_pObjects;       
    uint8_t* m_byteMap;     
    int32_t m_nSize;        
};

typedef void (*fn_UpdateAimingCoors)(void* cam, RwV3d* target, float, float, float, bool);
static fn_UpdateAimingCoors gUpdateAimingCoors = nullptr;

typedef int (*fn_GetBonePosition)(void* ped, RwV3d* outPos, int boneId, bool updateBones);
static fn_GetBonePosition gGetBonePosition = nullptr;

static CPool** g_pPedPool = nullptr; 
static uintptr_t g_Camera = 0;
static bool g_ready = false;

// FIX COMPILER: Hindari powf yang sering bikin build error
float GetDistance(RwV3d a, RwV3d b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

uintptr_t GetClosestPlayer() {
    if (!g_pPedPool || !(*g_pPedPool)) return 0;
    CPool* pool = *g_pPedPool;
    
    uintptr_t localPed = (uintptr_t)pool->m_pObjects; 
    if (!localPed || (pool->m_byteMap[0] & 0x80)) return 0;

    RwV3d localPos = *(RwV3d*)(localPed + 0x04);
    uintptr_t target = 0;
    float minDist = 60.0f; 

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
    if (target && gGetBonePosition && gUpdateAimingCoors && g_Camera) {
        RwV3d headPos = {0.0f, 0.0f, 0.0f};
        
        // Memanggil fungsi internal untuk tulang kepala (Bone ID 8)
        gGetBonePosition((void*)target, &headPos, 8, false);

        if (headPos.x != 0.0f && headPos.y != 0.0f) {
            gUpdateAimingCoors((void*)g_Camera, &headPos, 0.0f, 0.0f, 0.0f, true);
        }
    }
}

static int find_base(struct dl_phdr_info *info, size_t size, void *data) {
    if (info->dlpi_name && strstr(info->dlpi_name, "libGTASA.so")) {
        *(uintptr_t *)data = info->dlpi_addr;
        return 1;
    }
    return 0;
}

#define T_PTR(a) ((void*)((a) | 1u))

static void* init_thread(void*) {
    uintptr_t base = 0;
    while (base == 0) {
        dl_iterate_phdr(find_base, &base);
        sleep(1);
    }
    sleep(10);

    void* hDobby = dlopen("libdobby.so", RTLD_NOW | RTLD_GLOBAL);
    if (!hDobby) return nullptr;
    auto dobbyHook = (int(*)(void*,void*,void**)) dlsym(hDobby, "DobbyHook");

    g_pPedPool = (CPool**)(base + OFF_PED_POOL);
    g_Camera   = (base + OFF_CAMERA);
    
    gUpdateAimingCoors = (fn_UpdateAimingCoors)T_PTR(base + OFF_UPDATE_AIMING);
    gGetBonePosition   = (fn_GetBonePosition)T_PTR(base + OFF_GET_BONE_POS);

    dobbyHook((void*)T_PTR(base + OFF_PROCESS_AIMING), (void*)hook_CamProcess, (void**)&gOCamProcess);
    
    g_ready = true;
    return nullptr;
}

extern "C" {
    EXPORT void* __GetModInfo() { return (void*)"riski_aimbot|2.0|Bone Aimbot|ahayriski"; }
    EXPORT void OnModLoad() {
        pthread_t t;
        pthread_create(&t, nullptr, init_thread, nullptr);
        pthread_detach(t);
    }
}
