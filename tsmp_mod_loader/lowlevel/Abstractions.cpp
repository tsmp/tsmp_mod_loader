#include "..\Common.h"
#include "Abstractions.h"

// TODO: implement FZUnknownGameVersion and maybe saving log to another file

enum class FZ_GAME_VERSION
{
	FZ_VER_UNKNOWN, FZ_VER_SOC_10006, FZ_VER_SOC_10006_V2
};

enum EErrorDlg
{
	ErrInvalidPassword,
	ErrInvalidHost,
	ErrSessionFull,
	ErrServerReject,
	ErrCDKeyInUse,
	ErrCDKeyDisabled,
	ErrCDKeyInvalid,
	ErrDifferentVersion,
	ErrGSServiceFailed,
	ErrMasterServerConnectFailed,
	NoNewPatch,
	NewPatchFound,
	PatchDownloadError,
	PatchDownloadSuccess,
	ConnectToMasterServer,
	SessionTerminate,
	LoadingError
};

typedef char string_path[2 * _MAX_PATH];
typedef char string64[64];
typedef char string512[512];

struct xrCoreData
{
	string64 ApplicationName;
	string_path ApplicationPath;
	string_path WorkingPath;
	string64 UserName;
	string64 CompName;
	string512 Params;
};

using pxrCoreData = xrCoreData*;

class FZBaseGameVersion : public FZAbstractGameVersion
{
protected:
	string m_ExeModuleName;
	string m_XrGameModueName;
	string m_XrCoreModuleName;

	HMODULE m_ExeModuleAddress;
	HMODULE m_XrGameModuleAddress;
	HMODULE m_XrCoreModuleAddress;

	void* m_pGameLevel;
	u32* m_pConsole;
	u32* m_pXrFS;
	pxrCoreData m_pCore;

	void* m_pCConsoleExecute;
	void* m_pCConsoleGetString;

	void* m_pCLocatorApiUpdatePath;
	void* m_pCLocatorApiPathExists;

	using LogFuncPtr = void(__cdecl* )(const char* text);
	LogFuncPtr m_pLogFunction;
		
	//string _log_file_name;

	static void* FunFromVTable(void* obj, u32 index);
	static void* DoEcxCall_noarg(void* fun, void* obj);

	static void* DoEcxCall_1arg(void* fun, void* obj, const void* arg);
	static void* DoEcxCall_2arg(void* fun, void* obj, const void* arg1, const void* arg2);
	static void* DoEcxCall_3arg(void* fun, void* obj, const void* arg1, const void* arg2, const void* arg3);

	void* GetLevel() const;

public:
	FZBaseGameVersion();
	~FZBaseGameVersion() override = default;

	string GetCoreParams() override;
	string GetCoreApplicationPath() override;

	void ExecuteConsoleCommand(const string &cmd) override;

	string GetEngineExeFileName() override;
	void* GetEngineExeModuleAddress() override;

	string UpdatePath(const string &root, const string &appendix) override;
	bool PathExists(const string &root) override;
	bool CheckForLevelExist() override;
	string GetPlayerName() override;
	void Log(const string &txt) override;
};


//{ FZUnknownGameVersion }
//
//FZUnknownGameVersion = class(FZBaseGameVersion)
//public
//void ShowMpMainMenu(); override;
//void AssignStatus(str:PAnsiChar); override;
//function CheckForUserCancelDownload() :boolean; override;
//function StartVisualDownload() :boolean; override;
//function StopVisualDownload() :boolean; override;
//void SetVisualProgress({ % H - }progress:single); override;
//function ThreadSpawn(proc:uintptr; args:uintptr; {% H - }name:PAnsiChar = nil; {% H - }stack:cardinal = 0) :boolean; override;
//void AbortConnection(); override;
//function IsServerListUpdateActive() :boolean; override;
//function IsMessageActive() :boolean; override;
//void TriggerMessage(); override;
//void PrepareForMessageShowing(); override;
//void ResetMasterServerError(); override;
//end;
//
//{ FZCommonGameVersion }

class FZCommonGameVersion : public FZBaseGameVersion
{
protected:
	u32 m_pGamePersistent;	
	u32 m_pDevice;
	volatile int *m_pbRendering;

	void* m_pXrCriticalSectionEnter;
	void* m_pXrCriticalSectionLeave;
	void* m_pStrContainerDock;
	u32 m_pStringContainer;

	using ThreadSpawnFuncPtr = void(__cdecl*)(void* function, const char* name, u32 stackSize, void* args);
	ThreadSpawnFuncPtr m_pThreadSpawnFunc;

	// НЕ ТРОГАТЬ! ОПАСНО ДЛЯ ЖИЗНИ!
	void SafeExec_start();
	void SafeExec_end();

	void assign_string(u32 pshared_str, const string &text);

	u32 GetMainMenuPtr();
	u32 PatchDownloadIsInProgressPtr();
	virtual void ActivateMainMenu(bool state);

	virtual u32 get_CMainMenu_castto_IMainMenu_offset() = 0;

