#pragma once

class FZDownloaderThread;

enum FZDownloadResult
{
	DOWNLOAD_SUCCESS,
	DOWNLOAD_ERROR
};

class FZFileDownloader
{
public:
	FZFileDownloader(const string &url, const string &filename, u32 compression_type, FZDownloaderThread* thread);
	virtual ~FZFileDownloader();

	virtual bool IsDownloading() = 0;
	bool IsBusy();

	string GetUrl();
	string GetFilename();
	u32 GetCompressionType();
	bool StartAsyncDownload();
	bool StartSyncDownload();
	u32 DownloadedBytes();
	bool IsSuccessful();
	bool RequestStop();

	virtual void Flush() = 0;

protected:
	std::recursive_mutex _lock;

	u32 _request;
	string _url;
	string _filename;
	u32 _downloaded_bytes;
	u32 _compression_type;
	FZDownloadResult _status;
	u32 _acquires_count;
	FZDownloaderThread* _thread;
	u32 _filesize;

	void SetRequestId(u32 id);
	u32 GetRequestId();
	void Acquire();
	void Release();
	void Lock();
	void Unlock();
	void SetStatus(FZDownloadResult status);
	void SetFileSize(u32 filesize);
	void SetDownloadedBytesCount(u32 count);

	friend class FZGameSpyDownloaderThread;
	friend class FZDownloaderThreadInfoQueue;
	friend void DownloaderThreadBody(FZDownloaderThread* th);
	friend void UnpackerThreadBody(FZFileDownloader* downloader);
	friend void OnDownloadInProgress(FZFileDownloader* downloader, u32 filesize, u32 downloaded);
	friend void OnDownloadFinished(FZFileDownloader* downloader, FZDownloadResult dlresult);
	friend u32 OnGamespyDownloadFinished(u32 request, u32 requestResult, char* buffer, u32 bufferLen_low, u32 bufferLen_high, void* param);
};

class FZGameSpyFileDownloader : public FZFileDownloader
{
public:
	FZGameSpyFileDownloader(string url, string filename, u32 compression_type, FZDownloaderThread* thread);
	virtual ~FZGameSpyFileDownloader();

	bool IsDownloading() override;
	void Flush() override;
};

class FZCurlFileDownloader : public FZFileDownloader
{
	HANDLE _file_hndl;

public:
	FZCurlFileDownloader(string url, string filename, u32 compression_type, FZDownloaderThread* thread);
	~FZCurlFileDownloader();

	bool IsDownloading() override;
	void Flush() override;
};

enum FZDownloaderThreadCmdType
{
	FZDownloaderAdd,
	FZDownloaderStop
};

struct FZDownloaderThreadCmd
{
	FZDownloaderThreadCmdType cmd;
	FZFileDownloader* downloader;
};

class FZDownloaderThreadInfoQueue
{
public:
	FZDownloaderThreadInfoQueue();
	virtual ~FZDownloaderThreadInfoQueue();

	bool Add(FZDownloaderThreadCmd* item);
	void Flush();
	int Count();
	FZDownloaderThreadCmd* Get(int i);

private:
	vector<FZDownloaderThreadCmd*> _queue;
	int _cur_items_cnt;
};

class FZDownloaderThread
{
public:
	FZDownloaderThread();
	virtual ~FZDownloaderThread();

	bool AddCommand(FZDownloaderThreadCmd* cmd);
	virtual FZFileDownloader* CreateDownloader(const string &url, const string &filename, u32 compression_type) = 0;

	virtual bool StartDownload(FZFileDownloader* dl) = 0;
	virtual bool ProcessDownloads() = 0;
	virtual bool CancelDownload(FZFileDownloader* dl) = 0;

protected:
	std::recursive_mutex _lock;
	FZDownloaderThreadInfoQueue* _commands_queue;
	vector<FZFileDownloader*> _downloaders;
	bool _need_terminate;
	bool _thread_active;
	bool _good;

	void _WaitForThreadTermination();
	int _FindDownloader(FZFileDownloader* dl);
	void _ProcessCommands();

	friend void DownloaderThreadBody(FZDownloaderThread* th);
};

class FZGameSpyDownloaderThread : public FZDownloaderThread
{
private:
	HMODULE _dll_handle;

	typedef void(__cdecl* gHttpStartupFuncPtr)();
	gHttpStartupFuncPtr _xrGS_ghttpStartup;

	typedef void(__cdecl* gHttpCleanupFuncPtr)();
	gHttpCleanupFuncPtr _xrGS_ghttpCleanup;

	typedef void(__cdecl* gHttpThinkFuncPtr)();
	gHttpThinkFuncPtr _xrGS_ghttpThink;

	typedef u32(__cdecl* gHttpSaveFuncPtr)(char* url, char* filename, u32 blocking, void* completedCallback, void* param);
	gHttpSaveFuncPtr _xrGS_ghttpSave;

	typedef u32(__cdecl* gHttpSaveExFuncPtr)(const char* url, const char* fname, char* headers, void* post, u32 throttle, u32 block, void* progressCB, void* completedCB, void* param);
	gHttpSaveExFuncPtr _xrGS_ghttpSaveEx;

	typedef void(__cdecl* gHttpCancelReqFuncPtr)(u32 request);
	gHttpCancelReqFuncPtr _xrGS_ghttpCancelRequest;

public:

	FZGameSpyDownloaderThread();
	virtual ~FZGameSpyDownloaderThread();

	FZFileDownloader* CreateDownloader(const string &url, const string &filename, u32 compression_type) override;
	bool StartDownload(FZFileDownloader* dl) override;
	bool ProcessDownloads() override;
	bool CancelDownload(FZFileDownloader* dl) override;
};