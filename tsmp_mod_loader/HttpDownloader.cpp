#include "Common.h"
#include "lowlevel/Abstractions.h"

#include "HttpDownloader.h"

// TODO: implement curl downloader

extern u32 DecompressFile(const string &filename, u32 compressionType);

//class FZCurlDownloaderThread : public FZDownloaderThread
//{
//_multi_handle:pTCURLM;
//    public
//        constructor Create;
//    destructor Destroy; override;
//    function CreateDownloader(url:string; filename:string; compression_type:cardinal) :FZFileDownloader; override;
//    function StartDownload(dl:FZFileDownloader) : boolean; override;
//    function ProcessDownloads() : boolean; override;
//    function CancelDownload(dl:FZFileDownloader) : boolean; override;
//    end;
//};

const u32 GHTTPSuccess = 0;
const u32 GHTTPRequestError = static_cast<u32>(-1);

const char* TH_LBL = "[TH]";
const char* DL_LBL = "[DL]";
const char* CB_LBL = "[CB]";
const char* QUEUE_LBL = "[Q]";

void CreateThreadedFun(void* proc, void* param)
{
	VersionAbstraction()->ThreadSpawn(proc, param);
}

void DownloaderThreadBody(FZDownloaderThread* th)
{
	Msg("- %s DL thread started", TH_LBL);
	bool need_stop = false;

	while (!need_stop)
	{
		th->_ProcessCommands();
		bool immediate_call = false;
		th->_lock.lock();

		try
		{
			for (int i = th->_downloaders.size() - 1; i >= 0; i--)
			{
				if (!th->_downloaders[i]->IsDownloading())
				{
					Msg("- %s Removing from active list DL %s", TH_LBL, th->_downloaders[i]->GetFilename().c_str());
					th->_downloaders[i]->Release();
					int last = th->_downloaders.size() - 1;

					if (i < last)
						th->_downloaders[i] = th->_downloaders[last];

					th->_downloaders.resize(last);
					Msg("- %s Active downloaders count %u", TH_LBL, th->_downloaders.size());
				}
			}

			if (!th->_downloaders.empty())
				immediate_call = th->ProcessDownloads();

			need_stop = th->_need_terminate && th->_downloaders.empty();
		}
		catch (...)
		{			
		}

		th->_lock.unlock();

		if (!immediate_call)
			Sleep(1);
	}

	Msg("- %s DL thread finished", TH_LBL);
	th->_thread_active = false;
}

void DownloaderThreadBodyWrapper(void* params)
{
	//__asm
	//{
	//	pushad
	//	push ebx
	//	call DownloaderThreadBody
	//	popad
	//}
	DownloaderThreadBody(reinterpret_cast<FZDownloaderThread*>(params));
}

void UnpackerThreadBody(FZFileDownloader* downloader)
{
	Msg("- %s Unpacker thread started", CB_LBL);

	if (downloader->GetCompressionType())
	{
		if (u32 size = DecompressFile(downloader->GetFilename(), downloader->GetCompressionType()))
		{
			downloader->SetFileSize(size);
			downloader->SetDownloadedBytesCount(size);
		}
		else
			downloader->SetStatus(DOWNLOAD_ERROR);
	}

	downloader->Release();
	Msg("- %s Unpacker thread finished", CB_LBL);
}

void UnpackerThreadBodyWrapper(void* params)
{
	//__asm
	//{
	//	pushad
	//	push ebx
	//	call UnpackerThreadBody
	//	popad
	//}
	UnpackerThreadBody(reinterpret_cast<FZFileDownloader*>(params));
}

void OnDownloadInProgress(FZFileDownloader* downloader, u32 filesize, u32 downloaded)
{
	downloader->Lock();
	downloader->SetDownloadedBytesCount(downloaded);
	downloader->SetFileSize(filesize);
	downloader->Unlock();
}

void OnDownloadFinished(FZFileDownloader* downloader, FZDownloadResult dlresult)
{
	downloader->Lock();
	Msg("- %s Download finished for %s with result %u", CB_LBL, downloader->GetFilename().c_str(), static_cast<u32>(dlresult));
	downloader->SetStatus(dlresult);

	if (dlresult == DOWNLOAD_SUCCESS && downloader->GetCompressionType())
	{
		downloader->Acquire();
		CreateThreadedFun(UnpackerThreadBodyWrapper, downloader);
	}

	downloader->Unlock();
}