	virtual u32 get_IGamePersistent__m_pMainMenu_offset() = 0;
	virtual u32 get_CMainMenu__m_startDialog_offset() = 0;
	virtual u32 get_CMainMenu__m_sPDProgress__FileName_offset() = 0;
	virtual u32 get_CMainMenu__m_sPDProgress__Status_offset() = 0;
	virtual u32 get_CMainMenu__m_sPDProgress__IsInProgress_offset() = 0;
	virtual u32 get_CMainMenu__m_sPDProgress__Progress_offset() = 0;
	virtual u32 get_CMainMenu__m_pGameSpyFull_offset() = 0;
	virtual u32 get_CMainMenu__m_NeedErrDialog_offset() = 0;
	virtual u32 get_CMainMenu__ConnectToMasterServer_dlg_id() = 0;
	virtual u32 get_CMainMenu__Message_dlg_id() = 0;
	virtual u32 get_CGameSpy_Full__m_pGS_HTTP_offset() = 0;
	virtual u32 get_CGameSpy_Full__m_pGS_SB_offset() = 0;
	virtual u32 get_CGameSpy_HTTP__m_LastRequest_offset() = 0;
	virtual u32 get_CGameSpy_Browser__m_bTryingToConnectToMasterServer_offset() = 0;
	virtual u32 get_CGameSpy_Browser__m_bAbleToConnectToMasterServer_offset() = 0;

	virtual u32 get_CMainMenu__m_pMB_ErrDlgs_first_element_ptr_offset() = 0;
	virtual u32 get_CUIMessageBoxEx_to_CUISimpleWindow_m_bShowMe_offset() = 0;

	virtual u32 get_CRenderDevice__mt_bMustExit_offset() = 0;
	virtual u32 get_CRenderDevice__mt_csEnter_offset() = 0;
	virtual u32 get_CRenderDevice__b_is_Active_offset() = 0;

	virtual u32 get_CLevel__m_bConnectResult_offset() = 0;
	virtual u32 get_CLevel__m_bConnectResultReceived_offset() = 0;
	virtual u32 get_CLevel__m_connect_server_err_offset() = 0;

	virtual u32 get_shared_str__p_offset() = 0;
	virtual u32 get_str_value__dwReference_offset() = 0;
	virtual u32 get_str_value__value_offset() = 0;

	virtual u32 get_SecondaryThreadProcAddress() = 0;
	virtual char* get_SecondaryThreadProcName() = 0;

	virtual u32 virtual_IMainMenu__Activate_index() = 0;
	virtual u32 virtual_CUIDialogWnd__Dispatch_index() = 0;

	void SetActiveErrorDlg(u32 dlg);
	u32 GetNeedErrorDlg();
public:

	FZCommonGameVersion();

	void ShowMpMainMenu() override;
	void AssignStatus(const string &str) override;

	bool CheckForUserCancelDownload() override;

	bool StartVisualDownload() override;
	bool StopVisualDownload() override;

	void SetVisualProgress(float progress) override;

	bool ThreadSpawn(void* proc, void* args, const string &name = "", u32 stack = 0) override;
	void AbortConnection() override;

	bool IsServerListUpdateActive() override;
	bool IsMessageActive() override;

	void TriggerMessage() override;
	void PrepareForMessageShowing() override;
	void ResetMasterServerError() override;
};

class FZGameVersion10006 : public FZCommonGameVersion
{
protected:
	u32 get_CMainMenu_castto_IMainMenu_offset() override;

	u32 get_IGamePersistent__m_pMainMenu_offset() override;
	u32 get_CMainMenu__m_startDialog_offset() override;
	u32 get_CMainMenu__m_sPDProgress__FileName_offset() override;
	u32 get_CMainMenu__m_sPDProgress__Status_offset() override;
	u32 get_CMainMenu__m_sPDProgress__IsInProgress_offset() override;
	u32 get_CMainMenu__m_sPDProgress__Progress_offset() override;
	u32 get_CMainMenu__m_pGameSpyFull_offset() override;
	u32 get_CMainMenu__m_NeedErrDialog_offset() override;
	u32 get_CMainMenu__ConnectToMasterServer_dlg_id() override;
	u32 get_CMainMenu__Message_dlg_id() override;

	u32 get_CGameSpy_Full__m_pGS_HTTP_offset() override;
	u32 get_CGameSpy_Full__m_pGS_SB_offset() override;
	u32 get_CGameSpy_HTTP__m_LastRequest_offset() override;
	u32 get_CGameSpy_Browser__m_bTryingToConnectToMasterServer_offset() override;
	u32 get_CGameSpy_Browser__m_bAbleToConnectToMasterServer_offset() override;

	u32 get_CMainMenu__m_pMB_ErrDlgs_first_element_ptr_offset() override;
	u32 get_CUIMessageBoxEx_to_CUISimpleWindow_m_bShowMe_offset() override;

	u32 get_CRenderDevice__mt_bMustExit_offset() override;
	u32 get_CRenderDevice__mt_csEnter_offset() override;
	u32 get_CRenderDevice__b_is_Active_offset() override;

	u32 get_CLevel__m_bConnectResult_offset() override;
	u32 get_CLevel__m_bConnectResultReceived_offset() override;
	u32 get_CLevel__m_connect_server_err_offset() override;

	u32 get_shared_str__p_offset() override;
	u32 get_str_value__dwReference_offset() override;
	u32 get_str_value__value_offset() override;

	u32 get_SecondaryThreadProcAddress() override;
	char* get_SecondaryThreadProcName() override;
	
