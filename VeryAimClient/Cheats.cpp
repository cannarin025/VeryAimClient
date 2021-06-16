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

//Utils
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

struct glowStructEnemy {
	float red = 1.f;
	float green = 0.f;
	float blue = 0.f;
	float alpha = 1.f;
	uint8_t padding[8];
	float unknown = 1.f;
	uint8_t padding2[4];
	BYTE renderOccluded = true;
	BYTE renderUnocclude = false;
	BYTE fullBloom = false;
} enemyGlow;

struct glowStructAlly {
	float red = 0.f;
	float green = 1.f;
	float blue = 0.f;
	float alpha = 1.f;
	uint8_t padding[8];
	float unknown = 1.f;
	uint8_t padding2[4];
	BYTE renderOccluded = true;
	BYTE renderUnocclude = false;
	BYTE fullBloom = false;
} allyGlow;

struct view_matrix_t {
	float matrix[16];
} playerView;

class Vector3 {
public:
	float x, y, z;
	Vector3() : x(0.f), y(0.f), z(0.f) {}
	Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

Vector3 entityLocation(uintptr_t entity) { //Stores XYZ coordinates in a Vector3.
	return RPM<Vector3>(entity + m_vecOrigin);
}

Vector3 GetHead(uintptr_t player) {
	struct boneMatrix_t {
		byte pad3[12];
		float x;
		byte pad1[12];
		float y;
		byte pad2[12];
		float z;
	};
	uintptr_t boneBase = RPM<uintptr_t>(player + m_dwBoneMatrix);
	boneMatrix_t boneMatrix = RPM<boneMatrix_t>(boneBase + (sizeof(boneMatrix) * 8 /*8 is the boneid for head*/));
	return Vector3(boneMatrix.x, boneMatrix.y, boneMatrix.z);
}

struct Vector3 WorldToScreen(const struct Vector3 pos, struct view_matrix_t matrix) { //This turns 3D coordinates (ex: XYZ) int 2D coordinates (ex: XY).
	struct Vector3 out;
	float _x = matrix.matrix[0] * pos.x + matrix.matrix[1] * pos.y + matrix.matrix[2] * pos.z + matrix.matrix[3];
	float _y = matrix.matrix[4] * pos.x + matrix.matrix[5] * pos.y + matrix.matrix[6] * pos.z + matrix.matrix[7];
	out.z = matrix.matrix[12] * pos.x + matrix.matrix[13] * pos.y + matrix.matrix[14] * pos.z + matrix.matrix[15];

	_x *= 1.f / out.z;
	_y *= 1.f / out.z;

	out.x = screen_width * .5f;
	out.y = screen_height * .5f;

	out.x += 0.5f * _x * screen_width + 0.5f;
	out.y -= 0.5f * _y * screen_height + 0.5f;

