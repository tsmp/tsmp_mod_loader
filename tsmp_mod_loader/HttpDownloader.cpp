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
//    function CreateDownloader(url:string; filename:string; compressionType:cardinal) :FZFileDownloader; override;
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
	bool needStop = false;

	while (!needStop)
	{
		th->ProcessCommands();
		bool immediateCall = false;
		th->m_RecursiveLock.lock();

		try
		{
			for (int i = th->m_Downloaders.size() - 1; i >= 0; i--)
			{
				if (!th->m_Downloaders[i]->IsDownloading())
				{
					Msg("- %s Removing from active list DL %s", TH_LBL, th->m_Downloaders[i]->GetFilename().c_str());
					th->m_Downloaders[i]->Release();
					int last = th->m_Downloaders.size() - 1;

					if (i < last)
						th->m_Downloaders[i] = th->m_Downloaders[last];

					th->m_Downloaders.resize(last);
					Msg("- %s Active downloaders count %u", TH_LBL, th->m_Downloaders.size());
				}
			}

			if (!th->m_Downloaders.empty())
				immediateCall = th->ProcessDownloads();

			needStop = th->m_bNeedTerminate && th->m_Downloaders.empty();
		}
		catch (...)
		{			
		}

		th->m_RecursiveLock.unlock();

		if (!immediateCall)
			Sleep(1);
	}

	Msg("- %s DL thread finished", TH_LBL);
	th->m_bThreadActive = false;
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
//    m_bGood:=false;
//  end;
//end;
//
//destructor FZCurlDownloaderThread.Destroy;
//begin
//  if m_bGood then begin
//    curl_multi_cleanup(_multi_handle);
//  end;
//  inherited Destroy;
//end;
//
//function FZCurlDownloaderThread.CreateDownloader(url: string; filename: string;
//  compressionType: cardinal): FZFileDownloader;
//begin
//  result:=FZCurlFileDownloader.Create(url, filename, compressionType, self);
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
//    if not m_bGood then begin
//      FZLogMgr.Get.Write(TH_LBL+'Thread is not in a good state', FZ_LOG_ERROR);
//      dl.SetRequestId(0);
//    end else if FindDownloader(dl)<0 then begin
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
//      dl_i:=length(m_Downloaders);
//      setlength(m_Downloaders, dl_i+1);
//      m_Downloaders[dl_i]:=dl;
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
//        for i:=length(m_Downloaders)-1 downto 0 do begin
//          if m_Downloaders[i].GetRequestId() = uintptr(msg.easy_handle) then begin
//            dl:=m_Downloaders[i];
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
//  dl_i:=FindDownloader(dl);
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
//  compressionType: cardinal; thread: FZDownloaderThread);
//begin
//  inherited;
//  m_Request := 0;
//  _file_hndl:=INVALID_HANDLE_VALUE;
//end;
//
//function FZCurlFileDownloader.IsDownloading: boolean;
//begin
//  Lock();
//  result:=(m_Request<>0);
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
//    FZLogMgr.Get.Write(DL_LBL+'Flushing file '+m_FileName+' ('+inttostr(_file_hndl)+')', FZ_LOG_INFO);
//    FlushFileBuffers(_file_hndl);
//  end;
//  Unlock();
//end;

FZGameSpyFileDownloader::FZGameSpyFileDownloader(string url, string filename, u32 compressionType, FZDownloaderThread* thread) :
	FZFileDownloader(url, filename, compressionType, thread)
{
	m_Request = GHTTPRequestError;
}

FZGameSpyFileDownloader::~FZGameSpyFileDownloader()
{
	Msg("- %s Wait for DL finished for %s", DL_LBL, m_FileName.c_str());

	while (IsBusy())
		Sleep(100);

	Msg("- %s Destroying downloader for %s", DL_LBL, m_FileName.c_str());
}

bool FZGameSpyFileDownloader::IsDownloading()
{
	Lock();
	bool res = m_Request != GHTTPRequestError;
	Unlock();
	return res;
}

void FZGameSpyFileDownloader::Flush()
{
	Msg("- %s Flush request for %s", DL_LBL, m_FileName.c_str());
}

