#include "Common.h"
#include "lowlevel/Abstractions.h"

const char* FzLoaderSemaphoreName = "Local\\FREEZONE_STK_MOD_LOADER_SEMAPHORE";
const char* FzLoaderModulesMutexName = "Local\\FREEZONE_STK_MOD_LOADER_MODULES_MUTEX";
const string ModDirPrefix = ".svn\\";

extern void SetErrorHandler();

enum FZDllModFunResult : u32
{
	FZ_DLL_MOD_FUN_SUCCESS_LOCK = 0,    //��� ������� ����������, ��������� �������� ������� �� name_lock
	FZ_DLL_MOD_FUN_SUCCESS_NOLOCK = 1,  //�����, ������ ������� (� �������������� name_lock) ���� �� ����
	FZ_DLL_MOD_FUN_FAILURE = 2			//������ �������� ����
};

extern bool DoWork(const string &modName, const string &modPath);

HINSTANCE g_hDll;
HANDLE g_hFzLoaderSemaphore;
HMODULE g_hCurrentModule;

string g_ModRelPath;
string g_ModName;
string g_ModParams;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID pvReserved)
{
	g_hCurrentModule = hModule;
	return TRUE;
}

bool ThreadBodyInternal()
{
	//��������, ��� ��� ��������� �������� �������
	const HANDLE mutex = CreateMutex(nullptr, FALSE, FzLoaderModulesMutexName);

	if (!mutex || mutex == INVALID_HANDLE_VALUE)
		return false;

	if (WaitForSingleObject(mutex, INFINITE) != WAIT_OBJECT_0)
	{
		CloseHandle(mutex);
		return false;
	}

	if (!InitAbstractions())
	{
		ReleaseMutex(mutex);
		CloseHandle(mutex);
		return false;
	}

	Msg("- FreeZone Mod Loader TSMP v2");
	Msg("- Build date: %s", __DATE__);
	Msg("- Mod name is %s", g_ModName.c_str());
	Msg("- Mod params %s", g_ModParams.c_str());
	Msg("Working thread started");

	if (!DoWork(g_ModName, g_ModRelPath))
	{
		Msg("! Loading failed!");
		VersionAbstraction()->SetVisualProgress(0);

		if (VersionAbstraction()->IsMessageActive())
			VersionAbstraction()->TriggerMessage();

		VersionAbstraction()->AssignStatus("Downloading failed.Try again.");

		for (u32 i = 0; i < 10000; i++)
		{
			if (VersionAbstraction()->CheckForUserCancelDownload() || VersionAbstraction()->CheckForLevelExist())
				break;

			Sleep(1);
		}

		VersionAbstraction()->StopVisualDownload();
	}

	Msg("- Releasing resources");
	VersionAbstraction()->ExecuteConsoleCommand("flush");

	FreeAbstractions();
	ReleaseMutex(mutex);
	CloseHandle(mutex);
	return true;
}

//������, ������� �� ���������, ��� FreeLibraryAndExitThread �� ���������� ����������. ��-�� ����� ��������� ���������� �����������
//�� ��������, � ��� ����� ��� ����������. ��� ������� ������� ������ ���� ������� �������, �������� ��������� �����������.
u32 ThreadBody()
{
	__asm
	{
		call ThreadBodyInternal

		push[g_hDll]
		push eax

		//����� ��� ���� �������� �� ������������ ��������, �� ���� ��� ��������� ��� � ����� ����� - ������� �� �������� � �����
		mov g_hDll, 0

		push[g_hFzLoaderSemaphore] // ��� ������ CloseHandle
		push 0
		push 1
		push[g_hFzLoaderSemaphore]
		mov[g_hFzLoaderSemaphore], 0
		call ReleaseSemaphore
		call CloseHandle

		pop eax //��������� ������
		pop ebx //����������� �����
		cmp al, 0
		je error_happen
		push 0
		call GetCurrentProcess
		push eax
		call TerminateProcess

	error_happen :
		push 0
		push ebx
		call FreeLibraryAndExitThread
	}
}

bool RunModLoad()
{
	char path[MAX_PATH];
	GetModuleFileName(g_hCurrentModule, path, MAX_PATH);

	//�������� ��� ��� �������������� �������� �� ����� ������ ������ ����������
	Msg("Path to loader is: %s", path);
	g_hDll = LoadLibrary(path);

	if (!g_hDll)
	{
		Msg("! Cannot acquire DLL %s", path);
		return false;
	}

	//�������� ������������� ������ ���� � ��������� ������
	Msg("Starting working thread");

	if (!VersionAbstraction()->ThreadSpawn(ThreadBody, nullptr))
	{
		Msg("! Cannot start thread");
		FreeLibrary(g_hDll);
		g_hDll = nullptr;
		return false;
	}

	return true;
}

void AbortConnection()
{
	Msg("Aborting connection");
	VersionAbstraction()->AbortConnection();
}

bool ValidateInput(const char* modName, const char* modParams)
{
	const u32 MaxNameSize = 4096;
	const u32 MaxParamsSize = 4096;
	const string AllowedSymbolsInModName = "1234567890abcdefghijklmnopqrstuvwxyz_";
	const u32 modStrLen = strnlen_s(modName, MaxNameSize);

	if (modStrLen + ModDirPrefix.size() > MaxNameSize)
	{
		Msg("! Too long mod name, exiting");
		return false;
	}

	if (strnlen_s(modParams, MaxParamsSize) >= MaxParamsSize)
	{
		Msg("! Too long mod params, exiting");
		return false;
	}

	for (u32 i = 0; i < modStrLen; i++)
	{
		if (AllowedSymbolsInModName.find(modName[i]) == string::npos)
		{
			Msg("! Invalid mod name, exiting");
			return false;
		}
	}

	return true;
}

