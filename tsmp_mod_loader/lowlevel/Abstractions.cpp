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
	LoadingError,
	ErrMax,
	ErrNoError = ErrMax,
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
	string _exe_module_name;
	string _xrGame_module_name;
	string _xrCore_module_name;

	HMODULE _exe_module_address;
	HMODULE _xrGame_module_address;
	HMODULE _xrCore_module_address;

	void* _g_ppGameLevel;
	u32* _g_ppConsole;
	u32* _xr_FS;
	pxrCoreData _core;

	void* CConsole__Execute;
	void* CLocatorApi__update_path;
	void* CLocatorApi__path_exists;

	typedef void(__cdecl* Log_fun_Ptr)(const char* text);
	Log_fun_Ptr Log_fun;

	void* CConsole__GetString;
	string _log_file_name;

	void* FunFromVTable(void* obj, u32 index);
	void* DoEcxCall_noarg(void* fun, void* obj);

	void* DoEcxCall_1arg(void* fun, void* obj, const void* arg);
	void* DoEcxCall_2arg(void* fun, void* obj, const void* arg1, const void* arg2);
	void* DoEcxCall_3arg(void* fun, void* obj, const void* arg1, const void* arg2, const void* arg3);

	void* GetLevel();

public:
	FZBaseGameVersion();
	virtual ~FZBaseGameVersion() = default;

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
	u32 _g_ppGamePersistent;	
	u32 _pDevice;
	volatile int *m_pG_pbRendering;

	void* xrCriticalSection__Enter;
	void* xrCriticalSection__Leave;
	void* str_container__dock;
	u32 _g_ppStringContainer;

	typedef void(__cdecl* ThreadSpawnFuncPtr)(void* function, const char* name, u32 stackSize, void* args);
	ThreadSpawnFuncPtr thread_spawn;

	// НЕ ТРОГАТЬ! ОПАСНО ДЛЯ ЖИЗНИ!
	void SafeExec_start();
	void SafeExec_end();

	void assign_string(u32 pshared_str, const string &text);

	u32 GetMainMenu();
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

//implementation

u32 AtomicExchange(u32* addr, u32 val)
{
	return InterlockedExchange(addr, val); // TODO: check
}

void uniassert(bool cond, const string &descr)
{
	if (cond)
		return;

	MessageBox(0, descr.c_str(), "Assertion failed!", MB_OK | MB_ICONERROR);
	TerminateProcess(GetCurrentProcess(), 1);
}

string CConsole__GetString_fake(string cmd) { return "Unknown"; }