//
//{ FZCurlDownloaderThread }
//
//constructor FZCurlDownloaderThread.Create;
//begin
//  inherited;
//  _multi_handle:=curl_multi_init();
//  if _multi_handle = nil then begin
//     FZLogMgr.Get.Write(TH_LBL+'Cannot create multy handle!', FZ_LOG_ERROR);
//    _good:=false;
//  end;
//end;
//
//destructor FZCurlDownloaderThread.Destroy;
//begin
//  if _good then begin
//    curl_multi_cleanup(_multi_handle);
//  end;
//  inherited Destroy;
//end;
//
//function FZCurlDownloaderThread.CreateDownloader(url: string; filename: string;
//  compression_type: cardinal): FZFileDownloader;
//begin
//  result:=FZCurlFileDownloader.Create(url, filename, compression_type, self);
//end;
//
//function CurlWriteCb(ptr:PChar; size:cardinal; nitems:Cardinal; userdata:pointer):cardinal; cdecl;
//var
//  res:cardinal;
//  dl:FZFileDownloader;
//begin
//  res:=0;
//  dl:=userdata;
//  if (FZCurlFileDownloader(dl)._file_hndl<>INVALID_HANDLE_VALUE) then begin
//    WriteFile(FZCurlFileDownloader(dl)._file_hndl, ptr[0], nitems*size, res, nil);
//  end;
//  result:=nitems*size;
//end;
//
//function CurlProgressCb(clientp:pointer; dltotal:int64; dlnow:int64; {%H-}ultotal:int64; {%H-}ulnow:int64):integer; cdecl;
//var
//  downloader:FZFileDownloader;
//begin
//  downloader:=clientp;
//  OnDownloadInProgress(downloader, dltotal, dlnow);
//  result:=CURLE_OK;
//end;
//
//function FZCurlDownloaderThread.StartDownload(dl: FZFileDownloader): boolean;
//var
//  purl:pTCURL;
//  dl_i:integer;
//  fname:PAnsiChar;
//  useragent:PAnsiChar;
//begin
//  dl.Lock();
//  try
//    if not _good then begin
//      FZLogMgr.Get.Write(TH_LBL+'Thread is not in a good state', FZ_LOG_ERROR);
//      dl.SetRequestId(0);
//    end else if _FindDownloader(dl)<0 then begin
//      fname:=PAnsiChar(dl.GetFilename());
//
//      FZLogMgr.Get.Write(TH_LBL+'Opening file '+fname, FZ_LOG_INFO);
//
//      FZCurlFileDownloader(dl)._file_hndl:=CreateFile(fname, GENERIC_WRITE, FILE_SHARE_READ, nil, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL or FILE_FLAG_SEQUENTIAL_SCAN, 0);
//      if FZCurlFileDownloader(dl)._file_hndl = INVALID_HANDLE_VALUE then begin
//        FZLogMgr.Get.Write(TH_LBL+'File creation failed for '+dl.GetFilename(), FZ_LOG_ERROR);
//        exit;
//      end;
//      FZLogMgr.Get.Write(TH_LBL+'File '+fname+' opened, handle is '+inttostr(FZCurlFileDownloader(dl)._file_hndl), FZ_LOG_INFO);
//
//      FZLogMgr.Get.Write(TH_LBL+'Setting up DL handle', FZ_LOG_INFO);
//      purl:=curl_easy_init();
//      if purl = nil then begin
//        FZLogMgr.Get.Write(TH_LBL+'Failed to create dl handle!', FZ_LOG_ERROR );
//        exit;
//      end;
//
//      useragent:=PAnsiChar('FreeZone Curl-Downloader, build '+{$INCLUDE %DATE});
//
//      curl_easy_setopt(purl, CURLOPT_URL, uintptr(PAnsiChar(dl.GetUrl())));
//      curl_easy_setopt(purl, CURLOPT_WRITEFUNCTION, uintptr(@CurlWriteCb) );
//      curl_easy_setopt(purl, CURLOPT_WRITEDATA, uintptr(dl));
//      curl_easy_setopt(purl, CURLOPT_NOPROGRESS, 0);
//      curl_easy_setopt(purl, CURLOPT_FAILONERROR, 1);
//      curl_easy_setopt(purl, CURLOPT_USERAGENT, uintptr(useragent));
//      curl_easy_setopt(purl, CURLOPT_XFERINFODATA, uintptr(dl));
//      curl_easy_setopt(purl, CURLOPT_XFERINFOFUNCTION, uintptr(@CurlProgressCb));
//      dl.SetRequestId(uintptr(purl));
//      curl_multi_add_handle(_multi_handle, purl);
//      FZLogMgr.Get.Write(TH_LBL+'Download started for dl '+inttostr(cardinal(dl))+', handle '+inttostr(cardinal(purl)), FZ_LOG_INFO);
//
//      dl_i:=length(_downloaders);
//      setlength(_downloaders, dl_i+1);
//      _downloaders[dl_i]:=dl;
//      result:=true;
//    end;
//  finally
//    dl.Unlock();
//  end;
//end;
//
//function FZCurlDownloaderThread.ProcessDownloads():boolean;
//var
//  cnt, inprogress, i:integer;
//  msg:pCURLMsg;
//  dl:FZFileDownloader;
//
//  dl_result:CURLcode;
//  dl_handle:pTCURL;
//begin
//  result:=curl_multi_perform(_multi_handle, @inprogress) = CURLM_CALL_MULTI_PERFORM;
//
//  repeat
//    msg :=curl_multi_info_read(_multi_handle, @cnt);
//    if (msg<>nil) then begin
//      if (msg.msg = CURLMSG_DONE) then begin
//        dl:=nil;
//        for i:=length(_downloaders)-1 downto 0 do begin
//          if _downloaders[i].GetRequestId() = uintptr(msg.easy_handle) then begin
//            dl:=_downloaders[i];
//            break;
//          end;
//        end;
//
//        //Скопируем статус закачки и хендл (после закрытия хендла доступ к ним пропадет)
//        dl_result:=msg.data.result;
//        dl_handle:=msg.easy_handle;
//
//        //Оптимизация - закрываем соединение, дабы оно не "висело" во время распаковки
//        curl_multi_remove_handle(_multi_handle, msg.easy_handle);
//        curl_easy_cleanup(msg.easy_handle);
//
//        if dl<>nil then begin
//          if (FZCurlFileDownloader(dl)<>nil) and (INVALID_HANDLE_VALUE<>FZCurlFileDownloader(dl)._file_hndl) then begin
//            FZLogMgr.Get.Write(TH_LBL+'Close file handle '+inttostr(FZCurlFileDownloader(dl)._file_hndl), FZ_LOG_INFO);
//            CloseHandle(FZCurlFileDownloader(dl)._file_hndl);
//            FZCurlFileDownloader(dl)._file_hndl:=INVALID_HANDLE_VALUE;
//          end;
//          if dl_result = CURLE_OK then begin
//            OnDownloadFinished(dl, DOWNLOAD_SUCCESS);
//          end else begin
//            FZLogMgr.Get.Write(TH_LBL+'Curl Download Error  ('+inttostr(dl_result)+')', FZ_LOG_ERROR);
//            OnDownloadFinished(dl, DOWNLOAD_ERROR);
//          end;
//          dl.SetRequestId(0);
//        end else begin
//          FZLogMgr.Get.Write(TH_LBL+'Downloader not found for handle '+inttostr(uintptr(dl_handle)), FZ_LOG_ERROR);
//        end;
//      end;
//    end;
//  until cnt = 0 ;
//end;
//
//function FZCurlDownloaderThread.CancelDownload(dl: FZFileDownloader): boolean;
//var
//  purl:pTCURL;
//  dl_i:integer;
//  code:CURLMcode;
//begin
//  result:=false;
//  dl_i:=_FindDownloader(dl);
//  if dl_i>=0 then begin
//    try
//      dl.Lock();
//      purl:=pointer(dl.GetRequestId());
//      if purl<>nil then begin
//        FZLogMgr.Get.Write(TH_LBL+'Cancelling request '+inttostr(uintptr(purl)), FZ_LOG_INFO);
//        if (FZCurlFileDownloader(dl)<>nil) and (INVALID_HANDLE_VALUE<>FZCurlFileDownloader(dl)._file_hndl) then begin
//          CloseHandle(FZCurlFileDownloader(dl)._file_hndl);
//          FZCurlFileDownloader(dl)._file_hndl:=INVALID_HANDLE_VALUE;
//        end;
//        OnDownloadFinished(dl, DOWNLOAD_ERROR);
//        code:=curl_multi_remove_handle(_multi_handle, purl);
//        if code = CURLM_OK then begin
//          curl_easy_cleanup(purl);
//        end else begin
//          FZLogMgr.Get.Write(TH_LBL+'Fail to remove from multi handle ('+inttostr(code)+')', FZ_LOG_ERROR);
//        end;
//        dl.SetRequestId(0);
//        result:=true;
//      end;
//    finally
//      dl.Unlock();
//    end;
//  end else begin
//    FZLogMgr.Get.Write(TH_LBL+'Downloader not found', FZ_LOG_INFO);
//  end;
//end;
//
//{ FZCurlFileDownloader }
//
//constructor FZCurlFileDownloader.Create(url: string; filename: string;
//  compression_type: cardinal; thread: FZDownloaderThread);
//begin
//  inherited;
//  _request := 0;
//  _file_hndl:=INVALID_HANDLE_VALUE;
//end;
//
//function FZCurlFileDownloader.IsDownloading: boolean;
//begin
//  Lock();
//  result:=(_request<>0);
//  Unlock();
//end;
//
//destructor FZCurlFileDownloader.Destroy;
//begin
//  inherited Destroy;
//end;
//
//procedure FZCurlFileDownloader.Flush;
//begin
//  Lock();
//  if _file_hndl<>INVALID_HANDLE_VALUE then begin
//    FZLogMgr.Get.Write(DL_LBL+'Flushing file '+_filename+' ('+inttostr(_file_hndl)+')', FZ_LOG_INFO);
//    FlushFileBuffers(_file_hndl);
//  end;
//  Unlock();
//end;

