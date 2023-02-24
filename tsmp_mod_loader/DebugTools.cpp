#include "Common.h"
#include <minidumpapiset.h>
#include "lowlevel/Abstractions.h"

#define BUGTRAP_EXPORTS // for static linking
#include "..\BugTrap\BugTrap.h"

#pragma comment(lib, "BugTrap.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Version.lib")

//#define SEND_ERROR_REPORTS

void uniassert(const bool cond, const string& descr)
{
	if (cond)
		return;

	MessageBox(0, descr.c_str(), "Assertion failed!", MB_OK | MB_ICONERROR);
	TerminateProcess(GetCurrentProcess(), 1);
}

void AttachLogToReport()
{
	FZAbstractGameVersion* game = VersionAbstraction();

	if (!game)
		return;

	game->ExecuteConsoleCommand("flush");
	string logName = game->GetCoreAppName() + "_" + game->GetCoreUserName() + ".log";

	if(game->PathExists("$logs$"))
		logName = game->UpdatePath("$logs$", logName);

	BT_AddLogFile(logName.c_str());
}

void CALLBACK PreErrorHandler(INT_PTR)
{
	AttachLogToReport();
}

void SetErrorHandler()
{
	// Install bugtrap exceptions filter
	BT_InstallSehFilter();

#ifdef SEND_ERROR_REPORTS
	BT_SetSupportServer("192.162.247.202", 9999);
	BT_SetActivityType(BTA_SENDREPORT);	
#else
	BT_SetActivityType(BTA_SAVEREPORT);
#endif

	BT_SetPreErrHandler(PreErrorHandler, 0);
	BT_SetAppName("TSMP mod loader");
	BT_SetReportFormat(BTRF_TEXT);
	BT_SetFlags(BTF_DETAILEDMODE | BTF_ATTACHREPORT);
	BT_SetDumpType(MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory | 0);
}