FZBaseGameVersion::FZBaseGameVersion()
{
	_exe_module_name = "xr_3DA.exe";
	_exe_module_address = GetModuleHandle(_exe_module_name.c_str());

	_xrGame_module_name = "xrGame";
	_xrGame_module_address = GetModuleHandle(_xrGame_module_name.c_str());

	_xrCore_module_name = "xrCore";
	_xrCore_module_address = GetModuleHandle(_xrCore_module_name.c_str());

	uniassert(_exe_module_address, "xr_3DA is 0");
	uniassert(_xrGame_module_address, "xrGame is 0");
	uniassert(_xrCore_module_address, "xrCore is 0");

	_g_ppGameLevel = GetProcAddress(_exe_module_address, "?g_pGameLevel@@3PAVIGame_Level@@A");
	uniassert(_g_ppGameLevel, "g_ppGameLevel is 0");

	_g_ppConsole = reinterpret_cast<u32*>(GetProcAddress(_exe_module_address, "?Console@@3PAVCConsole@@A"));
	uniassert(_g_ppConsole, "console is 0");

	_core = (pxrCoreData)GetProcAddress(_xrCore_module_address, "?Core@@3VxrCore@@A");
	uniassert(_core, "core is 0");

	_xr_FS = reinterpret_cast<u32*>(GetProcAddress(_xrCore_module_address, "?xr_FS@@3PAVCLocatorAPI@@A"));
	uniassert(_xr_FS, "fs is 0");

	CConsole__Execute = GetProcAddress(_exe_module_address, "?Execute@CConsole@@QAEXPBD@Z");
	uniassert(CConsole__Execute, "CConsole::Execute is 0");

	CLocatorApi__update_path = GetProcAddress(_xrCore_module_address, "?update_path@CLocatorAPI@@QAEPBDAAY0CAI@DPBD1@Z");
	uniassert(CLocatorApi__update_path, "CLocatorApi::update_path is 0");

	CLocatorApi__path_exists = GetProcAddress(_xrCore_module_address, "?path_exist@CLocatorAPI@@QAE_NPBD@Z");
	uniassert(CLocatorApi__path_exists, "CLocatorApi::path_exists is 0");

	//Осторожно! Собака кусается! (тут функция, проверяем значение указателя на нее)
	Log_fun = (Log_fun_Ptr)GetProcAddress(_xrCore_module_address, "?Msg@@YAXPBDZZ");
	uniassert(Log_fun, "Log_fun is 0");

	//По умолчанию, будем использовать эту заглушку, если в потомках не найдется чего-то поприличнее
	CConsole__GetString = CConsole__GetString_fake;

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

void* FZBaseGameVersion::GetLevel()
{
	return reinterpret_cast<void*>(*reinterpret_cast<u32*>(_g_ppGameLevel));
}

string FZBaseGameVersion::GetCoreParams()
{
	return _core->Params;
}

string FZBaseGameVersion::GetCoreApplicationPath()
{
	return _core->ApplicationPath;
}

void FZBaseGameVersion::ExecuteConsoleCommand(const string &cmd)
{
	DoEcxCall_1arg(CConsole__Execute, reinterpret_cast<void*>(*_g_ppConsole), cmd.data());
}

string FZBaseGameVersion::GetEngineExeFileName()
{
	return _exe_module_name;
}

void* FZBaseGameVersion::GetEngineExeModuleAddress()
{
	return _exe_module_address;
}

string FZBaseGameVersion::UpdatePath(const string &root, const string &appendix)
{
	string_path path{ '\0' };
	DoEcxCall_3arg(CLocatorApi__update_path, reinterpret_cast<void*>(*_xr_FS), path, root.data(), appendix.data());
	return path;
}

bool FZBaseGameVersion::PathExists(const string &root)
{
	return DoEcxCall_1arg(CLocatorApi__path_exists, reinterpret_cast<void*>(*_xr_FS), root.data());
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

	Log_fun(txt.c_str());
}

string FZBaseGameVersion::GetPlayerName()
{
	const char* cmd = "mm_net_player_name";
	void* res = DoEcxCall_1arg(CConsole__GetString, reinterpret_cast<void*>(*_g_ppConsole), (void*)cmd);
	return reinterpret_cast<char*>(res);
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
	_g_ppStringContainer = reinterpret_cast<u32>(GetProcAddress(_xrCore_module_address, "?g_pStringContainer@@3PAVstr_container@@A"));
	uniassert(_g_ppStringContainer, "StringContainer is 0");

	_g_ppGamePersistent = reinterpret_cast<u32>(GetProcAddress(_exe_module_address, "?g_pGamePersistent@@3PAVIGame_Persistent@@A"));
	uniassert(_g_ppGamePersistent, "GamePersistent is 0");

	_pDevice = reinterpret_cast<u32>(GetProcAddress(_exe_module_address, "?Device@@3VCRenderDevice@@A"));
	uniassert(_pDevice, "Device is 0");

	m_pG_pbRendering = reinterpret_cast<int*>(GetProcAddress(_exe_module_address, "?g_bRendering@@3HA"));
	uniassert(m_pG_pbRendering, "bRendering is 0");

	xrCriticalSection__Enter = GetProcAddress(_xrCore_module_address, "?Enter@xrCriticalSection@@QAEXXZ");
	uniassert(xrCriticalSection__Enter, "xrCriticalSection::Enter is 0");

	xrCriticalSection__Leave = GetProcAddress(_xrCore_module_address, "?Leave@xrCriticalSection@@QAEXXZ");
	uniassert(xrCriticalSection__Leave, "xrCriticalSection::Leave is 0");

	str_container__dock = GetProcAddress(_xrCore_module_address, "?dock@str_container@@QAEPAUstr_value@@PBD@Z");
	uniassert(str_container__dock, "str_container::dock is 0");

	//Осторожно! Собака кусается! (тут функция, проверяем значение указателя на нее)
	thread_spawn = (ThreadSpawnFuncPtr)GetProcAddress(_xrCore_module_address, "?thread_spawn@@YAXP6AXPAX@ZPBDI0@Z");
	uniassert(thread_spawn, "thread_spawn is 0");
}

// НЕ ТРОГАТЬ! ОПАСНО ДЛЯ ЖИЗНИ!
void FZCommonGameVersion::SafeExec_start()
{
	volatile bool* mt_bMustExit = reinterpret_cast<bool*>(_pDevice + get_CRenderDevice__mt_bMustExit_offset());
	void* mt_csEnter = reinterpret_cast<void*>(_pDevice + get_CRenderDevice__mt_csEnter_offset());
	u32* b_is_Active = reinterpret_cast<u32*>(_pDevice + get_CRenderDevice__b_is_Active_offset());

	// Даем cигнал к завершению второго потока
	*mt_bMustExit = true;

	// Ожидаем завершения
	while (*mt_bMustExit) 
		Sleep(1);

	// Теперь мимикрируем под Secondary Thread, захватывая мьютекс, разрешающий
	// начало его выполнения и сигнализируещий главному потоку об активной работе оного
	// Он может быть захвачен только во время активности параллельного участка главного потока!
	DoEcxCall_noarg(xrCriticalSection__Enter, mt_csEnter);

	// Но тут нас ожидает проблема: главный поток сейчас может вовсю исполнять свою работу и
	// рендерить. Надо заблокировать ему возможность начала рендеринга, а если он после этого
	// окажется уже занят им - подождать, пока он закончит свои дела.
	u32 old_active_status = AtomicExchange(b_is_Active, 0);

	// CRenderDevice::b_is_Active, будучи выставлен в false, предотвратит начало рендеринга
	// Но если рендеринг начался до того, как мы выставили флаг, нам надо подождать его конца
	while (*m_pG_pbRendering)
		Sleep(1);

	AtomicExchange(b_is_Active, old_active_status);
}

// НЕ ТРОГАТЬ! ОПАСНО ДЛЯ ЖИЗНИ!
void FZCommonGameVersion::SafeExec_end()
{
	//Самое время перезапустить второй поток
	ThreadSpawn(reinterpret_cast<void*>(get_SecondaryThreadProcAddress()), 0, get_SecondaryThreadProcName(), 0);

	//Больше не требуется ничего ждать :)
	DoEcxCall_noarg(xrCriticalSection__Leave, reinterpret_cast<void*>(_pDevice + get_CRenderDevice__mt_csEnter_offset()));

	//ждать завершения работы итерации главного потока нет необходимости.
	//Более того, вторичный поток еще может успеть захватить mt_csEnter ;)
}

void FZCommonGameVersion::assign_string(u32 pshared_str, const string &text)
{
	uniassert(pshared_str, "pshared_str is nil, cannot assign");
	void* pnewvalue = DoEcxCall_1arg(str_container__dock, *reinterpret_cast<void**>(_g_ppStringContainer), text.data());

	if (pnewvalue)
	{
		u32 newWalCH = reinterpret_cast<u32>(pnewvalue);
		u32* dwReference = reinterpret_cast<u32*>(newWalCH + get_str_value__dwReference_offset());
		*dwReference = *dwReference + 1;
	}

	u32 poldvalue = *reinterpret_cast<u32*>(pshared_str + get_shared_str__p_offset());

	if (poldvalue)
	{
		u32* dwReference = reinterpret_cast<u32*>(poldvalue + get_str_value__dwReference_offset());
		*dwReference = *dwReference - 1;

		if (!*dwReference)
			*reinterpret_cast<char*>(pshared_str + get_str_value__value_offset()) = '\0';
	}

	void** sharedStrP = reinterpret_cast<void**>(pshared_str + get_shared_str__p_offset());
	*sharedStrP = pnewvalue;
}

u32 FZCommonGameVersion::GetMainMenu()
{
	u32 gamePersistent = *reinterpret_cast<u32*>(_g_ppGamePersistent);
	uniassert(gamePersistent, "gamePersistent not exist");
	return *reinterpret_cast<u32*>(gamePersistent + get_IGamePersistent__m_pMainMenu_offset());
}

void FZCommonGameVersion::ActivateMainMenu(bool state)
{
	void* imm = reinterpret_cast<void*>(GetMainMenu() + get_CMainMenu_castto_IMainMenu_offset());
	DoEcxCall_1arg(FunFromVTable(imm, virtual_IMainMenu__Activate_index()), imm, reinterpret_cast<void*>(state));
}

bool FZCommonGameVersion::IsMessageActive()
{
	u32 mm = GetMainMenu();

	if (!mm)
		return false;

	u32 windows_start = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_pMB_ErrDlgs_first_element_ptr_offset());
	if (!windows_start)
		return false;

	u32 msgwnd = *reinterpret_cast<u32*>(windows_start + sizeof(u32) * get_CMainMenu__Message_dlg_id());
	if (!msgwnd)
		return false;

	//Сначала смотрим, не показывается ли окно
	if (*reinterpret_cast<bool*>(msgwnd + get_CUIMessageBoxEx_to_CUISimpleWindow_m_bShowMe_offset()))
		return true;

	//Быть может, мы его собираемся показать? (наоборот проверять нельзя! Выставление значения во время активности окна его скрывает!)
	EErrorDlg m_NeedErrDialog = *reinterpret_cast<EErrorDlg*>(mm + get_CMainMenu__m_NeedErrDialog_offset());
	return m_NeedErrDialog == get_CMainMenu__Message_dlg_id(); // NoNewPatch
}