FZGameSpyFileDownloader::FZGameSpyFileDownloader(string url, string filename, u32 compression_type, FZDownloaderThread* thread) :
	FZFileDownloader(url, filename, compression_type, thread)
{
	_request = GHTTPRequestError;
}

FZGameSpyFileDownloader::~FZGameSpyFileDownloader()
{
	Msg("- %s Wait for DL finished for %s", DL_LBL, _filename.c_str());

	while (IsBusy())
		Sleep(100);

	Msg("- %s Destroying downloader for %s", DL_LBL, _filename.c_str());
}

bool FZGameSpyFileDownloader::IsDownloading()
{
	Lock();
	bool res = _request != GHTTPRequestError;
	Unlock();
	return res;
}

void FZGameSpyFileDownloader::Flush()
{
	Msg("- %s Flush request for %s", DL_LBL, _filename.c_str());
}

FZGameSpyDownloaderThread::FZGameSpyDownloaderThread()
{
	_dll_handle = LoadLibrary("xrGameSpy.dll");
	_xrGS_ghttpStartup = reinterpret_cast<gHttpStartupFuncPtr>(GetProcAddress(_dll_handle, "xrGS_ghttpStartup"));
	_xrGS_ghttpCleanup = reinterpret_cast<gHttpCleanupFuncPtr>(GetProcAddress(_dll_handle, "xrGS_ghttpCleanup"));
	_xrGS_ghttpThink = reinterpret_cast<gHttpThinkFuncPtr>(GetProcAddress(_dll_handle, "xrGS_ghttpThink"));
	_xrGS_ghttpSave = reinterpret_cast<gHttpSaveFuncPtr>(GetProcAddress(_dll_handle, "xrGS_ghttpSave"));
	_xrGS_ghttpSaveEx = reinterpret_cast<gHttpSaveExFuncPtr>(GetProcAddress(_dll_handle, "xrGS_ghttpSaveEx"));
	_xrGS_ghttpCancelRequest = reinterpret_cast<gHttpCancelReqFuncPtr>(GetProcAddress(_dll_handle, "xrGS_ghttpCancelRequest"));

	if (_xrGS_ghttpStartup && _xrGS_ghttpCleanup && _xrGS_ghttpThink && _xrGS_ghttpSave && _xrGS_ghttpSaveEx && _xrGS_ghttpCancelRequest)
		_xrGS_ghttpStartup();
	else
	{
		_good = false;
		Msg("! %s Downloader thread in a bad state!", TH_LBL);
	}
}