FZGameSpyDownloaderThread::FZGameSpyDownloaderThread()
{
	m_hDll = LoadLibrary("xrGameSpy.dll");
	m_pXrGsGHttpStartup = reinterpret_cast<gHttpStartupFuncPtr>(GetProcAddress(m_hDll, "xrGS_ghttpStartup"));
	m_pXrGsGHttpCleanup = reinterpret_cast<gHttpCleanupFuncPtr>(GetProcAddress(m_hDll, "xrGS_ghttpCleanup"));
	m_pXrGsGHttpThink = reinterpret_cast<gHttpThinkFuncPtr>(GetProcAddress(m_hDll, "xrGS_ghttpThink"));
	m_pXrGsGHttpSave = reinterpret_cast<gHttpSaveFuncPtr>(GetProcAddress(m_hDll, "xrGS_ghttpSave"));
	m_pXrGsGHttpSaveEx = reinterpret_cast<gHttpSaveExFuncPtr>(GetProcAddress(m_hDll, "xrGS_ghttpSaveEx"));
	m_pXrGsGHttpCancelRequest = reinterpret_cast<gHttpCancelReqFuncPtr>(GetProcAddress(m_hDll, "xrGS_ghttpCancelRequest"));

	if (m_pXrGsGHttpStartup && m_pXrGsGHttpCleanup && m_pXrGsGHttpThink && m_pXrGsGHttpSave && m_pXrGsGHttpSaveEx && m_pXrGsGHttpCancelRequest)
		m_pXrGsGHttpStartup();
	else
	{
		m_bGood = false;
		Msg("! %s Downloader thread in a bad state!", TH_LBL);
	}
}