void FZCommonGameVersion::TriggerMessage()
{
	SetActiveErrorDlg(get_CMainMenu__Message_dlg_id());
}

void FZCommonGameVersion::PrepareForMessageShowing()
{
	//Окно сообщение в ТЧ/ЧН может "зависнуть" в неопределенном состоянии
	//Будем триггерить его до тех пор, пока не "отлипнет"
	while (IsMessageActive())
		TriggerMessage();
}

void FZCommonGameVersion::ResetMasterServerError()
{
	u32 mm = GetMainMenu();

	if (!mm)
		return;

	u32 gsfull = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_pGameSpyFull_offset());
	if (!gsfull)
		return;

	u32 sb = *reinterpret_cast<u32*>(gsfull + get_CGameSpy_Full__m_pGS_SB_offset());
	if (!sb)
		return;

	bool* m_bAbleToConnectToMasterServer = reinterpret_cast<bool*>(sb + get_CGameSpy_Browser__m_bAbleToConnectToMasterServer_offset());
	*m_bAbleToConnectToMasterServer = true;
}

void FZCommonGameVersion::ShowMpMainMenu()
{
	const u32 MP_MENU_CMD = 2;
	const u32 MP_MENU_PARAM = 1;

	void* arg1 = reinterpret_cast<void*>(MP_MENU_CMD);
	void* arg2 = reinterpret_cast<void*>(MP_MENU_PARAM);

	u32 mm = GetMainMenu();
	SafeExec_start();

	ActivateMainMenu(false);
	ActivateMainMenu(true);

	//m_startDialog обновляется после ActivateMainMenu, поэтому нельзя заранее смотреть его положение!
	u32 dlg = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_startDialog_offset());
	void* pDlg = reinterpret_cast<void*>(dlg);

	if (dlg)
		DoEcxCall_2arg(FunFromVTable(reinterpret_cast<void*>(pDlg), virtual_CUIDialogWnd__Dispatch_index()), pDlg, arg1, arg2);

	SafeExec_end();
}

