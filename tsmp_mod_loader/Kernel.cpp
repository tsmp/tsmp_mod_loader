#include "Common.h"
#include "lowlevel/Abstractions.h"

const char* FzLoaderSemaphoreName = "Local\\FREEZONE_STK_MOD_LOADER_SEMAPHORE";
const char* FzLoaderModulesMutexName = "Local\\FREEZONE_STK_MOD_LOADER_MODULES_MUTEX";
const string ModDirPrefix = ".svn\\";

extern void SetErrorHandler();

enum FZDllModFunResult : u32
{
	FZ_DLL_MOD_FUN_SUCCESS_LOCK = 0,    //Мод успешно загрузился, требуется залочить клиента по name_lock
	FZ_DLL_MOD_FUN_SUCCESS_NOLOCK = 1,  //Успех, лочить клиента (с использованием name_lock) пока не надо
	FZ_DLL_MOD_FUN_FAILURE = 2			//Ошибка загрузки мода
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
	//Убедимся, что нам разрешено выделить ресурсы
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

//Похоже, компиль не просекает, что FreeLibraryAndExitThread не возвращает управление. Из-за этого локальные переменные оказываются
//не зачищены, и это рушит нам приложение. Для решения вопроса делаем свой асмовый враппер, лишенный указанных недостатков.
u32 ThreadBody()
{
	__asm
	{
		call ThreadBodyInternal

		push[g_hDll]
		push eax

		//Хэндл ДЛЛ надо занулить до освобождения семафора, но саму ДЛЛ выгрузить уже в самом конце - поэтому он сохранен в стеке
		mov g_hDll, 0

		push[g_hFzLoaderSemaphore] // для вызова CloseHandle
		push 0
		push 1
		push[g_hFzLoaderSemaphore]
		mov[g_hFzLoaderSemaphore], 0
		call ReleaseSemaphore
		call CloseHandle

		pop eax //Результат работы
		pop ebx //сохраненный хэндл
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

	//Захватим ДЛЛ для предотвращения выгрузки во время работы потока загрузчика
	Msg("Path to loader is: %s", path);
	g_hDll = LoadLibrary(path);

	if (!g_hDll)
	{
		Msg("! Cannot acquire DLL %s", path);
		return false;
	}

	//Начинаем синхронизацию файлов мода в отдельном потоке
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

	//Отлично, основной поток закачки не стартует, пока мы не отпустим мьютекс
	if (InitAbstractions())
	{
		AbortConnection();

		if (ValidateInput(modName, modParams))
		{
			g_ModName = modName;
			g_ModParams = modParams;

			//Благодаря этому хаку с префиксом, игра не полезет подгружать файлы мода при запуске оригинального клиента
			g_ModRelPath = ModDirPrefix;
			g_ModRelPath += modName;

			if (RunModLoad())
			{
				// Не лочимся - загрузка может окончиться неудачно либо быть отменена
				// кроме того, повторный коннект при активной загрузке и выставленной инфе о моде приведет к неожиданным результатам
				result = FZ_DLL_MOD_FUN_SUCCESS_NOLOCK;
			}
		}

		//Основной поток закачки сам проинициализирует их заново - если не освобождать, происходит какая-то фигня при освобождении из другого потока.
		FreeAbstractions();
	}

	ReleaseMutex(mutex);
	CloseHandle(mutex);
	return result;
}

/*
 * Схема работы загрузчика с использованием с мастер-списка модов:
 * 
 * 1) Скачиваем мастер-список модов
 * 2) Если мастер-список скачан и мод с таким названием есть в списке - используем ссылки на движок и геймдату
 * из этого списка; если заданы кастомные и не совпадающие с теми, которые в списке - ругаемся и не работаем
 * 3) Если мастер-список скачан, но мода с таким названием в нем нет - убеждаемся, что ссылка на движок либо не
 * задана (используется текущий), либо есть среди других модов, либо на клиенте активен дебаг-режим. Если не
 * выполняется ничего из этого - ругаемся и не работаем, если выполнено - используем указанные ссылки
 * 4) Если мастер-список НЕ скачан - убеждаемся, что ссылка на движок либо не задана (надо использовать текущий),
 * либо активен дебаг-режим на клиенте. Если это не выполнено - ругаемся и не работаем, иначе используем
 * предоставленные пользователем ссылки.
 * 5) Скачиваем сначала геймдатный список, затем движковый (чтобы не дать возможность переопределить в первом файлы второго)
 * 6) Актуализируем файлы и рестартим клиент
 * 
 * 
 * Доступные ключи запуска, передающиеся в mod_params:
 * 
 * -binlist <URL> - ссылка на адрес, по которому берется список файлов движка (для работы требуется запуск клиента с ключлм -fz_custom_bin)
 * -gamelist <URL> - ссылка на адрес, по которому берется список файлов мода (геймдатных\патчей)
 * -srv <IP> - IP-адрес сервера, к которому необходимо присоединиться после запуска мода
 * -srvname <domainname> - (не реализовано) доменное имя, по которому располагается сервер. Можно использовать вместо параметра -srv в случае динамического IP сервера
 * -port <number> - порт сервера
 * -gamespymode - стараться использовать загрузку средствами GameSpy
 * -fullinstall - мод представляет собой самостоятельную копию игры, связь с файлами оригинальной не требуется
 * -sharedpatches - использовать общую с инсталляцией игры директорию патчей
 * -configsdir <string> - директория конфигов
 * -exename <string> - имя исполняемого файла мода
 * -includename - включить в строку запуска мода параметр /name= с именем игрока
 * -preservemessage - показывать окно с сообщением о загрузке мода
 * -nomirrors - запретить скачивание списков файлов мода с URL, отличающихся от указанных в ключах -binlist/-gamelist * 
 */

// Сюда передается управление из волшебного пакета sysmsgs
extern "C" __declspec(dllexport) u32 __stdcall ModLoad(const char* modName, const char* modParams)
{
	SetErrorHandler();
	const HANDLE semaphore = CreateSemaphore(nullptr, 1, 1, FzLoaderSemaphoreName);
	FZDllModFunResult result = FZ_DLL_MOD_FUN_FAILURE;

	if (!semaphore || semaphore == INVALID_HANDLE_VALUE)
		return result;

	if (WaitForSingleObject(semaphore, 0) == WAIT_OBJECT_0)
	{
		// Отлично, семафор наш. Сохраним хендл на него для последующего освобождения
		g_hFzLoaderSemaphore = semaphore;
		g_hDll = nullptr;

		result = ModLoadInternal(modName, modParams);

		// В случае успеха семафор будет разлочен в другом треде после окончания загрузки.
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