FZGameSpyDownloaderThread::~FZGameSpyDownloaderThread()
{
	Msg("- %s Destroying GS downloader thread", TH_LBL);
	m_RecursiveLock.lock();

	m_bNeedTerminate = true;
	m_RecursiveLock.unlock();
	WaitForThreadTermination();

	if (m_bGood)
	{
		Msg("- %s Turn off GS service", TH_LBL);
		m_pXrGsGHttpCleanup();
	}

	FreeLibrary(m_hDll);
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

FZFileDownloader* FZGameSpyDownloaderThread::CreateDownloader(const string &url, const string &filename, u32 compressionType)
{
	return new FZGameSpyFileDownloader(url, filename, compressionType, this);
}

bool FZGameSpyDownloaderThread::StartDownload(FZFileDownloader* dl)
{
	void* progresscb = &OnGameSpyDownloadInProgress;
	void* finishcb = &OnGamespyDownloadFinished;
	bool res = false;
	dl->Lock();

	try
	{
		if (!m_bGood)
		{
			Msg("! %s Thread is not in a good state", TH_LBL);
			dl->SetRequestId(GHTTPRequestError);
		}
		else if (FindDownloader(dl) < 0)
		{
			u32 request = m_pXrGsGHttpSaveEx(dl->GetUrl().c_str(), dl->GetFilename().c_str(), nullptr, nullptr, 0, 0, progresscb, finishcb, dl);

			if (request != GHTTPRequestError)
			{
				Msg("- %s Download started, request %u", TH_LBL, request);
				int dl_i = m_Downloaders.size();
				m_Downloaders.push_back(dl);
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
	m_pXrGsGHttpThink();
	return false; //Don't call immediately
}

bool FZGameSpyDownloaderThread::CancelDownload(FZFileDownloader* dl)
{
	bool result = false;
	int dlIt = FindDownloader(dl);

	if (dlIt >= 0)
	{
		dl->Lock();
		const u32 request = dl->GetRequestId();

		//Несмотря на то, что пока очередь не очищена, удаления не произойдет - все равно защитим от этого на всякий
		dl->Acquire();

		//Анлок необходим тут, так как во время работы функции отмены реквеста может начать работу колбэк прогресса (из потока мейнменю) и зависнуть на получении мьютекса - дедлок
		dl->Unlock();

		if (request != GHTTPRequestError)
		{
			Msg("- %s Cancelling request %u", TH_LBL, request);
			m_pXrGsGHttpCancelRequest(request);

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
	m_Queue.clear();
	m_CurItemsCnt = 0;
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
	m_Queue.push_back(item);
	m_CurItemsCnt++;

	Msg("- %s Command in queue, count=%u", QUEUE_LBL, m_CurItemsCnt);
	item->downloader->Unlock();
	return true;
}

void FZDownloaderThreadInfoQueue::Flush()
{
	Msg("- %s Flush commands", QUEUE_LBL);

	for (auto cmd : m_Queue)
		cmd->downloader->Release();

	m_Queue.clear();
	m_CurItemsCnt = 0;
}

u32 FZDownloaderThreadInfoQueue::Count() const
{
	return m_CurItemsCnt;
}

FZDownloaderThreadCmd* FZDownloaderThreadInfoQueue::Get(int i)
{
	if (i >= Count())
		Msg("! %s Invalid item index", QUEUE_LBL);
	return m_Queue[i];
}

FZFileDownloader::FZFileDownloader(const string &url, const string &filename, u32 compressionType, FZDownloaderThread* thread)
{
	m_Url = url;
	m_FileName = filename;
	m_FileSize = 0;
	m_CompressionType = compressionType;
	m_DownloadedBytes = 0;
	m_Status = DOWNLOAD_SUCCESS;
	m_AcquiresCount = 0;
	m_pThread = thread;
	Msg("- %s Created downloader for %s from %s, compression %u", DL_LBL, m_FileName.c_str(), url.c_str(), m_CompressionType);
}

FZFileDownloader::~FZFileDownloader()
{
	// moved to parent
	//Msg("- %s Wait for DL finished for %s", DL_LBL, m_FileName.c_str());

	//while (IsBusy())
	//	Sleep(100);

	//Msg("- %s Destroying downloader for %s", DL_LBL, m_FileName.c_str());
}

bool FZFileDownloader::IsBusy()
{
	Lock();
	const bool res = m_AcquiresCount > 0 || IsDownloading();
	Unlock();
	return res;
}

string FZFileDownloader::GetUrl()
{
	Lock();
	const string res = m_Url;
	Unlock();
	return res;
}

string FZFileDownloader::GetFilename()
{
	Lock();
	const string res = m_FileName;
	Unlock();
	return res;
}

u32 FZFileDownloader::GetCompressionType()
{
	Lock();
	const u32 res = m_CompressionType;
	Unlock();
	return res;
}

bool FZFileDownloader::StartAsyncDownload()
{
	bool result = false;
	Lock();

	if (IsBusy())
	{
		Msg("- %s Downloader is busy - cannot start async DL of %s", DL_LBL, m_FileName.c_str());
		Unlock();
		return false;
	}

	Msg("- %s Start async DL of %s", DL_LBL, m_FileName.c_str());
	Acquire(); //вызов показывает, что даунлоадер приписан к треду, тред зарелизит его перед удалением из списка

	try
	{
		auto info = new FZDownloaderThreadCmd();
		info->downloader = this;
		info->cmd = FZDownloaderAdd;
		result = m_pThread->AddCommand(info);
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
	Msg("- %s Start sync DL of %s", DL_LBL, m_FileName.c_str());

	if (StartAsyncDownload())
	{
		Msg("- %s Waiting for DL finished %s", DL_LBL, m_FileName.c_str());
		while (IsBusy())
			Sleep(100);

		return IsSuccessful();
	}

	return false;
}

u32 FZFileDownloader::DownloadedBytes()
{
	Lock();
	const u32 result = m_DownloadedBytes;
	Unlock();
	return result;
}

bool FZFileDownloader::IsSuccessful()
{
	Lock();
	const bool result = m_Status == DOWNLOAD_SUCCESS;
	Unlock();
	return result;
}

bool FZFileDownloader::RequestStop()
{
	Lock();

	Msg("- %s RequestStop for downloader %s, req = %u", DL_LBL, GetFilename().c_str(), m_Request);
	Acquire(); //To avoid removing before command is sent

	auto info = new FZDownloaderThreadCmd();
	info->downloader = this;
	info->cmd = FZDownloaderStop;
	Unlock(); //Unlock to avoid deadlock between downloader's and thread's mutexes
	const bool res = m_pThread->AddCommand(info);
	Release();
	return res;
}

void FZFileDownloader::SetRequestId(u32 id)
{
	Lock();
	Msg("- %s Set request id=%u for %s", DL_LBL, id, GetFilename().c_str());
	m_Request = id;
	Unlock();
}

u32 FZFileDownloader::GetRequestId()
{
	Lock();
	const u32 result = m_Request;
	Unlock();
	return result;
}

void FZFileDownloader::Acquire()
{
	const u32 i = InterlockedIncrement(&m_AcquiresCount);
	Msg("- %s Downloader acquired (cnt=%u) %s", DL_LBL, i, GetFilename().c_str());
}

void FZFileDownloader::Release()
{
	const string name = GetFilename();
	const u32 i = InterlockedDecrement(&m_AcquiresCount);
	Msg("- %s Downloader released (cnt=%u) %s", DL_LBL, i, name.c_str());
}

void FZFileDownloader::Lock()
{
	m_Lock.lock();
}

void FZFileDownloader::Unlock()
{
	m_Lock.unlock();
}

void FZFileDownloader::SetStatus(FZDownloadResult status)
{
	Lock();
	m_Status = status;
	Unlock();
}

void FZFileDownloader::SetFileSize(u32 filesize)
{
	Lock();
	m_FileSize = filesize;
	Unlock();
}

void FZFileDownloader::SetDownloadedBytesCount(u32 count)
{
	Lock();
	m_DownloadedBytes = count;
	Unlock();
}

FZDownloaderThread::FZDownloaderThread()
{
	m_CommandsQueue = new FZDownloaderThreadInfoQueue();
	m_bNeedTerminate = false;
	m_bGood = true;

	Msg("- %s Creating downloader thread fun", TH_LBL);
	m_bThreadActive = true;
	CreateThreadedFun(DownloaderThreadBodyWrapper, this);
}

FZDownloaderThread::~FZDownloaderThread()
{
	Msg("- %s Destroying base downloader thread", TH_LBL);

	//Make sure that thread is terminated
	m_RecursiveLock.lock();
	m_bNeedTerminate = true;
	m_RecursiveLock.unlock();

	WaitForThreadTermination();
	delete m_CommandsQueue;
}

bool FZDownloaderThread::AddCommand(FZDownloaderThreadCmd* cmd)
{
	bool res = false;
	std::lock_guard guard(m_RecursiveLock);

	try
	{
		if (!m_bGood)
			return res;

		res = m_CommandsQueue->Add(cmd);
	}
	catch (...) {}

	return res;
}

void FZDownloaderThread::WaitForThreadTermination()
{
	bool active = true;
	Msg("- %s Waiting for DL thread termination", TH_LBL);

	while (active)
	{
		m_RecursiveLock.lock();
		active = m_bThreadActive;
		m_RecursiveLock.unlock();
	}
}

int FZDownloaderThread::FindDownloader(FZFileDownloader* dl)
{
	int result = -1;
	std::lock_guard guard(m_RecursiveLock);

	for (u32 i = m_Downloaders.size() - 1; i > 0; i--)
	{
		if (dl == m_Downloaders[i])
		{
			result = i;
			break;
		}
	}

	return result;
}

void FZDownloaderThread::ProcessCommands()
{
	std::lock_guard<std::recursive_mutex> guard(m_RecursiveLock);

	if (u32 queue_cnt = m_CommandsQueue->Count())
	{
		Msg("- %s Start processing commands (%u)", TH_LBL, queue_cnt);

		for (u32 i = 0; i < queue_cnt; i++)
		{
			FZDownloaderThreadCmd* command = m_CommandsQueue->Get(i);

			switch (command->cmd)
			{
			case FZDownloaderAdd:			
				Msg("- %s Command \"Add\" for downloader %s", TH_LBL, command->downloader->GetFilename().c_str());
				StartDownload(command->downloader);
				break;

			case FZDownloaderStop:			
				Msg("- %s Command \"Stop\" for downloader %s", TH_LBL, command->downloader->GetFilename().c_str());
				CancelDownload(command->downloader);
				break;

			default:
				Msg("! %s Unknown command!", TH_LBL);
			}

			Msg("- %s Command processed, active downloaders count %u", TH_LBL, m_Downloaders.size());
		}

		m_CommandsQueue->Flush();
		Msg("- %s Processing commands finished", TH_LBL);
	}
}
