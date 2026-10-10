#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <windows.h>
#include <d3d9.h>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct CVector {
    float x, y, z;
    CVector() : x(0.0f), y(0.0f), z(0.0f) {}
    CVector(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    CVector operator+(const CVector& v) const { return { x + v.x, y + v.y, z + v.z }; }
    CVector operator-(const CVector& v) const { return { x - v.x, y - v.y, z - v.z }; }
    CVector operator*(float val) const { return { x * val, y * val, z * val }; }
    CVector& operator+=(const CVector& v) { x += v.x; y += v.y; z += v.z; return *this; }
    CVector& operator-=(const CVector& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
};

static void InstallCallHook(uintptr_t at, void* dest) {
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(at), 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    *reinterpret_cast<uint8_t*>(at) = 0xE8;
    *reinterpret_cast<uintptr_t*>(at + 1) = reinterpret_cast<uintptr_t>(dest) - (at + 5);
    VirtualProtect(reinterpret_cast<void*>(at), 5, oldProtect, &oldProtect);
}

class DroneCam {
public:
    static inline bool bActive = false;
    static inline bool bHideHud = false;
    static inline bool bShowUI = false;
    static inline bool bImGuiInitialized = false;

    static inline WNDPROC oWndProc = nullptr;

    static inline CVector vPos = { 0.0f, 0.0f, 0.0f };
    static inline CVector vVelocity = { 0.0f, 0.0f, 0.0f };

    static inline float fYaw = 0.0f;
    static inline float fPitch = 0.0f;

    static inline float fMoveSpeed = 0.45f;
    static inline float fSpeedMultiplier = 1.0f;
    static inline float fBoostFactor = 2.5f;
    static inline float fCurrentFOV = 70.0f;
    static inline float fInertiaDamping = 0.86f;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (bShowUI) {
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            return true;
        }
        return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
    }

    static void NormalizeAngles() {
        if (fYaw > static_cast<float>(M_PI))  fYaw -= 2.0f * static_cast<float>(M_PI);
        if (fYaw < -static_cast<float>(M_PI)) fYaw += 2.0f * static_cast<float>(M_PI);
        fPitch = std::clamp(fPitch, -1.52f, 1.52f);
    }

    static void ToggleCamera() {
        bActive = !bActive;

        void* pCamera = reinterpret_cast<void*>(0xB6F028);
        if (bActive) {
            vPos = *reinterpret_cast<CVector*>(0xB6F028 + 0x174 + 0x1C);
            vVelocity = { 0.0f, 0.0f, 0.0f };
            fYaw = 0.0f;
            fPitch = 0.0f;
            reinterpret_cast<void(__thiscall*)(void*, void*, short, short, int)>(0x50C7C0)(pCamera, nullptr, 1, 1, 1);
        } else {
            reinterpret_cast<void(__thiscall*)(void*)>(0x50AB50)(pCamera);
            *reinterpret_cast<bool*>(0xBA6769) = true;
        }
    }

    static void OnProcess() {
        static bool bAltF3Held = false;
        bool bAltPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        bool bF3Pressed  = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;

        if (bAltPressed && bF3Pressed) {
            if (!bAltF3Held) {
                bShowUI = !bShowUI;
                bAltF3Held = true;
            }
        } else {
            bAltF3Held = false;
        }

        if (bShowUI) {
            *reinterpret_cast<short*>(0xB73458 + 0x10E) = 1;
            return;
        }

        if (!bActive) return;

        static bool bHHeld = false;
        if (GetAsyncKeyState('H') & 0x8000) {
            if (!bHHeld) {
                bHideHud = !bHideHud;
                bHHeld = true;
            }
        } else {
            bHHeld = false;
        }

        *reinterpret_cast<short*>(0xB73458 + 0x10E) = 1;

        float mouseDeltaX = -*reinterpret_cast<float*>(0xB73418);
        float mouseDeltaY = -*reinterpret_cast<float*>(0xB7341C);

        fYaw   += mouseDeltaX * 0.0035f;
        fPitch += mouseDeltaY * 0.0035f;
        NormalizeAngles();

        CVector forward(
            -sinf(fYaw) * cosf(fPitch),
             cosf(fYaw) * cosf(fPitch),
             sinf(fPitch)
        );

        CVector right(
             cosf(fYaw),
             sinf(fYaw),
             0.0f
        );

        CVector worldUp(0.0f, 0.0f, 1.0f);
        CVector inputAccel(0.0f, 0.0f, 0.0f);

        if (GetAsyncKeyState('W') & 0x8000) inputAccel += forward;
        if (GetAsyncKeyState('S') & 0x8000) inputAccel -= forward;
        if (GetAsyncKeyState('D') & 0x8000) inputAccel += right;
        if (GetAsyncKeyState('A') & 0x8000) inputAccel -= right;
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) inputAccel += worldUp;
        if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) inputAccel -= worldUp;

        float boost = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) ? fBoostFactor : 1.0f;
        float frameStep = *reinterpret_cast<float*>(0xB7CB5C);

        vVelocity = (vVelocity * fInertiaDamping) + (inputAccel * (fMoveSpeed * boost * fSpeedMultiplier * 0.14f));
        vPos += (vVelocity * frameStep);

        CVector targetLookAt = vPos + (forward * 5.0f);
        void* pCamera = reinterpret_cast<void*>(0xB6F028);
        reinterpret_cast<void(__thiscall*)(void*, CVector*, CVector*)>(0x50BEC0)(pCamera, &vPos, &targetLookAt);
        *reinterpret_cast<float*>(0xB6F028 + 0x174 + 0x38) = fCurrentFOV;
    }

    static void RenderUI() {
        if (!bShowUI) return;

        ImGui::SetNextWindowSize(ImVec2(340, 290), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("DroneCam Controller", &bShowUI, ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::Checkbox("Active [Drone Mode]", &bActive)) {
                void* pCamera = reinterpret_cast<void*>(0xB6F028);
                if (bActive) {
                    vPos = *reinterpret_cast<CVector*>(0xB6F028 + 0x174 + 0x1C);
                    vVelocity = { 0.0f, 0.0f, 0.0f };
                    fYaw = 0.0f;
                    fPitch = 0.0f;
                    reinterpret_cast<void(__thiscall*)(void*, void*, short, short, int)>(0x50C7C0)(pCamera, nullptr, 1, 1, 1);
                } else {
                    reinterpret_cast<void(__thiscall*)(void*)>(0x50AB50)(pCamera);
                    *reinterpret_cast<bool*>(0xBA6769) = true;
                }
            }

            ImGui::Checkbox("Hide HUD [H]", &bHideHud);
            ImGui::Separator();

            ImGui::SliderFloat("Base Speed", &fMoveSpeed, 0.05f, 2.0f, "%.2f");
            ImGui::SliderFloat("Speed Mult", &fSpeedMultiplier, 0.1f, 5.0f, "%.2f");
            ImGui::SliderFloat("Boost Factor", &fBoostFactor, 1.0f, 10.0f, "%.1fx");
            ImGui::SliderFloat("Damping", &fInertiaDamping, 0.50f, 0.98f, "%.2f");
            ImGui::SliderFloat("FOV", &fCurrentFOV, 20.0f, 120.0f, "%.1f deg");

            ImGui::Separator();
            if (ImGui::Button("Reset Physics", ImVec2(-1, 0))) {
                vVelocity = { 0.0f, 0.0f, 0.0f };
                fMoveSpeed = 0.45f;
                fSpeedMultiplier = 1.0f;
                fBoostFactor = 2.5f;
                fInertiaDamping = 0.86f;
                fCurrentFOV = 70.0f;
            }
        }
        ImGui::End();
    }

    static void InitImGui(IDirect3DDevice9* pDevice) {
        D3DDEVICE_CREATION_PARAMETERS params;
        if (FAILED(pDevice->GetCreationParameters(&params))) {
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        ImGui_ImplWin32_Init(params.hFocusWindow);
        ImGui_ImplDX9_Init(pDevice);

        oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(params.hFocusWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
        bImGuiInitialized = true;
    }

    static void OnRender() {
        if (bActive && bHideHud) {
            *reinterpret_cast<bool*>(0xBA6769) = false;
        }

        IDirect3DDevice9* pDevice = *reinterpret_cast<IDirect3DDevice9**>(0xC97C28);
        if (!pDevice) return;

        if (pDevice->TestCooperativeLevel() != D3D_OK) return;

        if (!bImGuiInitialized) {
            InitImGui(pDevice);
        }

        if (bImGuiInitialized) {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDrawCursor = bShowUI;

            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            RenderUI();

            ImGui::EndFrame();
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }
    }
};

static void (*orig_GameProcess)() = reinterpret_cast<void(*)()>(0x619D40);
static void __cdecl Hooked_GameProcess() {
    orig_GameProcess();
    DroneCam::OnProcess();
}

static void (*orig_Render2DStuff)() = reinterpret_cast<void(*)()>(0x71E360);
static void __cdecl Hooked_Render2DStuff() {
    orig_Render2DStuff();
    DroneCam::OnRender();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        InstallCallHook(0x53EB12, reinterpret_cast<void*>(Hooked_GameProcess));
        InstallCallHook(0x53EADC, reinterpret_cast<void*>(Hooked_Render2DStuff));
    }
    return TRUE;
}