	u32 virtual_IMainMenu__Activate_index() override;
	u32 virtual_CUIDialogWnd__Dispatch_index() override;

public:
	FZGameVersion10006();
};

class FZGameVersion10006_v2 : public FZGameVersion10006
{
protected:
	u32 get_SecondaryThreadProcAddress() override;
	char* get_SecondaryThreadProcName() override;
};

class FZGameVersionCreator
{
	static FZ_GAME_VERSION GetGameVersion();

public:
	static FZAbstractGameVersion* DetermineGameVersion();
};

// implementation

u32 AtomicExchange(u32* addr, u32 val)
{
	return InterlockedExchange(addr, val); // TODO: check
}

string CConsoleGetStringFake(const string &cmd) { return "Unknown"; }

FZBaseGameVersion::FZBaseGameVersion()
{
	m_ExeModuleName = "xr_3DA.exe";
	m_ExeModuleAddress = GetModuleHandle(m_ExeModuleName.c_str());

	m_XrGameModueName = "xrGame";
	m_XrGameModuleAddress = GetModuleHandle(m_XrGameModueName.c_str());

	m_XrCoreModuleName = "xrCore";
	m_XrCoreModuleAddress = GetModuleHandle(m_XrCoreModuleName.c_str());

	uniassert(m_ExeModuleAddress, "xr_3DA is 0");
	uniassert(m_XrGameModuleAddress, "xrGame is 0");
	uniassert(m_XrCoreModuleAddress, "xrCore is 0");

	m_pGameLevel = GetProcAddress(m_ExeModuleAddress, "?g_pGameLevel@@3PAVIGame_Level@@A");
	uniassert(m_pGameLevel, "g_ppGameLevel is 0");

	m_pConsole = reinterpret_cast<u32*>(GetProcAddress(m_ExeModuleAddress, "?Console@@3PAVCConsole@@A"));
	uniassert(m_pConsole, "console is 0");

	m_pCore = reinterpret_cast<pxrCoreData>(GetProcAddress(m_XrCoreModuleAddress, "?Core@@3VxrCore@@A"));
	uniassert(m_pCore, "core is 0");

	m_pXrFS = reinterpret_cast<u32*>(GetProcAddress(m_XrCoreModuleAddress, "?xr_FS@@3PAVCLocatorAPI@@A"));
	uniassert(m_pXrFS, "fs is 0");

	m_pCConsoleExecute = GetProcAddress(m_ExeModuleAddress, "?Execute@CConsole@@QAEXPBD@Z");
	uniassert(m_pCConsoleExecute, "CConsole::Execute is 0");

	m_pCLocatorApiUpdatePath = GetProcAddress(m_XrCoreModuleAddress, "?update_path@CLocatorAPI@@QAEPBDAAY0CAI@DPBD1@Z");
	uniassert(m_pCLocatorApiUpdatePath, "CLocatorApi::update_path is 0");

	m_pCLocatorApiPathExists = GetProcAddress(m_XrCoreModuleAddress, "?path_exist@CLocatorAPI@@QAE_NPBD@Z");
	uniassert(m_pCLocatorApiPathExists, "CLocatorApi::path_exists is 0");

	// Осторожно! Собака кусается! (тут функция, проверяем значение указателя на нее)
	m_pLogFunction = reinterpret_cast<LogFuncPtr>(GetProcAddress(m_XrCoreModuleAddress, "?Msg@@YAXPBDZZ"));
	uniassert(m_pLogFunction, "m_pLogFunction is 0");

	// По умолчанию, будем использовать эту заглушку, если в потомках не найдется чего-то поприличнее
	m_pCConsoleGetString = CConsoleGetStringFake;

	/*
	f : textfile;
	{$IFNDEF RELEASE}
	_log_file_name: = "fz_loader_log.txt";
	{$ELSE}
	_log_file_name: = "";
	{$ENDIF}

	if length(_log_file_name) > 0 then begin
	assignfile(f, _log_file_name);
	rewrite(f);
	closefile(f);
	end;*/
}

void* FZBaseGameVersion::FunFromVTable(void* obj, u32 index)
{
	index *= sizeof(void*);
	void* result;

	__asm
	{
		pushad
		mov eax, obj
		mov eax, [eax]
		mov ebx, index
		mov eax, [eax + ebx]
		mov result, eax
		popad
	}

	return result;
}

void* FZBaseGameVersion::DoEcxCall_noarg(void* fun, void* obj)
{
	void* result;

	__asm
	{
		pushad
		mov ecx, obj
		call fun
		mov result, eax
		popad
	}

	return result;
}

void* FZBaseGameVersion::DoEcxCall_1arg(void* fun, void* obj, const void* arg)
{
	void* result;

	__asm
	{
		pushad
		mov ecx, obj
		push arg
		call fun
		mov result, eax
		popad
	}

	return result;
}

void* FZBaseGameVersion::DoEcxCall_2arg(void* fun, void* obj, const void* arg1, const void* arg2)
{
	void* result;

	__asm
	{
		pushad
		mov ecx, obj
		push arg2
		push arg1
		call fun
		mov result, eax
		popad
	}
}

void* FZBaseGameVersion::DoEcxCall_3arg(void* fun, void* obj, const void* arg1, const void* arg2, const void* arg3)
{
	void* result;

	__asm
	{
		pushad
		mov ecx, obj
		push arg3
		push arg2
		push arg1
		call fun
		mov result, eax
		popad
	}
}