void FZCommonGameVersion::AssignStatus(const string &str)
{
	u32 mm = GetMainMenu();
	SafeExec_start();
	assign_string(mm + get_CMainMenu__m_sPDProgress__FileName_offset(), str);
	assign_string(mm + get_CMainMenu__m_sPDProgress__Status_offset(), str);
	SafeExec_end();
}

bool FZCommonGameVersion::CheckForUserCancelDownload()
{
	// TODO: разобраться почему срабатывает само
	return false;
	//return *reinterpret_cast<bool*>(GetMainMenu() + get_CMainMenu__m_sPDProgress__IsInProgress_offset());
}

bool FZCommonGameVersion::StartVisualDownload()
{
	u32 mm = GetMainMenu();

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
		//Атомарно выставим активность загрузки и получим предыдущее значение (только в младшем байте, остальное мусор!)
		tmp = AtomicExchange(reinterpret_cast<u32*>(mm + get_CMainMenu__m_sPDProgress__IsInProgress_offset()), 1) & 0xFF;
		//Убедимся, что загрузку до нас никто еще не стартовал, пока мы ждали захват мьютекса
	} while (tmp != 0 && cyclesCount != 0);

	if (tmp)
		return false;

	SetVisualProgress(0);

	//На случай нажатия кнопки отмена - укажем, что активного запроса о загрузке не было
	u32 gsFull = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_pGameSpyFull_offset());
	uniassert(gsFull, "m_pGameSpyFull is 0");

	u32 gsHttp = *reinterpret_cast<u32*>(gsFull + get_CGameSpy_Full__m_pGS_HTTP_offset());
	uniassert(gsHttp, "m_pGS_HTTP is 0");

	int* m_LastRequest = reinterpret_cast<int*>(gsHttp + get_CGameSpy_HTTP__m_LastRequest_offset());
	*m_LastRequest = -1;

	//Включим главное меню на вкладке мультиплеера(ползунок загрузки есть только там)
	ShowMpMainMenu();
	return true;
}

