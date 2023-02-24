#include "Common.h"
#include <minidumpapiset.h>

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

	BT_SetAppName("TSMP mod loader");
	BT_SetReportFormat(BTRF_TEXT);
	BT_SetFlags(BTF_DETAILEDMODE | BTF_ATTACHREPORT);
	BT_SetDumpType(MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory | 0);
}
