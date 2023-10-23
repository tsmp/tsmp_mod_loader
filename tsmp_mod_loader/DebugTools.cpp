#include "Common.h"
#include <minidumpapiset.h>

//#define SEND_ERROR_REPORTS

void DebugInvoke()
{
	__asm int 3;
}

void uniassert(const bool cond, const string& descr)
{
	if (cond)
		return;

	DebugInvoke();
	MessageBox(0, descr.c_str(), "Assertion failed!", MB_OK | MB_ICONERROR);
}

void SetErrorHandler(bool send)
{
#ifdef SEND_ERROR_REPORTS
	using BtSetAppName = void(__stdcall*)(LPCTSTR pszAppName);
	using BtSetSupportSrv = void(__stdcall*)(LPCTSTR pszSupportHost, SHORT nSupportPort);
	using BtSetActivityType = void(__stdcall*)(DWORD eActivityType);

	const DWORD BTA_SENDREPORT = 4;
	const DWORD BTA_SAVEREPORT = 2;
	const HMODULE bt = GetModuleHandle("BugTrap.dll");

	if(!bt)
		return;

	const auto btSetAppName = reinterpret_cast<BtSetAppName>(GetProcAddress(bt, "BT_SetAppName"));
	const auto btSetActivityType = reinterpret_cast<BtSetActivityType>(GetProcAddress(bt, "BT_SetActivityType"));
	const auto btSetSupportSrv = reinterpret_cast<BtSetSupportSrv>(GetProcAddress(bt, "BT_SetSupportServer"));

	btSetAppName("TSMP mod loader");
	btSetActivityType(send ? BTA_SENDREPORT : BTA_SAVEREPORT);
	btSetSupportSrv("192.162.247.202", 9999);
#endif
}