bool FZCommonGameVersion::StopVisualDownload()
{
	SetVisualProgress(0);
	AtomicExchange(reinterpret_cast<u32*>(GetMainMenu() + get_CMainMenu__m_sPDProgress__IsInProgress_offset()), 0);
	return true;
}

void FZCommonGameVersion::SetVisualProgress(float progress)
{
	*reinterpret_cast<float*>(GetMainMenu() + get_CMainMenu__m_sPDProgress__Progress_offset()) = progress;
}

bool FZCommonGameVersion::ThreadSpawn(void* proc, void* args, const string &name, u32 stack)
{
	thread_spawn(proc, name.data(), stack, args);
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

	char* lvl = reinterpret_cast<char*>(GetLevel());

	bool* m_bConnectResult = reinterpret_cast<bool*>(lvl + get_CLevel__m_bConnectResult_offset());
	bool* m_bConnectResultReceived = reinterpret_cast<bool*>(lvl + get_CLevel__m_bConnectResultReceived_offset());
	EConnect* m_connect_server_err = reinterpret_cast<EConnect*>(lvl + get_CLevel__m_connect_server_err_offset());
	
	*m_bConnectResult = false;
	*m_connect_server_err = EConnect::ErrConnect;
	*m_bConnectResultReceived = true;
}

