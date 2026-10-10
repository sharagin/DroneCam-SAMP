#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <windows.h>
#include <d3d9.h>

#include "plugin.h"
#include "CCamera.h"
#include "CTimer.h"
#include "CPad.h"
#include "CHud.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

using namespace plugin;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

float &CTimer::ms_fTimeStep = *reinterpret_cast<float*>(0xB7CB5C);
bool &CHud::m_Wants_To_Draw_Hud = *reinterpret_cast<bool*>(0xBA6769);
CCamera &TheCamera = *reinterpret_cast<CCamera*>(0xB6F028);

CPad *CPad::GetPad(int padNumber) {
    return reinterpret_cast<CPad*(__cdecl*)(int)>(0x53FB70)(padNumber);
}

void CCamera::TakeControl(CEntity *target, short mode, short switchStyle, int performCut) {
    reinterpret_cast<void(__thiscall*)(CCamera*, CEntity*, short, short, int)>(0x50C7C0)(this, target, mode, switchStyle, performCut);
}

void CCamera::Restore() {
    reinterpret_cast<void(__thiscall*)(CCamera*)>(0x50AB50)(this);
}

void CCamera::SetCamPositionForFixedMode(CVector *pos, CVector *lookAt) {
    reinterpret_cast<void(__thiscall*)(CCamera*, CVector*, CVector*)>(0x50BEC0)(this, pos, lookAt);
}

class DroneCam {
private:
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

        if (bActive) {
            vPos = TheCamera.GetPosition();
            vVelocity = CVector(0.0f, 0.0f, 0.0f);
            fYaw = 0.0f;
            fPitch = 0.0f;
            TheCamera.TakeControl(nullptr, 0, 1, 1);
        } else {
            TheCamera.Restore();
            CHud::m_Wants_To_Draw_Hud = true;
        }
    }

    static void ProcessInput() {
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
            CPad::GetPad(0)->DisablePlayerControls = 1;
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

        CPad::GetPad(0)->DisablePlayerControls = 1;

        float mouseDeltaX = -static_cast<float>(CPad::GetPad(0)->NewMouseControllerState.X);
        float mouseDeltaY = -static_cast<float>(CPad::GetPad(0)->NewMouseControllerState.Y);

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

        if (GetAsyncKeyState('W') & 0x8000) inputAccel = inputAccel + forward;
        if (GetAsyncKeyState('S') & 0x8000) inputAccel = inputAccel - forward;
        if (GetAsyncKeyState('D') & 0x8000) inputAccel = inputAccel + right;
        if (GetAsyncKeyState('A') & 0x8000) inputAccel = inputAccel - right;
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) inputAccel = inputAccel + worldUp;
        if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) inputAccel = inputAccel - worldUp;

        float boost = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) ? fBoostFactor : 1.0f;
        float frameStep = CTimer::ms_fTimeStep;

        vVelocity = (vVelocity * fInertiaDamping) + (inputAccel * (fMoveSpeed * boost * fSpeedMultiplier * 0.14f));
        vPos = vPos + (vVelocity * frameStep);

        CVector targetLookAt = vPos + (forward * 5.0f);
        TheCamera.SetCamPositionForFixedMode(&vPos, &targetLookAt);
        TheCamera.m_asCams[TheCamera.m_nActiveCam].m_fFOV = fCurrentFOV;
    }

    static void RenderUI() {
        if (!bShowUI) return;

        ImGui::SetNextWindowSize(ImVec2(340, 290), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("DroneCam Controller", &bShowUI, ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::Checkbox("Active [Drone Mode]", &bActive)) {
                if (bActive) {
                    vPos = TheCamera.GetPosition();
                    vVelocity = CVector(0.0f, 0.0f, 0.0f);
                    fYaw = 0.0f;
                    fPitch = 0.0f;
                    TheCamera.TakeControl(nullptr, 0, 1, 1);
                } else {
                    TheCamera.Restore();
                    CHud::m_Wants_To_Draw_Hud = true;
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
                vVelocity = CVector(0.0f, 0.0f, 0.0f);
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

public:
    DroneCam() {
        Events::gameProcessEvent += []() {
            ProcessInput();
        };

        Events::drawingEvent += []() {
            if (bActive && bHideHud) {
                CHud::m_Wants_To_Draw_Hud = false;
            }

            IDirect3DDevice9* pDevice = *reinterpret_cast<IDirect3DDevice9**>(0xC97C28);
            if (!pDevice) return;

            if (pDevice->TestCooperativeLevel() != D3D_OK) return;

            if (!bImGuiInitialized) {
                InitImGui(pDevice);
            }

            if (bImGuiInitialized) {
                ImGui_ImplDX9_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                RenderUI();

                ImGui::EndFrame();
                ImGui::Render();
                ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            }
        };
    }
} dronePlugin;