FZGameSpyDownloaderThread::~FZGameSpyDownloaderThread()
{
	Msg("- %s Destroying GS downloader thread", TH_LBL);
	_lock.lock();

	_need_terminate = true;
	_lock.unlock();
	_WaitForThreadTermination();

	if (_good)
	{
		Msg("- %s Turn off GS service", TH_LBL);
		_xrGS_ghttpCleanup();
	}

	FreeLibrary(_dll_handle);
}

void __cdecl OnGameSpyDownloadInProgress(u32 request, u32 state, char* buffer, u32 bufferLen_low, u32 bufferLen_high,
	u32 bytesReceived_low, u32 bytesReceived_high, u32 totalSize_low, u32 totalSize_high, void* param)
{
	FZFileDownloader* downloader = reinterpret_cast<FZFileDownloader*>(param);
	OnDownloadInProgress(downloader, totalSize_low, bytesReceived_low);
}

u32 __cdecl OnGamespyDownloadFinished(u32 request, u32 requestResult, char* buffer, u32 bufferLen_low, u32 bufferLen_high, void* param)
{
	FZFileDownloader* downloader = reinterpret_cast<FZFileDownloader*>(param);

	if (requestResult == GHTTPSuccess)
		Msg("- %s OnGamespyDownloadFinished (%u)", CB_LBL, requestResult);
	else
		Msg("! %s OnGamespyDownloadFinished (%u)", CB_LBL, requestResult);

	downloader->Lock();
	downloader->SetDownloadedBytesCount(bufferLen_low);
	downloader->SetFileSize(bufferLen_low);
	downloader->SetRequestId(GHTTPRequestError); //Not downloading now

	if (requestResult == GHTTPSuccess)
		OnDownloadFinished(downloader, DOWNLOAD_SUCCESS);
	else
		OnDownloadFinished(downloader, DOWNLOAD_ERROR);

	downloader->Unlock();
	return 1;
}

