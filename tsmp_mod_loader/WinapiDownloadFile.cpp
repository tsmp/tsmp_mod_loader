#include "Common.h"
#include <WinInet.h>

#pragma comment(lib, "wininet.lib")

const int MaxDownloadFileSize = 0x100000;

bool WinapiDownloadFile(const char* url, const char* path)
{
	HINTERNET hInetSession = InternetOpen(0, INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0);
	HINTERNET hURL = InternetOpenUrl(hInetSession, url, 0, 0, 0, 0);

	char* buffer = new char[MaxDownloadFileSize];
	DWORD dwBytesRead = 0;

	BOOL bResult = InternetReadFile(hURL, buffer, MaxDownloadFileSize, &dwBytesRead);
	InternetCloseHandle(hURL);
	InternetCloseHandle(hInetSession);

	if (!bResult)
	{
		Msg("! WinapiDownloadFile cant download file from [%s]", url);
		return false;
	}

	HANDLE handle = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW | TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (handle == INVALID_HANDLE_VALUE)
	{
		Msg("! WinapiDownloadFile cant open or create file [%s]", path);
		return false;
	}

	DWORD dwBytesWrite = 0;
	bResult = WriteFile(handle, buffer, dwBytesRead, &dwBytesWrite, NULL);
	CloseHandle(handle);

	if (!bResult)
	{
		Msg("! WinapiDownloadFile cant write to file [%s]", path);
		return false;
	}

	return true;
}
