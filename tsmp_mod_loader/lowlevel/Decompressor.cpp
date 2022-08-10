#include <string>
#include "..\Common.h"

// TODO: implement

//type TDecompressorLogFun = procedure(txt:PAnsiChar); stdcall;
//
//function Init(logfun:TDecompressorLogFun) :boolean;
//procedure Free();
//
//implementation
//uses Windows, sysutils;
//type
//compressor_fun = function(dst:pointer; dst_len:cardinal; src:pointer; src_len:cardinal) :cardinal; cdecl;
//
//var
//rtc_lzo_decompressor : compressor_fun;
//
//_logfun:TDecompressorLogFun;
//
//_cs:TRtlCriticalSection;
//
//procedure Log(txt:PAnsiChar);
//begin
//if @_logfun<>nil then begin
//_logfun(txt);
//end;
//end;
//
//function DecompressLzoFile(filename:string) :cardinal;
//var
//file_handle, mapping_handle:THandle;
//filesize_src, filesize_dst, bytes, crc32:cardinal;
//src_ptr, dst_ptr:pointer;
//begin
//result : = 0;
//bytes: = 0;
//crc32: = 0;
//dst_ptr: = nil;
//src_ptr: = nil;
//filesize_dst: = 0;
//file_handle: = INVALID_HANDLE_VALUE;
//mapping_handle: = INVALID_HANDLE_VALUE;
//
//try
//file_handle: = CreateFile(PAnsiChar(filename), GENERIC_READ, FILE_SHARE_READ, nil, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
//if file_handle = INVALID_HANDLE_VALUE then exit;
//filesize_src: = GetFileSize(file_handle, nil);
//
//mapping_handle: = CreateFileMapping(file_handle, nil, PAGE_READONLY, 0, 0, nil);
//if mapping_handle = INVALID_HANDLE_VALUE then exit;
//src_ptr: = MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, 0);
//
//if not ReadFile(file_handle, filesize_dst, sizeof(filesize_dst), bytes, nil) then exit;
//dst_ptr: = VirtualAlloc(nil, filesize_dst, MEM_COMMIT, PAGE_READWRITE);
//if dst_ptr = nil then exit;
//
//if not ReadFile(file_handle, crc32, sizeof(crc32), bytes, nil) then exit;
//
//Log(PAnsiChar('Running decompressor for ' + filename));
//bytes: = rtc_lzo_decompressor(dst_ptr, filesize_dst, src_ptr + 8, filesize_src - 8);
//if (bytes<>filesize_dst) then exit;
//
//UnmapViewOfFile(src_ptr);
//src_ptr: = nil;
//CloseHandle(mapping_handle);
//mapping_handle: = INVALID_HANDLE_VALUE;
//CloseHandle(file_handle);
//
//Log(PAnsiChar('Writing decompressed data to ' + filename));
//file_handle: = CreateFile(PAnsiChar(filename), GENERIC_WRITE, 0, nil, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
//if file_handle = INVALID_HANDLE_VALUE then exit;
//
//if not WriteFile(file_handle, PByte(dst_ptr)^, filesize_dst, bytes, nil) then exit;
//
//result: = filesize_dst;
//finally
//if dst_ptr<>nil then begin
//VirtualFree(dst_ptr, 0, MEM_RELEASE);
//end;
//if src_ptr<>nil then begin
//UnmapViewOfFile(src_ptr);
//end;
//if mapping_handle<>INVALID_HANDLE_VALUE then begin
//CloseHandle(mapping_handle);
//end;
//if file_handle<>INVALID_HANDLE_VALUE then begin
//CloseHandle(file_handle);
//end;
//end;
//end;

//#define LOG_UNPACKING

std::mutex DecompressLock;

u32 DecompressCabFile(string filename)
{
	string tmpname;
	string cmd;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD exitcode;
	HANDLE file_handle;

	u32 retryCount, retryCount2;

	std::lock_guard<std::mutex> guard(DecompressLock);

	u32 result = 0;
	tmpname = filename + ".tmp";

	Msg("- Trying to unpack %s", filename.c_str());

	retryCount = 3;

	while (retryCount)
	{
		//#ifdef LOG_UNPACKING 
		//	cmd = "cmd.exe /C EXPAND " + filename + " " + tmpname + " > " + filename + '_' + inttostr(retryCount) + '.log';
		//#else
		cmd = "EXPAND " + filename + " " + tmpname;
		//#endif

		retryCount--;

		Msg("Running command %s", cmd.c_str());

		FillMemory(&si, sizeof(si), 0);
		FillMemory(&pi, sizeof(pi), 0);
		si.cb = sizeof(si);

		if (!CreateProcess(0, cmd.data(), 0, 0, false, CREATE_NO_WINDOW, 0, 0, &si, &pi))
		{
			Msg("! ERROR: Cannot create unpacker process");
			return result;
		}

		WaitForSingleObject(pi.hProcess, INFINITE);
		exitcode = 0;

		if (GetExitCodeProcess(pi.hProcess, &exitcode) && exitcode != STILL_ACTIVE)
		{
			retryCount2 = 3;

			while (retryCount2)
			{
				file_handle = CreateFile(tmpname.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
				if (file_handle != INVALID_HANDLE_VALUE)
				{
					result = GetFileSize(file_handle, 0);
					Msg("- Unpacked file size is %u", result);
					CloseHandle(file_handle);

					//Заканчиваем попытки распаковки
					retryCount = 0;
					retryCount2 = 0;
				}
				else
				{
					Msg("! ERROR: cannot open file %s", tmpname.c_str());
					Sleep(1000);
					retryCount2--;
				}
			}
		}
		else
			Msg("! ERROR: process exitcode is %u", exitcode);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		if (retryCount)
			Sleep(5000);
	}

	if (!MoveFileEx(tmpname.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		Msg("! ERROR: Cant move %s to %s", filename.c_str(), tmpname.c_str());
		result = 0;
	}

	return result;
}

u32 DecompressFile(string filename, u32 compressionType)
{
	switch (compressionType)
	{
	case 0:

	{
		HANDLE file_handle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		u32 result = 0;

		if (file_handle != INVALID_HANDLE_VALUE)
		{
			result = GetFileSize(file_handle, nullptr);
			CloseHandle(file_handle);
		}

		return result;
	}
	break;

	//case 1:
	//    result = DecompressLzoFile(filename);
	//    break;

	case 2:
		return DecompressCabFile(filename);
		break;

	default:
		Msg("! ERROR: unknown compression type: %u", compressionType);
		return 0;
		break;
	}
}

//    function Init(logfun:TDecompressorLogFun) :boolean;
//    var
//        xrCore : uintptr;
//    begin
//        result : = false;
//xrCore: = GetModuleHandle('xrCore');
//    if xrCore = 0 then exit;
//
//_logfun: = logfun;
//
//    InitializeCriticalSection(_cs);
//
//rtc_lzo_decompressor: = GetProcAddress(xrCore, '?rtc9_decompress@@YAIPAXIPBXI@Z');
//result: = (@rtc_lzo_decompressor<>nil);
//    end;
//
//    procedure Free;
//    begin
//        DeleteCriticalSection(_cs);
//    end;
//
//    end.
//