FZFileDownloader* FZGameSpyDownloaderThread::CreateDownloader(const string &url, const string &filename, u32 compression_type)
{
	return new FZGameSpyFileDownloader(url, filename, compression_type, this);
}

bool FZGameSpyDownloaderThread::StartDownload(FZFileDownloader* dl)
{
	void* progresscb = &OnGameSpyDownloadInProgress;
	void* finishcb = &OnGamespyDownloadFinished;
	bool res = false;
	dl->Lock();

	try
	{
		if (!_good)
		{
			Msg("! %s Thread is not in a good state", TH_LBL);
			dl->SetRequestId(GHTTPRequestError);
		}
		else if (_FindDownloader(dl) < 0)
		{
			u32 request = _xrGS_ghttpSaveEx(dl->GetUrl().c_str(), dl->GetFilename().c_str(), nullptr, nullptr, 0, 0, progresscb, finishcb, dl);

			if (request != GHTTPRequestError)
			{
				Msg("- %s Download started, request %u", TH_LBL, request);
				int dl_i = _downloaders.size();
				_downloaders.push_back(dl);
				res = true;
			}
			else
				Msg("! %s Failed to start download", TH_LBL);

			dl->SetRequestId(request);
		}
	}
	catch (...)
	{		
	}

	dl->Unlock();
	return res;
}

bool FZGameSpyDownloaderThread::ProcessDownloads()
{
	_xrGS_ghttpThink();
	return false; //Don't call immediately
}

bool FZGameSpyDownloaderThread::CancelDownload(FZFileDownloader* dl)
{
	bool result = false;
	int dl_i = _FindDownloader(dl);

	if (dl_i >= 0)
	{
		dl->Lock();
		u32 request = dl->GetRequestId();

		//Несмотря на то, что пока очередь не очищена, удаления не произойдет - все равно защитим от этого на всякий
		dl->Acquire();

		//Анлок необходим тут, так как во время работы функции отмены реквеста может начать работу колбэк прогресса (из потока мейнменю) и зависнуть на получении мьютекса - дедлок
		dl->Unlock();

		if (request != GHTTPRequestError)
		{
			Msg("- %s Cancelling request %u", TH_LBL, request);
			_xrGS_ghttpCancelRequest(request);

			//Автоматически не вызывается при отмене - приходится вручную
			OnGamespyDownloadFinished(GHTTPRequestError, GHTTPRequestError, nullptr, 0, 0, dl);
		}

		dl->Release();
		result = true;
	}
	else
		Msg("! %s Downloader not found", TH_LBL);

	return result;
}

FZDownloaderThreadInfoQueue::FZDownloaderThreadInfoQueue()
{
	_queue.clear();
	_cur_items_cnt = 0;
}

FZDownloaderThreadInfoQueue::~FZDownloaderThreadInfoQueue()
{
	Flush();
}