void* FZBaseGameVersion::GetLevel() const
{
	return reinterpret_cast<void*>(*static_cast<u32*>(m_pGameLevel));
}

string FZBaseGameVersion::GetCoreParams()
{
	return m_pCore->Params;
}

string FZBaseGameVersion::GetCoreApplicationPath()
{
	return m_pCore->ApplicationPath;
}

void FZBaseGameVersion::ExecuteConsoleCommand(const string &cmd)
{
	DoEcxCall_1arg(m_pCConsoleExecute, reinterpret_cast<void*>(*m_pConsole), cmd.data());
}

string FZBaseGameVersion::GetEngineExeFileName()
{
	return m_ExeModuleName;
}

void* FZBaseGameVersion::GetEngineExeModuleAddress()
{
	return m_ExeModuleAddress;
}

string FZBaseGameVersion::UpdatePath(const string &root, const string &appendix)
{
	string_path path{ '\0' };
	DoEcxCall_3arg(m_pCLocatorApiUpdatePath, reinterpret_cast<void*>(*m_pXrFS), path, root.data(), appendix.data());
	return path;
}

bool FZBaseGameVersion::PathExists(const string &root)
{
	return DoEcxCall_1arg(m_pCLocatorApiPathExists, reinterpret_cast<void*>(*m_pXrFS), root.data());
}

bool FZBaseGameVersion::CheckForLevelExist()
{
	return GetLevel();
}

void FZBaseGameVersion::Log(const string &txt)
{
	//var
	//	f : textfile;
	//begin
	//	if length(_log_file_name) > 0 then begin
	//		assignfile(f, _log_file_name);
	//try
	//	append(f);
	//except
	//	rewrite(f);
	//end;
	//writeln(f, txt);
	//closefile(f);
	//end;

	m_pLogFunction(txt.c_str());
}

string FZBaseGameVersion::GetPlayerName()
{
	const char* cmd = "mm_net_player_name";
	void* res = DoEcxCall_1arg(m_pCConsoleGetString, reinterpret_cast<void*>(*m_pConsole), cmd);
	string result(static_cast<char*>(res));
	return  result;
}

//
//{ FZUnknownGameVersion }
//
//void FZUnknownGameVersion.ShowMpMainMenu();
//begin
//end;
//
//void FZUnknownGameVersion.AssignStatus(str: PAnsiChar);
//begin
//FZLogMgr.Get().Write("Changing status to: " + str, FZ_LOG_INFO);
//end;
//
//function FZUnknownGameVersion.CheckForUserCancelDownload() : boolean;
//begin
//return  false;
//end;
//
//function FZUnknownGameVersion.StartVisualDownload() : boolean;
//begin
//return  true;
//end;
//
//function FZUnknownGameVersion.StopVisualDownload() : boolean;
//begin
//return  true;
//end;
//
//void FZUnknownGameVersion.SetVisualProgress(progress: single);
//begin
//end;
//
//function FZUnknownGameVersion.ThreadSpawn(proc: uintptr; args: uintptr; name:PAnsiChar = nil; stack:cardinal = 0) :boolean;
//var
//thId : cardinal;
//begin
//result : = true;
//thId: = 0;
//CreateThread(nil, 0, pointer(proc), pointer(args), 0, thId);
//end;
//
//void FZUnknownGameVersion.AbortConnection();
//begin
//end;
//
//function FZUnknownGameVersion.IsServerListUpdateActive() : boolean;
//begin
//return  false;
//end;
//
//function FZUnknownGameVersion.IsMessageActive() : boolean;
//begin
//return  false;
//end;
//
//void FZUnknownGameVersion.TriggerMessage();
//begin
//end;
//
//void FZUnknownGameVersion.PrepareForMessageShowing();
//begin
//end;
//
//void FZUnknownGameVersion.ResetMasterServerError();
//begin
//end;
// 

FZCommonGameVersion::FZCommonGameVersion()
{
	m_pStringContainer = reinterpret_cast<u32>(GetProcAddress(m_XrCoreModuleAddress, "?g_pStringContainer@@3PAVstr_container@@A"));
	uniassert(m_pStringContainer, "StringContainer is 0");

	m_pGamePersistent = reinterpret_cast<u32>(GetProcAddress(m_ExeModuleAddress, "?g_pGamePersistent@@3PAVIGame_Persistent@@A"));
	uniassert(m_pGamePersistent, "GamePersistent is 0");

	m_pDevice = reinterpret_cast<u32>(GetProcAddress(m_ExeModuleAddress, "?Device@@3VCRenderDevice@@A"));
	uniassert(m_pDevice, "Device is 0");

	m_pbRendering = reinterpret_cast<int*>(GetProcAddress(m_ExeModuleAddress, "?g_bRendering@@3HA"));
	uniassert(m_pbRendering, "bRendering is 0");

	m_pXrCriticalSectionEnter = GetProcAddress(m_XrCoreModuleAddress, "?Enter@xrCriticalSection@@QAEXXZ");
	uniassert(m_pXrCriticalSectionEnter, "xrCriticalSection::Enter is 0");

	m_pXrCriticalSectionLeave = GetProcAddress(m_XrCoreModuleAddress, "?Leave@xrCriticalSection@@QAEXXZ");
	uniassert(m_pXrCriticalSectionLeave, "xrCriticalSection::Leave is 0");

	m_pStrContainerDock = GetProcAddress(m_XrCoreModuleAddress, "?dock@str_container@@QAEPAUstr_value@@PBD@Z");
	uniassert(m_pStrContainerDock, "str_container::dock is 0");

	//Осторожно! Собака кусается! (тут функция, проверяем значение указателя на нее)
	m_pThreadSpawnFunc = reinterpret_cast<ThreadSpawnFuncPtr>(GetProcAddress(m_XrCoreModuleAddress, "?thread_spawn@@YAXP6AXPAX@ZPBDI0@Z"));
	uniassert(m_pThreadSpawnFunc, "m_pThreadSpawnFunc is 0");
}