bool FZCommonGameVersion::IsServerListUpdateActive()
{
	u32 mm = GetMainMenu();
	if (!mm)
		return false;

	u32 gsfull = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_pGameSpyFull_offset());
	if (!gsfull)
		return false;

	u32 sb = *reinterpret_cast<u32*>(gsfull + get_CGameSpy_Full__m_pGS_SB_offset());
	if (!sb)
		return false;

	bool *m_bTryingToConnectToMasterServer = reinterpret_cast<bool*>(sb + get_CGameSpy_Browser__m_bTryingToConnectToMasterServer_offset());
	if (*m_bTryingToConnectToMasterServer)
		return true;

	//Смотрим, не активно ли все еще окно
	u32 windows_start = *reinterpret_cast<u32*>(mm + get_CMainMenu__m_pMB_ErrDlgs_first_element_ptr_offset());
	if (!windows_start)
		return false;

	u32 msgwnd = *reinterpret_cast<u32*>(windows_start + sizeof(u32) * get_CMainMenu__ConnectToMasterServer_dlg_id());
	if (!msgwnd)
		return false;

	//Сначала смотрим, не показывается ли окно
	return *reinterpret_cast<bool*>(msgwnd + get_CUIMessageBoxEx_to_CUISimpleWindow_m_bShowMe_offset());
}

u32 FZCommonGameVersion::GetNeedErrorDlg()
{
	u32 mm = GetMainMenu();

	if (!mm)
		return 0;

	return *reinterpret_cast<u32*>(mm + get_CMainMenu__m_NeedErrDialog_offset());
}

void FZCommonGameVersion::SetActiveErrorDlg(u32 dlg)
{
	u32 menu = GetMainMenu();

	if (!menu)
		return;

	*reinterpret_cast<u32*>(menu + get_CMainMenu__m_NeedErrDialog_offset()) = dlg;
}

FZGameVersion10006::FZGameVersion10006()
{
	if (void* addr = GetProcAddress(_exe_module_address, "?GetString@CConsole@@QAEPADPBD@Z"))
		CConsole__GetString = addr;
}

u32 FZGameVersion10006::get_CMainMenu_castto_IMainMenu_offset()
{
	return  0;
}

u32 FZGameVersion10006::get_IGamePersistent__m_pMainMenu_offset()
{
	return  0x468;
}

u32 FZGameVersion10006::get_CMainMenu__m_startDialog_offset()
{
	return  0x50;
}

u32 FZGameVersion10006::get_CMainMenu__m_sPDProgress__FileName_offset()
{
	return  0x284;
}

u32 FZGameVersion10006::get_CMainMenu__m_sPDProgress__Status_offset()
{
	return  0x280;
}

u32 FZGameVersion10006::get_CMainMenu__m_sPDProgress__IsInProgress_offset()
{
	return  0x278;
}

u32 FZGameVersion10006::get_CMainMenu__m_sPDProgress__Progress_offset()
{
	return  0x27C;
}

u32 FZGameVersion10006::get_CMainMenu__m_pGameSpyFull_offset()
{
	return  0x274;
}

u32 FZGameVersion10006::get_CMainMenu__m_NeedErrDialog_offset()
{
	return  0x288;
}

u32 FZGameVersion10006::get_CMainMenu__ConnectToMasterServer_dlg_id()
{
	return  14;
}

// TODO: delete me
u32 FZGameVersion10006::get_CMainMenu__Message_dlg_id()
{
	return  0x10;
}

u32 FZGameVersion10006::get_CGameSpy_Full__m_pGS_HTTP_offset()
{
	return  0x10;
}

u32 FZGameVersion10006::get_CGameSpy_HTTP__m_LastRequest_offset()
{
	return  0x04;
}

u32 FZGameVersion10006::get_CGameSpy_Full__m_pGS_SB_offset()
{
	return  0x14;
}