bool FZDownloaderThreadInfoQueue::Add(FZDownloaderThreadCmd* item)
{
	item->downloader->Acquire();
	item->downloader->Lock();

	Msg("- %s Put command %u into DL queue for %s", QUEUE_LBL, item->cmd, item->downloader->GetFilename().c_str());
	_queue.push_back(item);
	_cur_items_cnt++;

	Msg("- %s Command in queue, count=%u", QUEUE_LBL, _cur_items_cnt);
	item->downloader->Unlock();
	return true;
}

void FZDownloaderThreadInfoQueue::Flush()
{
	Msg("- %s Flush commands", QUEUE_LBL);

	for (auto cmd : _queue)
		cmd->downloader->Release();

	_queue.clear();
	_cur_items_cnt = 0;
}

int FZDownloaderThreadInfoQueue::Count()
{
	return _cur_items_cnt;
}

FZDownloaderThreadCmd* FZDownloaderThreadInfoQueue::Get(int i)
{
	if (i >= Count())
		Msg("! %s Invalid item index", QUEUE_LBL);
	return _queue[i];
}

FZFileDownloader::FZFileDownloader(const string &url, const string &filename, u32 compression_type, FZDownloaderThread* thread)
{
	_url = url;
	_filename = filename;
	_filesize = 0;
	_compression_type = compression_type;
	_downloaded_bytes = 0;
	_status = DOWNLOAD_SUCCESS;
	_acquires_count = 0;
	_thread = thread;
	Msg("- %s Created downloader for %s from %s, compression %u", DL_LBL, _filename.c_str(), url.c_str(), _compression_type);
}

FZFileDownloader::~FZFileDownloader()
{
	// moved to parent
	//Msg("- %s Wait for DL finished for %s", DL_LBL, _filename.c_str());

	//while (IsBusy())
	//	Sleep(100);

	//Msg("- %s Destroying downloader for %s", DL_LBL, _filename.c_str());
}

bool FZFileDownloader::IsBusy()
{
	Lock();
	bool res = _acquires_count > 0 || IsDownloading();
	Unlock();
	return res;
}

string FZFileDownloader::GetUrl()
{
	Lock();
	string res = _url;
	Unlock();
	return res;
}

string FZFileDownloader::GetFilename()
{
	Lock();
	string res = _filename;
	Unlock();
	return res;
}

u32 FZFileDownloader::GetCompressionType()
{
	Lock();
	u32 res = _compression_type;
	Unlock();
	return res;
}

bool FZFileDownloader::StartAsyncDownload()
{
	bool result = false;
	Lock();

	if (IsBusy())
	{
		Msg("- %s Downloader is busy - cannot start async DL of %s", DL_LBL, _filename.c_str());
		Unlock();
		return false;
	}

	Msg("- %s Start async DL of %s", DL_LBL, _filename.c_str());
	Acquire(); //вызов показывает, что даунлоадер приписан к треду, тред зарелизит его перед удалением из списка

	try
	{
		FZDownloaderThreadCmd* info = new FZDownloaderThreadCmd();
		info->downloader = this;
		info->cmd = FZDownloaderAdd;
		result = _thread->AddCommand(info);
	}
	catch (...)
	{
		Msg("! Catched exception!!!");
	}

	Unlock();

	if (!result)
		Release();

	return result;
}

bool FZFileDownloader::StartSyncDownload()
{
	Msg("- %s Start sync DL of %s", DL_LBL, _filename.c_str());

	if (StartAsyncDownload())
	{
		Msg("- %s Waiting for DL finished %s", DL_LBL, _filename.c_str());
		while (IsBusy())
			Sleep(100);

		return IsSuccessful();
	}
	else
		return false;
}

u32 FZFileDownloader::DownloadedBytes()
{
	Lock();
	u32 result = _downloaded_bytes;
	Unlock();
	return result;
}

bool FZFileDownloader::IsSuccessful()
{
	Lock();
	bool result = _status == DOWNLOAD_SUCCESS;
	Unlock();
	return result;
}

bool FZFileDownloader::RequestStop()
{
	Lock();

	Msg("- %s RequestStop for downloader %s, req = %u", DL_LBL, GetFilename().c_str(), _request);
	Acquire(); //To avoid removing before command is sent

	FZDownloaderThreadCmd* info = new FZDownloaderThreadCmd();
	info->downloader = this;
	info->cmd = FZDownloaderStop;
	Unlock(); //Unlock to avoid deadlock between downloader's and thread's mutexes
	bool res = _thread->AddCommand(info);
	Release();
	return res;
}