// НЕ ТРОГАТЬ! ОПАСНО ДЛЯ ЖИЗНИ!
void FZCommonGameVersion::SafeExec_start()
{
	volatile bool* mt_bMustExit = reinterpret_cast<bool*>(m_pDevice + get_CRenderDevice__mt_bMustExit_offset());
	void* mt_csEnter = reinterpret_cast<void*>(m_pDevice + get_CRenderDevice__mt_csEnter_offset());
	u32* bIsActive = reinterpret_cast<u32*>(m_pDevice + get_CRenderDevice__b_is_Active_offset());

	// Даем cигнал к завершению второго потока
	*mt_bMustExit = true;

	// Ожидаем завершения
	while (*mt_bMustExit) 
		Sleep(1);

	// Теперь мимикрируем под Secondary Thread, захватывая мьютекс, разрешающий
	// начало его выполнения и сигнализируещий главному потоку об активной работе оного
	// Он может быть захвачен только во время активности параллельного участка главного потока!
	DoEcxCall_noarg(m_pXrCriticalSectionEnter, mt_csEnter);

	// Но тут нас ожидает проблема: главный поток сейчас может вовсю исполнять свою работу и
	// рендерить. Надо заблокировать ему возможность начала рендеринга, а если он после этого
	// окажется уже занят им - подождать, пока он закончит свои дела.
	const u32 oldActiveStatus = AtomicExchange(bIsActive, 0);

	// CRenderDevice::b_is_Active, будучи выставлен в false, предотвратит начало рендеринга
	// Но если рендеринг начался до того, как мы выставили флаг, нам надо подождать его конца
	while (*m_pbRendering)
		Sleep(1);

	AtomicExchange(bIsActive, oldActiveStatus);
}

// НЕ ТРОГАТЬ! ОПАСНО ДЛЯ ЖИЗНИ!
void FZCommonGameVersion::SafeExec_end()
{
	// Самое время перезапустить второй поток
	ThreadSpawn(reinterpret_cast<void*>(get_SecondaryThreadProcAddress()), 0, get_SecondaryThreadProcName(), 0);

	// Больше не требуется ничего ждать :)
	DoEcxCall_noarg(m_pXrCriticalSectionLeave, reinterpret_cast<void*>(m_pDevice + get_CRenderDevice__mt_csEnter_offset()));

	// ждать завершения работы итерации главного потока нет необходимости.
	// Более того, вторичный поток еще может успеть захватить mt_csEnter ;)
}

void FZCommonGameVersion::assign_string(u32 pshared_str, const string &text)
{
	uniassert(pshared_str, "pshared_str is nil, cannot assign");
	void* pNewValue = DoEcxCall_1arg(m_pStrContainerDock, *reinterpret_cast<void**>(m_pStringContainer), text.data());

	if (pNewValue)
	{
		const u32 newVal = reinterpret_cast<u32>(pNewValue);
		u32* dwReference = reinterpret_cast<u32*>(newVal + get_str_value__dwReference_offset());
		*dwReference = *dwReference + 1;
	}
	
	if (const u32 pOldValue = *reinterpret_cast<u32*>(pshared_str + get_shared_str__p_offset()))
	{
		u32* dwReference = reinterpret_cast<u32*>(pOldValue + get_str_value__dwReference_offset());
		*dwReference = *dwReference - 1;

		if (!*dwReference)
			*reinterpret_cast<char*>(pshared_str + get_str_value__value_offset()) = '\0';
	}

	void** sharedStrP = reinterpret_cast<void**>(pshared_str + get_shared_str__p_offset());
	*sharedStrP = pNewValue;
}

u32 FZCommonGameVersion::GetMainMenuPtr()
{
	const u32 gamePersistent = *reinterpret_cast<u32*>(m_pGamePersistent);
	uniassert(gamePersistent, "gamePersistent not exist");
	return *reinterpret_cast<u32*>(gamePersistent + get_IGamePersistent__m_pMainMenu_offset());
}

void FZCommonGameVersion::ActivateMainMenu(bool state)
{
	void* imm = reinterpret_cast<void*>(GetMainMenuPtr() + get_CMainMenu_castto_IMainMenu_offset());
	DoEcxCall_1arg(FunFromVTable(imm, virtual_IMainMenu__Activate_index()), imm, reinterpret_cast<void*>(state));
}

