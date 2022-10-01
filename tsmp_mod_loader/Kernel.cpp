#include "Common.h"
#include "lowlevel/Abstractions.h"

const char* FzLoaderSemaphoreName = "Local\\FREEZONE_STK_MOD_LOADER_SEMAPHORE";
const char* FzLoaderModulesMutexName = "Local\\FREEZONE_STK_MOD_LOADER_MODULES_MUTEX";
const string ModDirPrefix = ".svn\\";

enum class FZDllModFunResult : u32
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

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	g_hCurrentModule = hModule;
	return TRUE;
}

bool InitModules()
{
	InitAbstractions();
	Msg("- Abstractions Inited!");
	return true;
}

void FreeModules()
{
	Msg("- Free modules");
	FreeAbstractions();
}

bool ThreadBody_internal()
{
	//��������, ��� ��� ��������� �������� �������
	HANDLE mutex = CreateMutex(nullptr, FALSE, FzLoaderModulesMutexName);

	if (!mutex || mutex == INVALID_HANDLE_VALUE)
		return false;

	if (WaitForSingleObject(mutex, INFINITE) != WAIT_OBJECT_0)
	{
		CloseHandle(mutex);
		return false;
	}

	if (!InitModules())
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

		u32 i = 0;

		while (i < 10000 && !VersionAbstraction()->CheckForUserCancelDownload() && !VersionAbstraction()->CheckForLevelExist())
		{
			Sleep(1);
			i = i + 1;
		}

		VersionAbstraction()->StopVisualDownload();
	}

	Msg("- Releasing resources");
	VersionAbstraction()->ExecuteConsoleCommand("flush");

	//FreeStringMemory();
	FreeModules();

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
		call ThreadBody_internal

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
		g_hDll = 0;
		return false;
	}

	return true;
}

void AbortConnection()
{
	Msg("Aborting connection");
	VersionAbstraction()->AbortConnection();
}

bool ValidateInput(char* modName, char* modParams)
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

FZDllModFunResult ModLoad_internal(char* modName, char* modParams)
{
	FZDllModFunResult result = FZDllModFunResult::FZ_DLL_MOD_FUN_FAILURE;
	HANDLE mutex = CreateMutex(nullptr, FALSE, FzLoaderModulesMutexName);

	if (!mutex || mutex == INVALID_HANDLE_VALUE || WaitForSingleObject(mutex, 0) != WAIT_OBJECT_0)
		return result;

	//�������, �������� ����� ������� �� ��������, ���� �� �� �������� �������
	if (InitModules())
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
				result = FZDllModFunResult::FZ_DLL_MOD_FUN_SUCCESS_NOLOCK;
			}
		}

		//�������� ����� ������� ��� ����������������� �� ������ - ���� �� �����������, ���������� �����-�� ����� ��� ������������ �� ������� ������.
		FreeModules();
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
 * -srvname <domainname> - �������� ���, �� �������� ������������� ������. ����� ������������ ������ ��������� -srv � ������ ������������� IP �������
 * -port <number> - ���� �������
 * -gamespymode - ��������� ������������ �������� ���������� GameSpy
 * -fullinstall - ��� ������������ ����� ��������������� ����� ����, ����� � ������� ������������ �� ���������
 * -sharedpatches - ������������ ����� � ������������ ���� ���������� ������
 * -logsev <number> - ������� ����������� ���������� ���������, �� ��������� FZ_LOG_ERROR
 * -configsdir <string> - ���������� ��������
 * -exename <string> - ��� ������������ ����� ����
 * -includename - �������� � ������ ������� ���� �������� /name= � ������ ������
 * -preservemessage - ���������� ���� � ���������� � �������� ����
 * -nomirrors - ��������� ���������� ������� ������ ���� � URL, ������������ �� ��������� � ������ -binlist/-gamelist * 
 */

// ���� ���������� ���������� �� ���������� ������ sysmsgs
extern "C" __declspec(dllexport) u32 __stdcall ModLoad(char* modName, char* modParams)
{
	HANDLE semaphore = CreateSemaphore(nullptr, 1, 1, FzLoaderSemaphoreName);
	FZDllModFunResult result = FZDllModFunResult::FZ_DLL_MOD_FUN_FAILURE;

	if (!semaphore || semaphore == INVALID_HANDLE_VALUE)
		return static_cast<u32>(result);

	if (WaitForSingleObject(semaphore, 0) == WAIT_OBJECT_0)
	{
		// �������, ������� ���. �������� ����� �� ���� ��� ������������ ������������
		g_hFzLoaderSemaphore = semaphore;
		g_hDll = 0;

		result = ModLoad_internal(modName, modParams);

		// � ������ ������ ������� ����� �������� � ������ ����� ����� ��������� ��������.
		if (result == FZDllModFunResult::FZ_DLL_MOD_FUN_FAILURE && g_hDll)
		{
			FreeLibrary(g_hDll);
			g_hDll = 0;
		}

		g_hFzLoaderSemaphore = INVALID_HANDLE_VALUE;
		ReleaseSemaphore(semaphore, 1, nullptr);
		CloseHandle(semaphore);
	}
	else
		CloseHandle(semaphore); // �� �������, �������������.

	return static_cast<u32>(result);
}
