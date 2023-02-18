#include <string>
#include "..\Common.h"

// TODO: implement LZO?

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

u32 DecompressCabFile(const string &filename)
{
	std::lock_guard guard(DecompressLock);

	u32 result = 0;
	const string tmpName = filename + ".tmp";

	Msg("- Trying to unpack %s", filename.c_str());

	u32 retryCount = 3;

	while (retryCount)
	{
		//#ifdef LOG_UNPACKING 
		//	cmd = "cmd.exe /C EXPAND " + filename + " " + tmpname + " > " + filename + '_' + inttostr(retryCount) + '.log';
		//#else
		string cmd = "EXPAND \"";
		cmd += filename;
		cmd += "\" \"";
		cmd += tmpName;
		cmd += "\"";
		//#endif

		retryCount--;
		Msg("Running command %s", cmd.c_str());

		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		FillMemory(&si, sizeof(si), 0);
		FillMemory(&pi, sizeof(pi), 0);
		si.cb = sizeof(si);

		if (!CreateProcess(0, cmd.data(), 0, 0, false, CREATE_NO_WINDOW, 0, 0, &si, &pi))
		{
			Msg("! ERROR: Cannot create unpacker process");
			return result;
		}

		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD exitCode = 0;

		if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
		{
			u32 retryCount2 = 3;

			while (retryCount2)
			{
				const HANDLE fileHandle = CreateFile(tmpName.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
				if (fileHandle != INVALID_HANDLE_VALUE)
				{
					result = GetFileSize(fileHandle, 0);
					Msg("- Unpacked file size is %u", result);
					CloseHandle(fileHandle);

					//Заканчиваем попытки распаковки
					retryCount = 0;
					retryCount2 = 0;
				}
				else
				{
					Msg("! ERROR: cannot open file %s", tmpName.c_str());
					Sleep(1000);
					retryCount2--;
				}
			}
		}
		else
			Msg("! ERROR: process exitcode is %u", exitCode);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		if (retryCount)
			Sleep(5000);
	}

	if (!MoveFileEx(tmpName.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		Msg("! ERROR: Cant move %s to %s", filename.c_str(), tmpName.c_str());
		result = 0;
	}

	return result;
}

u32 DecompressFile(const string &filename, u32 compressionType)
{
	u32 result;

	switch (compressionType)
	{
	case 0:
	{
		const HANDLE fileHandle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		result = 0;

		if (fileHandle != INVALID_HANDLE_VALUE)
		{
			result = GetFileSize(fileHandle, nullptr);
			CloseHandle(fileHandle);
		}
	}
	break;

	//case 1:
	//    result = DecompressLzoFile(filename);
	//    break;

	case 2:
		result = DecompressCabFile(filename);
		break;

	default:
		Msg("! ERROR: unknown compression type: %u", compressionType);
		result = 0;
	}

	return result;
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
