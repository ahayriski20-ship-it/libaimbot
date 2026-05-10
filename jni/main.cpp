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

// ═══════════════════════════════════════════════════════
// OFFSET ALYN SAMP (ARM 32-BIT)
// ═══════════════════════════════════════════════════════
#define OFF_PED_POOL           0x958E44u
#define OFF_CAMERA             0x95B060u
#define OFF_UPDATE_AIMING      0x44D97Cu // Alamat UpdateAimingCoors

struct RwV3d { float x, y, z; };
struct CPool {
    void* m_pObjects;       
    uint8_t* m_byteMap;     
    int32_t m_nSize;        
};

// Fungsi internal game untuk update posisi aim camera
typedef void (*fn_UpdateAimingCoors)(void* cam, RwV3d* target, float, float, float, bool);
static fn_UpdateAimingCoors gUpdateAimingCoors = nullptr;

static CPool** g_pPedPool = nullptr; 
static uintptr_t g_Camera = 0;
static bool g_ready = false;

// ═══════════════════════════════════════════════════════
// LOGIKA MENCARI TARGET TERDEKAT
// ═══════════════════════════════════════════════════════
float GetDistance(RwV3d a, RwV3d b) {
    return sqrt(pow(b.x - a.x, 2) + pow(b.y - a.y, 2) + pow(b.z - a.z, 2));
}

uintptr_t GetClosestPlayer() {
    if (!g_pPedPool || !(*g_pPedPool)) return 0;
    CPool* pool = *g_pPedPool;

    uintptr_t localPlayer = (uintptr_t)pool->m_pObjects; // Slot 0 biasanya local player
    RwV3d localPos = *(RwV3d*)(localPlayer + 0x04);

    uintptr_t target = 0;
    float minDist = 50.0f; // Range Aimbot (50 meter)

    for (int i = 1; i < pool->m_nSize; i++) { // Mulai dari i=1 agar tidak mengunci diri sendiri
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

// ═══════════════════════════════════════════════════════
// HOOKING UPDATE AIMING
// ═══════════════════════════════════════════════════════
typedef void (*fn_CamProcess)(void* self);
static fn_CamProcess gOCamProcess = nullptr;

void hook_CamProcess(void* self) {
    if (gOCamProcess) gOCamProcess(self);

    if (g_ready) {
        uintptr_t targetPed = GetClosestPlayer();
        if (targetPed) {
            RwV3d headPos;
            uintptr_t pMatrix = *(uintptr_t*)(targetPed + 0x14);
            
            // Anti-Crash Pointer Check (Fix 0x3F800030)
            if (pMatrix > 0x40000000 && (pMatrix % 4 == 0)) {
                headPos = *(RwV3d*)(pMatrix + 0x30);
            } else {
                headPos = *(RwV3d*)(targetPed + 0x04);
            }
            
            headPos.z += 0.7f; // Kunci ke arah kepala

            // Panggil UpdateAimingCoors untuk memaksa camera mengunci target
            if (gUpdateAimingCoors && g_Camera) {
                gUpdateAimingCoors((void*)g_Camera, &headPos, 0.0f, 0.0f, 0.0f, true);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════
// INIT THREAD
// ═══════════════════════════════════════════════════════
static int find_lib_base(struct dl_phdr_info *info, size_t size, void *data) {
    if (strstr(info->dlpi_name, "libGTASA.so")) {
        *(uintptr_t *)data = info->dlpi_addr;
        return 1; 
    }
    return 0;
}

#define T_PTR(a) ((void*)((a) | 1u))

static void* init_thread(void*) {
    uintptr_t base = 0;
    while (base == 0) {
        dl_iterate_phdr(find_lib_base, &base);
        sleep(1);
    }
    
    sleep(8); // Delay aman

    void* hDobby = dlopen("libdobby.so", RTLD_NOW | RTLD_GLOBAL);
    if (!hDobby) return nullptr;
    auto dobbyHook = (int(*)(void*,void*,void**)) dlsym(hDobby, "DobbyHook");

    g_pPedPool = (CPool**)(base + OFF_PED_POOL);
    g_Camera   = (base + OFF_CAMERA);
    gUpdateAimingCoors = (fn_UpdateAimingCoors)T_PTR(base + OFF_UPDATE_AIMING);

    // Hook di fungsi Camera Process (Proses Aiming Weapon)
    void* targetHook = T_PTR(base + 0x43DB20u); // Offset Process_AimWeapon
    dobbyHook(targetHook, (void*)hook_CamProcess, (void**)&gOCamProcess);

    g_ready = true;
    return nullptr;
}

extern "C" {
    EXPORT void* __GetModInfo() {
        return (void*)"riski_aimbot|1.0|Aimbot Auto-Head|ahayriski";
    }

    EXPORT void OnModLoad() {
        pthread_t t;
        pthread_create(&t, nullptr, init_thread, nullptr);
        pthread_detach(t);
    }
}