u32 FZGameVersion10006::get_CGameSpy_Browser__m_bTryingToConnectToMasterServer_offset()
{
	return  0x11;
}

u32 FZGameVersion10006::get_CGameSpy_Browser__m_bAbleToConnectToMasterServer_offset()
{
	return  0x10;
}

u32 FZGameVersion10006::get_CMainMenu__m_pMB_ErrDlgs_first_element_ptr_offset()
{
	return  0x29c;
}

u32 FZGameVersion10006::get_CUIMessageBoxEx_to_CUISimpleWindow_m_bShowMe_offset()
{
	return  4;
}

u32 FZGameVersion10006::get_CRenderDevice__mt_bMustExit_offset()
{
	return  0x34c;
}

u32 FZGameVersion10006::get_CRenderDevice__mt_csEnter_offset()
{
	return  0x344;
}

u32 FZGameVersion10006::get_CRenderDevice__b_is_Active_offset()
{
	return  0x114;
}

u32 FZGameVersion10006::get_CLevel__m_bConnectResult_offset()
{
	return  0x45a1;
}

u32 FZGameVersion10006::get_CLevel__m_bConnectResultReceived_offset()
{
	return  0x45a0;
}

u32 FZGameVersion10006::get_CLevel__m_connect_server_err_offset()
{
	return  0x4594;
}

u32 FZGameVersion10006::get_shared_str__p_offset()
{
	return  0;
}

u32 FZGameVersion10006::get_str_value__dwReference_offset()
{
	return  0;
}

u32 FZGameVersion10006::get_str_value__value_offset()
{
	return  0xc;
}

u32 FZGameVersion10006::get_SecondaryThreadProcAddress()
{
	return reinterpret_cast<u32>(_exe_module_address) + 0x83450;
}

char* FZGameVersion10006::get_SecondaryThreadProcName()
{
	return reinterpret_cast<char*>(_exe_module_address) + 0xD4D48;
}

u32 FZGameVersion10006::virtual_IMainMenu__Activate_index()
{
	return  1;
}

u32 FZGameVersion10006::virtual_CUIDialogWnd__Dispatch_index()
{
	return  0x2C;
}

u32 FZGameVersion10006_v2::get_SecondaryThreadProcAddress()
{
	return reinterpret_cast<u32>(_exe_module_address) + 0x836A0;
}

char* FZGameVersion10006_v2::get_SecondaryThreadProcName()
{
	return reinterpret_cast<char*>(_exe_module_address + 0xD4D78);
}

FZ_GAME_VERSION FZGameVersionCreator::GetGameVersion()
{
	FZ_GAME_VERSION result = FZ_GAME_VERSION::FZ_VER_UNKNOWN;	
	HMODULE xrGS = GetModuleHandle("xrGameSpy.dll");

	if (!xrGS)
		return result;

	typedef char* (__cdecl *xrGS_GetGameVersion)(const char* verFromReg);
	xrGS_GetGameVersion getVersion = (xrGS_GetGameVersion)GetProcAddress(xrGS, "xrGS_GetGameVersion");

	if (!getVersion)
		return result;

	u32* addr = reinterpret_cast<u32*>(reinterpret_cast<u32*>(getVersion)- reinterpret_cast<u32*>(xrGS));
	char*verr = getVersion(nullptr);
	std::string ver = verr;

	if (ver != "1.0006")
		return result;

	u32 xr3DA = reinterpret_cast<u32>(GetModuleHandle("xr_3DA.exe"));

	if (!xr3DA)
		return result;

	u32 Timestamp = *reinterpret_cast<u32*>(xr3DA + *reinterpret_cast<u32*>(xr3DA + 0x3C) + 8);
	return (Timestamp == 0x47C577F6) ? FZ_GAME_VERSION::FZ_VER_SOC_10006_V2 : FZ_GAME_VERSION::FZ_VER_SOC_10006;
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