FZDllModFunResult ModLoadInternal(const char* modName, const char* modParams)
{
	FZDllModFunResult result = FZ_DLL_MOD_FUN_FAILURE;
	const HANDLE mutex = CreateMutex(nullptr, FALSE, FzLoaderModulesMutexName);

	if (!mutex || mutex == INVALID_HANDLE_VALUE || WaitForSingleObject(mutex, 0) != WAIT_OBJECT_0)
		return result;

	//�������, �������� ����� ������� �� ��������, ���� �� �� �������� �������
	if (InitAbstractions())
	{
		AbortConnection();

		if (ValidateInput(modName, modParams))
		{
			g_ModName = modName;
			g_ModParams = modParams;

			//��������� ����� ���� � ���������, ���� �� ������� ���������� ����� ���� ��� ������� ������������� �������
			g_ModRelPath = ModDirPrefix;
			g_ModRelPath += modName;

			if (RunModLoad())
			{
				// �� ������� - �������� ����� ���������� �������� ���� ���� ��������
				// ����� ����, ��������� ������� ��� �������� �������� � ������������ ���� � ���� �������� � ����������� �����������
				result = FZ_DLL_MOD_FUN_SUCCESS_NOLOCK;
			}
		}

		//�������� ����� ������� ��� ����������������� �� ������ - ���� �� �����������, ���������� �����-�� ����� ��� ������������ �� ������� ������.
		FreeAbstractions();
	}

	ReleaseMutex(mutex);
	CloseHandle(mutex);
	return result;
}

/*
 * ����� ������ ���������� � �������������� � ������-������ �����:
 * 
 * 1) ��������� ������-������ �����
 * 2) ���� ������-������ ������ � ��� � ����� ��������� ���� � ������ - ���������� ������ �� ������ � ��������
 * �� ����� ������; ���� ������ ��������� � �� ����������� � ����, ������� � ������ - �������� � �� ��������
 * 3) ���� ������-������ ������, �� ���� � ����� ��������� � ��� ��� - ����������, ��� ������ �� ������ ���� ��
 * ������ (������������ �������), ���� ���� ����� ������ �����, ���� �� ������� ������� �����-�����. ���� ��
 * ����������� ������ �� ����� - �������� � �� ��������, ���� ��������� - ���������� ��������� ������
 * 4) ���� ������-������ �� ������ - ����������, ��� ������ �� ������ ���� �� ������ (���� ������������ �������),
 * ���� ������� �����-����� �� �������. ���� ��� �� ��������� - �������� � �� ��������, ����� ����������
 * ��������������� ������������� ������.
 * 5) ��������� ������� ���������� ������, ����� ��������� (����� �� ���� ����������� �������������� � ������ ����� �������)
 * 6) ������������� ����� � ��������� ������
 * 
 * 
 * ��������� ����� �������, ������������ � mod_params:
 * 
 * -binlist <URL> - ������ �� �����, �� �������� ������� ������ ������ ������ (��� ������ ��������� ������ ������� � ������ -fz_custom_bin)
 * -gamelist <URL> - ������ �� �����, �� �������� ������� ������ ������ ���� (����������\������)
 * -srv <IP> - IP-����� �������, � �������� ���������� �������������� ����� ������� ����
 * -srvname <domainname> - (�� �����������) �������� ���, �� �������� ������������� ������. ����� ������������ ������ ��������� -srv � ������ ������������� IP �������
 * -port <number> - ���� �������
 * -gamespymode - ��������� ������������ �������� ���������� GameSpy
 * -fullinstall - ��� ������������ ����� ��������������� ����� ����, ����� � ������� ������������ �� ���������
 * -sharedpatches - ������������ ����� � ������������ ���� ���������� ������
 * -configsdir <string> - ���������� ��������
 * -exename <string> - ��� ������������ ����� ����
 * -includename - �������� � ������ ������� ���� �������� /name= � ������ ������
 * -preservemessage - ���������� ���� � ���������� � �������� ����
 * -nomirrors - ��������� ���������� ������� ������ ���� � URL, ������������ �� ��������� � ������ -binlist/-gamelist * 
 */

// ���� ���������� ���������� �� ���������� ������ sysmsgs
extern "C" __declspec(dllexport) u32 __stdcall ModLoad(const char* modName, const char* modParams)
{
	SetErrorHandler();
	const HANDLE semaphore = CreateSemaphore(nullptr, 1, 1, FzLoaderSemaphoreName);
	FZDllModFunResult result = FZ_DLL_MOD_FUN_FAILURE;

	if (!semaphore || semaphore == INVALID_HANDLE_VALUE)
		return result;

	if (WaitForSingleObject(semaphore, 0) == WAIT_OBJECT_0)
	{
		// �������, ������� ���. �������� ����� �� ���� ��� ������������ ������������
		g_hFzLoaderSemaphore = semaphore;
		g_hDll = nullptr;

		result = ModLoadInternal(modName, modParams);

		// � ������ ������ ������� ����� �������� � ������ ����� ����� ��������� ��������.
		if (result == FZ_DLL_MOD_FUN_FAILURE && g_hDll)
		{
			FreeLibrary(g_hDll);
			g_hDll = nullptr;
		}

		g_hFzLoaderSemaphore = INVALID_HANDLE_VALUE;
		ReleaseSemaphore(semaphore, 1, nullptr);
	}

	CloseHandle(semaphore);
	return result;
}
