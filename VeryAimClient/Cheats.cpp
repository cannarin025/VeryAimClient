#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#include "Offsets.h"

//getting display resolution
const int screen_width = GetSystemMetrics(SM_CXSCREEN);
const int screen_height = GetSystemMetrics(SM_CYSCREEN);
const int xhairx = screen_width / 2;
const int xhairy = screen_height / 2;

//Declaring global variables
HWND hwnd;
DWORD procID;
HANDLE procH;
uintptr_t moduleBase;

//function to get base address of module
uintptr_t GetModuleBaseAddress(const char* modName) {
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procID);
	if (hSnap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry)) {
			do {
				if (!strcmp(modEntry.szModule, modName)) {
					CloseHandle(hSnap);
					return (uintptr_t)modEntry.modBaseAddr;
				}
			} while (Module32Next(hSnap, &modEntry));
			
		}
	}
}

template<typename T> T RPM(SIZE_T address) {
	T buffer;
	ReadProcessMemory(procH, (LPCVOID)address, &buffer, sizeof(T), NULL);
	return buffer;
}

template<typename T> void WPM(SIZE_T address, T buffer) {
	WriteProcessMemory(procH, (LPVOID)address, &buffer, sizeof(buffer), NULL);
}

int main() {
	//setup
	hwnd = FindWindowA(NULL, "Counter-Strike: Global Offensive");
	GetWindowThreadProcessId(hwnd, &procID);
	moduleBase = GetModuleBaseAddress("client.dll");
	procH = OpenProcess(PROCESS_ALL_ACCESS, NULL, procID);

	//hack configuration
	bool aimbot = true;
	bool triggerbot = true;
	bool radar = true;
	bool esp = true;
	bool bHop = true;

	while (!GetAsyncKeyState(VK_END)) {
		uintptr_t localPlayer = RPM<uintptr_t>(moduleBase + dwLocalPlayer);
		int localTeam = RPM<int>(localPlayer + m_iTeamNum);
		if (bHop) {
			uintptr_t buffer;
			int flags = RPM<int>(localPlayer + m_fFlags);
			if (flags & 1) {
				buffer = 5;
			}
			else {
				buffer = 4;
			}

			if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
				WPM(moduleBase + dwForceJump, buffer);
			}
		}
		if (aimbot || triggerbot || radar || esp) {
			for (int i = 1; i < 64; i++) {
				DWORD currentEntity = RPM<DWORD>(moduleBase + dwEntityList + i * 0x10);
				int hp = RPM<int>(currentEntity + m_iHealth);
				int dormant = RPM<int>(currentEntity + m_bDormant);
				//check to ensure entity is alive and not dormant
				if (hp < 1 || hp > 100) continue;
				if (dormant) continue;

				int team = RPM<int>(currentEntity + m_iTeamNum);
				int iGlowIndex = RPM<int>(currentEntity + m_iGlowIndex);
				if (radar) {
					if (currentEntity) {
						WPM<bool>(currentEntity + m_bSpotted, true);
					}
					//std::cout << "spotted! \n";
				}
			}
		}
	}
}