void FZFileDownloader::SetRequestId(u32 id)
{
	Lock();
	Msg("- %s Set request id=%u for %s", DL_LBL, id, GetFilename().c_str());
	_request = id;
	Unlock();
}

u32 FZFileDownloader::GetRequestId()
{
	Lock();
	u32 result = _request;
	Unlock();
	return result;
}

void FZFileDownloader::Acquire()
{
	u32 i = InterlockedIncrement(&_acquires_count);
	Msg("- %s Downloader acquired (cnt=%u) %s", DL_LBL, i, GetFilename().c_str());
}

void FZFileDownloader::Release()
{
	string name = GetFilename();
	u32 i = InterlockedDecrement(&_acquires_count);
	Msg("- %s Downloader released (cnt=%u) %s", DL_LBL, i, name.c_str());
}

void FZFileDownloader::Lock()
{
	_lock.lock();
}

void FZFileDownloader::Unlock()
{
	_lock.unlock();
}

void FZFileDownloader::SetStatus(FZDownloadResult status)
{
	Lock();
	_status = status;
	Unlock();
}

void FZFileDownloader::SetFileSize(u32 filesize)
{
	Lock();
	_filesize = filesize;
	Unlock();
}

void FZFileDownloader::SetDownloadedBytesCount(u32 count)
{
	Lock();
	_downloaded_bytes = count;
	Unlock();
}

FZDownloaderThread::FZDownloaderThread()
{
	_commands_queue = new FZDownloaderThreadInfoQueue();

	_need_terminate = false;
	_thread_active = false;
	_good = true;

	Msg("- %s Creating downloader thread fun", TH_LBL);
	_thread_active = true;
	CreateThreadedFun(DownloaderThreadBodyWrapper, this);
}

FZDownloaderThread::~FZDownloaderThread()
{
	Msg("- %s Destroying base downloader thread", TH_LBL);

	//Make sure that thread is terminated
	_lock.lock();
	_need_terminate = true;
	_lock.unlock();

	_WaitForThreadTermination();
	delete _commands_queue;
}

bool FZDownloaderThread::AddCommand(FZDownloaderThreadCmd* cmd)
{
	bool res = false;
	std::lock_guard<std::recursive_mutex> guard(_lock);

	try
	{
		if (!_good)
			return res;

		res = _commands_queue->Add(cmd);
	}
	catch (...) {}

	return res;
}

void FZDownloaderThread::_WaitForThreadTermination()
{
	bool active = true;
	Msg("- %s Waiting for DL thread termination", TH_LBL);

	while (active)
	{
		_lock.lock();
		active = _thread_active;
		_lock.unlock();
	}
}

int FZDownloaderThread::_FindDownloader(FZFileDownloader* dl)
{
	int result = -1;
	std::lock_guard<std::recursive_mutex> guard(_lock);

	for (int i = _downloaders.size() - 1; i > 0; i--)
	{
		if (dl == _downloaders[i])
		{
			result = i;
			break;
		}
	}

	return result;
}

void FZDownloaderThread::_ProcessCommands()
{
	std::lock_guard<std::recursive_mutex> guard(_lock);

	if (int queue_cnt = _commands_queue->Count())
	{
		Msg("- %s Start processing commands (%u)", TH_LBL, queue_cnt);

		for (int i = 0; i < queue_cnt; i++)
		{
			FZDownloaderThreadCmd* command = _commands_queue->Get(i);

			switch (command->cmd)
			{
			case FZDownloaderAdd:
			{
				Msg("- %s Command \"Add\" for downloader %s", TH_LBL, command->downloader->GetFilename().c_str());
				StartDownload(command->downloader);
			}
			break;

			case FZDownloaderStop:
			{
				Msg("- %s Command \"Stop\" for downloader %s", TH_LBL, command->downloader->GetFilename().c_str());
				CancelDownload(command->downloader);
			}
			break;

			default:
				Msg("! %s Unknown command!", TH_LBL);
			}

			Msg("- %s Command processed, active downloaders count %u", TH_LBL, _downloaders.size());
		}

		_commands_queue->Flush();
		Msg("- %s Processing commands finished", TH_LBL);
	}
}