bool FZCommonGameVersion::IsMessageActive()
{
	const u32 mm = GetMainMenuPtr();

	if (!mm)
		return false;

	const u32 windows_start = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_pMB_ErrDlgs_first_element_ptr_offset());
	if (!windows_start)
		return false;

	const u32 msgwnd = *reinterpret_cast<u32*>(windows_start + sizeof(u32) * get_CMainMenu__Message_dlg_id());
	if (!msgwnd)
		return false;

	// Сначала смотрим, не показывается ли окно
	if (*reinterpret_cast<bool*>(msgwnd + get_CUIMessageBoxEx_to_CUISimpleWindow_m_bShowMe_offset()))
		return true;

	// Быть может, мы его собираемся показать? (наоборот проверять нельзя! Выставление значения во время активности окна его скрывает!)
	const EErrorDlg m_NeedErrDialog = *reinterpret_cast<EErrorDlg*>(mm + get_CMainMenu__m_NeedErrDialog_offset());
	return m_NeedErrDialog == get_CMainMenu__Message_dlg_id(); // NoNewPatch
}

void FZCommonGameVersion::TriggerMessage()
{
	SetActiveErrorDlg(get_CMainMenu__Message_dlg_id());
}

void FZCommonGameVersion::PrepareForMessageShowing()
{
	// Окно сообщение в ТЧ/ЧН может "зависнуть" в неопределенном состоянии
	// Будем триггерить его до тех пор, пока не "отлипнет"
	while (IsMessageActive())
		TriggerMessage();
}

void FZCommonGameVersion::ResetMasterServerError()
{
	const u32 mm = GetMainMenuPtr();

	if (!mm)
		return;

	const u32 gsfull = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_pGameSpyFull_offset());
	if (!gsfull)
		return;

	const u32 sb = *reinterpret_cast<u32*>(gsfull + get_CGameSpy_Full__m_pGS_SB_offset());
	if (!sb)
		return;

	bool* m_bAbleToConnectToMasterServer = reinterpret_cast<bool*>(sb + get_CGameSpy_Browser__m_bAbleToConnectToMasterServer_offset());
	*m_bAbleToConnectToMasterServer = true;
}

void FZCommonGameVersion::ShowMpMainMenu()
{
	const u32 MP_MENU_CMD = 2;
	const u32 MP_MENU_PARAM = 1;

	const void* arg1 = reinterpret_cast<void*>(MP_MENU_CMD);
	const void* arg2 = reinterpret_cast<void*>(MP_MENU_PARAM);

	const u32 mm = GetMainMenuPtr();
	SafeExec_start();

	ActivateMainMenu(false);
	ActivateMainMenu(true);

	// m_startDialog обновляется после ActivateMainMenu, поэтому нельзя заранее смотреть его положение!
	const u32 dlg = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_startDialog_offset());
	void* pDlg = reinterpret_cast<void*>(dlg);

	if (dlg)
		DoEcxCall_2arg(FunFromVTable(reinterpret_cast<void*>(pDlg), virtual_CUIDialogWnd__Dispatch_index()), pDlg, arg1, arg2);

	SafeExec_end();
}

void FZCommonGameVersion::AssignStatus(const string &str)
{
	const u32 mm = GetMainMenuPtr();
	SafeExec_start();
	assign_string(mm + get_CMainMenu__m_sPDProgress__FileName_offset(), str);
	assign_string(mm + get_CMainMenu__m_sPDProgress__Status_offset(), str);
	SafeExec_end();
}

u32 FZCommonGameVersion::PatchDownloadIsInProgressPtr()
{
	const u32 mm = GetMainMenuPtr();
	return mm + get_CMainMenu__m_sPDProgress__IsInProgress_offset();
}

bool FZCommonGameVersion::CheckForUserCancelDownload()
{
	const bool inProgress = *reinterpret_cast<bool*>(PatchDownloadIsInProgressPtr());
	return !inProgress;
}

bool FZCommonGameVersion::StartVisualDownload()
{
	const u32 mm = GetMainMenuPtr();

	// Назначим строку-пояснение над индикатором загрузки (там что-то должно быть перед
	// назначением IsInProgress, иначе есть вероятность вылета при попытке отрисовки)
	if (!(*reinterpret_cast<char*>(mm + get_CMainMenu__m_sPDProgress__FileName_offset() + get_shared_str__p_offset())))
		AssignStatus("Preparing synchronization...");

	u32 cyclesCount = 1000; //будем ждать 10 секунд, пока нам разрешат загружаться
	u32 tmp;

	do
	{
		cyclesCount = cyclesCount - 1;
		Sleep(10);
		// Атомарно выставим активность загрузки и получим предыдущее значение (только в младшем байте, остальное мусор!)
		tmp = AtomicExchange(reinterpret_cast<u32*>(PatchDownloadIsInProgressPtr()), 1) & 0xFF;
		// Убедимся, что загрузку до нас никто еще не стартовал, пока мы ждали захват мьютекса
	} while (tmp != 0 && cyclesCount != 0);

	if (tmp)
		return false;

	SetVisualProgress(0);

	// На случай нажатия кнопки отмена - укажем, что активного запроса о загрузке не было
	const u32 gsFull = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_pGameSpyFull_offset());
	uniassert(gsFull, "m_pGameSpyFull is 0");

	const u32 gsHttp = *reinterpret_cast<u32*>(gsFull + get_CGameSpy_Full__m_pGS_HTTP_offset());
	uniassert(gsHttp, "m_pGS_HTTP is 0");

	int* m_LastRequest = reinterpret_cast<int*>(gsHttp + get_CGameSpy_HTTP__m_LastRequest_offset());
	*m_LastRequest = -1;

	// Включим главное меню на вкладке мультиплеера(ползунок загрузки есть только там)
	ShowMpMainMenu();
	return true;
}