	return out;
}

int getTeam(uintptr_t entity) {
	return RPM<int>(entity + m_iTeamNum);
}

uintptr_t GetPlayer() {
	return RPM< uintptr_t>(moduleBase + dwLocalPlayer);
}

uintptr_t GetEntity(int index) {  //Each player has an index. 1-64
	return RPM< uintptr_t>(moduleBase + dwEntityList + index * 0x10); //We multiply the index by 0x10 to select the player we want in the entity list.
}

int GetHealth(uintptr_t entity) {
	return RPM<int>(entity + m_iHealth);
}

bool DormantCheck(uintptr_t player) {
	return RPM<int>(player + m_bDormant);
}

float GetScreenDistance(int x1, int y1, int x2, int y2) {
	return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

int FindClosestEnemy() {
	float finish;
	int closestEntityIndex = 1;
	Vector3 calc = { 0, 0, 0 };
	float closest = FLT_MAX;
	int localTeam = getTeam(GetPlayer());
	for (int i = 1; i < 64; i++) { //Loops through all the entitys in the index 1-64.
		DWORD entity = GetEntity(i);
		int enmTeam = getTeam(entity); if (enmTeam == localTeam) continue;
		int enmHealth = GetHealth(entity); if (enmHealth < 1 || enmHealth > 100) continue;
		int dormant = DormantCheck(entity); if (dormant) continue;
		Vector3 headBone = WorldToScreen(GetHead(entity), playerView);
		finish = GetScreenDistance(headBone.x, headBone.y, xhairx, xhairy);
		if (finish < closest) {
			closest = finish;
			closestEntityIndex = i;
		}
	}
	DWORD closestEntity = GetEntity(closestEntityIndex);
	int closestHp = GetHealth(closestEntity);
	int isDormant = DormantCheck(closestEntity);
	if (isDormant || (closestHp < 1 || closestHp > 100)) {
		closestEntityIndex = NULL;
	}
	return closestEntityIndex;
}

HDC hdc;
void DrawLine(float StartX, float StartY, float EndX, float EndY) { //This function is optional for debugging.
	int a, b = 0;
	HPEN hOPen;
	HPEN hNPen = CreatePen(PS_SOLID, 2, 0x0000FF /*red*/);
	hOPen = (HPEN)SelectObject(hdc, hNPen);
	MoveToEx(hdc, StartX, StartY, NULL); //start of line
	a = LineTo(hdc, EndX, EndY); //end of line
	DeleteObject(SelectObject(hdc, hOPen));
}

//find enemy thread setup
int closestEnemyIndex;
void FindClosestEnemyThread() {
	while (1) {
		closestEnemyIndex = FindClosestEnemy();
	}
}



int main() {
	//setup
	hwnd = FindWindowA(NULL, "Counter-Strike: Global Offensive");
	GetWindowThreadProcessId(hwnd, &procID);
	moduleBase = GetModuleBaseAddress("client.dll");
	procH = OpenProcess(PROCESS_ALL_ACCESS, NULL, procID);
	hdc = GetDC(hwnd);

	//hack configuration
	bool aimbot = true;
	bool triggerbot = true;
	bool radar = true;
	bool esp = true;
	bool bHop = true;

	//creates thread to find closest enemy to player
	if (aimbot) {
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)FindClosestEnemyThread, NULL, NULL, NULL);
	}

	while (!GetAsyncKeyState(VK_END)) {
		uintptr_t glowManager = RPM<uintptr_t>(moduleBase + dwGlowObjectManager);
		
		//player vars
		uintptr_t player = RPM<uintptr_t>(moduleBase + dwLocalPlayer);
		int playerTeam = RPM<int>(player + m_iTeamNum);
		playerView = RPM<view_matrix_t>(moduleBase + dwViewMatrix);
		int playerHP = RPM<int>(player + m_iHealth); //godmode (not working)
		int playerArmour = RPM<int>(player + m_ArmorValue);


		if (playerHP < 10000 or playerArmour < 10000) {
			WPM<int>(player + m_iHealth, 10000);
			WPM<int>(player + m_ArmorValue, 10000);
		}

		if (bHop) {
			uintptr_t buffer;
			int flags = RPM<int>(player + m_fFlags);
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
				int entityHp = RPM<int>(currentEntity + m_iHealth);
				int dormant = RPM<int>(currentEntity + m_bDormant);
				//check to ensure entity is alive and not dormant
				if (entityHp < 1 || entityHp > 100) continue;
				if (dormant) continue;

				int entityTeam = RPM<int>(currentEntity + m_iTeamNum);

				if (esp) {
					int glowIndex = RPM<int>(currentEntity + m_iGlowIndex);
					if (entityTeam == playerTeam) {
						WPM<glowStructAlly>(glowManager + (glowIndex * 0x38) + 0x4, allyGlow);
					}
					else if (entityTeam != playerTeam) {
						WPM <glowStructEnemy>(glowManager + (glowIndex * 0x38) + 0x4, enemyGlow);
					}
				}
	
				if (radar) {
					if (currentEntity) {
						WPM<bool>(currentEntity + m_bSpotted, true);
					}
					//std::cout << "spotted! \n";
				}

				if (aimbot) {
					if (closestEnemyIndex != NULL) {
						Vector3 closestw2shead = WorldToScreen(GetHead(GetEntity(closestEnemyIndex)), playerView);
						//DrawLine(xhairx, xhairy, closestw2shead.x, closestw2shead.y); //optinal for debugging

						if (GetAsyncKeyState(VK_MENU /*alt key*/) && closestw2shead.z >= 0.001f /*onscreen check*/)
							SetCursorPos(closestw2shead.x, closestw2shead.y); //turn off "raw input" in CSGO settings
					}
				}
			}
		}
	}
}