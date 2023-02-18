#include "Common.h"
#include "lowlevel/Abstractions.h"
#include <cstdarg>
#include <debugapi.h>

void Log(const char* message)
{
	if (FZAbstractGameVersion* game = VersionAbstraction())
	{
		string msg = message;
		int insertIdx = 0;

		// Has color prefix ex: "! Error" red
		if (msg[1] == ' ')
			insertIdx = 2;

		msg.insert(insertIdx, "[fz]");
		OutputDebugString(msg.c_str());
		OutputDebugString("\n");
		game->Log(msg);
	}
}

void Msg(const char* format, ...)
{
	va_list mark;
	string1024 buf;
	va_start(mark, format);
	const int sz = _vsnprintf_s(buf, sizeof(buf) - 1, format, mark);
	buf[sizeof(buf) - 1] = '\0';
	va_end(mark);
	
	if (sz)
		Log(buf);
}