bool FZCommonGameVersion::StopVisualDownload()
{
	SetVisualProgress(0);
	AtomicExchange(reinterpret_cast<u32*>(PatchDownloadIsInProgressPtr()), 0);
	return true;
}

void FZCommonGameVersion::SetVisualProgress(float progress)
{
	*reinterpret_cast<float*>(GetMainMenuPtr() + get_CMainMenu__m_sPDProgress__Progress_offset()) = progress;
}

bool FZCommonGameVersion::ThreadSpawn(void* proc, void* args, const string &name, u32 stack)
{
	m_pThreadSpawnFunc(proc, name.data(), stack, args);
	return true;
}

void FZCommonGameVersion::AbortConnection()
{
	enum EConnect
	{
		ErrConnect,
		ErrBELoad,
		ErrNoLevel,
		ErrMax,
		ErrNoError = ErrMax,
	};

	if (!CheckForLevelExist())
		return;

	char* lvl = static_cast<char*>(GetLevel());

	bool* m_bConnectResult = reinterpret_cast<bool*>(lvl + get_CLevel__m_bConnectResult_offset());
	bool* m_bConnectResultReceived = reinterpret_cast<bool*>(lvl + get_CLevel__m_bConnectResultReceived_offset());
	const auto m_connect_server_err = reinterpret_cast<EConnect*>(lvl + get_CLevel__m_connect_server_err_offset());
	
	*m_bConnectResult = false;
	*m_connect_server_err = ErrConnect;
	*m_bConnectResultReceived = true;
}

bool FZCommonGameVersion::IsServerListUpdateActive()
{
	const u32 mm = GetMainMenuPtr();
	if (!mm)
		return false;

	const u32 gsfull = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_pGameSpyFull_offset());
	if (!gsfull)
		return false;

	const u32 sb = *reinterpret_cast<u32*>(gsfull + get_CGameSpy_Full__m_pGS_SB_offset());
	if (!sb)
		return false;

	const bool *m_bTryingToConnectToMasterServer = reinterpret_cast<bool*>(sb + get_CGameSpy_Browser__m_bTryingToConnectToMasterServer_offset());
	if (*m_bTryingToConnectToMasterServer)
		return true;

	//Смотрим, не активно ли все еще окно
	const u32 windowStart = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_pMB_ErrDlgs_first_element_ptr_offset());
	if (!windowStart)
		return false;

	const u32 msgwnd = *reinterpret_cast<u32*>(windowStart + sizeof(u32) * get_CMainMenu__ConnectToMasterServer_dlg_id());
	if (!msgwnd)
		return false;

	//Сначала смотрим, не показывается ли окно
	return *reinterpret_cast<bool*>(msgwnd + get_CUIMessageBoxEx_to_CUISimpleWindow_m_bShowMe_offset());
}

u32 FZCommonGameVersion::GetNeedErrorDlg()
{
	const u32 mm = GetMainMenuPtr();

	if (!mm)
		return 0;

	return *reinterpret_cast<u32*>(mm + get_CMainMenu__m_NeedErrDialog_offset());
}

void FZCommonGameVersion::SetActiveErrorDlg(u32 dlg)
{
	const u32 menu = GetMainMenuPtr();

	if (!menu)
		return;

	*reinterpret_cast<u32*>(menu + get_CMainMenu__m_NeedErrDialog_offset()) = dlg;
}

FZGameVersion10006::FZGameVersion10006()
{
	if (void* addr = GetProcAddress(m_ExeModuleAddress, "?GetString@CConsole@@QAEPADPBD@Z"))
		m_pCConsoleGetString = addr;
}

u32 FZGameVersion10006::get_CMainMenu_castto_IMainMenu_offset()
{
	return 0;
}

u32 FZGameVersion10006::get_IGamePersistent__m_pMainMenu_offset()
{
	return 0x468;
}

u32 FZGameVersion10006::get_CMainMenu__m_startDialog_offset()
{
	return 0x50;
}

u32 FZGameVersion10006::get_CMainMenu__m_sPDProgress__FileName_offset()
{
	return 0x284;
}

u32 FZGameVersion10006::get_CMainMenu__m_sPDProgress__Status_offset()
{
	return 0x280;
}

u32 FZGameVersion10006::get_CMainMenu__m_sPDProgress__IsInProgress_offset()
{
	return 0x278;
}

u32 FZGameVersion10006::get_CMainMenu__m_sPDProgress__Progress_offset()
{
	return 0x27C;
}

u32 FZGameVersion10006::get_CMainMenu__m_pGameSpyFull_offset()
{
	return 0x274;
}

u32 FZGameVersion10006::get_CMainMenu__m_NeedErrDialog_offset()
{
	return 0x288;
}

u32 FZGameVersion10006::get_CMainMenu__ConnectToMasterServer_dlg_id()
{
	return 14;
}

// TODO: delete me
u32 FZGameVersion10006::get_CMainMenu__Message_dlg_id()
{
	return 0x10;
}

u32 FZGameVersion10006::get_CGameSpy_Full__m_pGS_HTTP_offset()
{
	return 0x10;
}

u32 FZGameVersion10006::get_CGameSpy_HTTP__m_LastRequest_offset()
{
	return 0x04;
}

u32 FZGameVersion10006::get_CGameSpy_Full__m_pGS_SB_offset()
{
	return 0x14;
}

u32 FZGameVersion10006::get_CGameSpy_Browser__m_bTryingToConnectToMasterServer_offset()
{
	return 0x11;
}

u32 FZGameVersion10006::get_CGameSpy_Browser__m_bAbleToConnectToMasterServer_offset()
{
	return 0x10;
}

u32 FZGameVersion10006::get_CMainMenu__m_pMB_ErrDlgs_first_element_ptr_offset()
{
	return 0x29c;
}

u32 FZGameVersion10006::get_CUIMessageBoxEx_to_CUISimpleWindow_m_bShowMe_offset()
{
	return 4;
}

u32 FZGameVersion10006::get_CRenderDevice__mt_bMustExit_offset()
{
	return 0x34c;
}

u32 FZGameVersion10006::get_CRenderDevice__mt_csEnter_offset()
{
	return 0x344;
}

u32 FZGameVersion10006::get_CRenderDevice__b_is_Active_offset()
{
	return 0x114;
}

u32 FZGameVersion10006::get_CLevel__m_bConnectResult_offset()
{
	return 0x45a1;
}

u32 FZGameVersion10006::get_CLevel__m_bConnectResultReceived_offset()
{
	return 0x45a0;
}

u32 FZGameVersion10006::get_CLevel__m_connect_server_err_offset()
{
	return 0x4594;
}

u32 FZGameVersion10006::get_shared_str__p_offset()
{
	return 0;
}

u32 FZGameVersion10006::get_str_value__dwReference_offset()
{
	return 0;
}

u32 FZGameVersion10006::get_str_value__value_offset()
{
	return 0xc;
}

u32 FZGameVersion10006::get_SecondaryThreadProcAddress()
{
	return reinterpret_cast<u32>(m_ExeModuleAddress) + 0x83450;
}

char* FZGameVersion10006::get_SecondaryThreadProcName()
{
	return reinterpret_cast<char*>(m_ExeModuleAddress) + 0xD4D48;
}

u32 FZGameVersion10006::virtual_IMainMenu__Activate_index()
{
	return 1;
}

u32 FZGameVersion10006::virtual_CUIDialogWnd__Dispatch_index()
{
	return 0x2C;
}

u32 FZGameVersion10006_v2::get_SecondaryThreadProcAddress()
{
	return reinterpret_cast<u32>(m_ExeModuleAddress) + 0x836A0;
}

char* FZGameVersion10006_v2::get_SecondaryThreadProcName()
{
	return reinterpret_cast<char*>(m_ExeModuleAddress + 0xD4D78);
}

FZ_GAME_VERSION FZGameVersionCreator::GetGameVersion()
{
	const FZ_GAME_VERSION result = FZ_GAME_VERSION::FZ_VER_UNKNOWN;	
	const HMODULE xrGS = GetModuleHandle("xrGameSpy.dll");

	if (!xrGS)
		return result;

	using xrGS_GetGameVersion = char* (__cdecl *)(const char* verFromReg);
	const auto getVersion = reinterpret_cast<xrGS_GetGameVersion>(GetProcAddress(xrGS, "xrGS_GetGameVersion"));

	if (!getVersion)
		return result;

	const char* pVersion = getVersion(nullptr);
	const std::string ver = pVersion;

	if (ver != "1.0006")
		return result;

	const u32 xr3DA = reinterpret_cast<u32>(GetModuleHandle("xr_3DA.exe"));

	if (!xr3DA)
		return result;

	const u32 timestamp = *reinterpret_cast<u32*>(xr3DA + *reinterpret_cast<u32*>(xr3DA + 0x3C) + 8);
	return (timestamp == 0x47C577F6) ? FZ_GAME_VERSION::FZ_VER_SOC_10006_V2 : FZ_GAME_VERSION::FZ_VER_SOC_10006;
}

FZAbstractGameVersion* FZGameVersionCreator::DetermineGameVersion()
{
	FZAbstractGameVersion* result = nullptr;

	switch (GetGameVersion())
	{
	case FZ_GAME_VERSION::FZ_VER_SOC_10006:
		result = new FZGameVersion10006();
		break;
	case FZ_GAME_VERSION::FZ_VER_SOC_10006_V2:
		result = new FZGameVersion10006_v2();
		break;
	default:
		uniassert(false, "can not init abstraction");
	}

	return result;
}

FZAbstractGameVersion* g_Abstraction = nullptr;

FZAbstractGameVersion* VersionAbstraction()
{
	return g_Abstraction;
}

bool InitAbstractions()
{
	g_Abstraction = FZGameVersionCreator::DetermineGameVersion();

	if (g_Abstraction)
		Msg("- Abstractions Inited!");

	return g_Abstraction;
}

void FreeAbstractions()
{
	Msg("- Free abstractions");
	delete g_Abstraction;
}
