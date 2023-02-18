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
	FZFileDownloader(const string &url, const string &filename, u32 compressionType, FZDownloaderThread* thread);
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
	std::recursive_mutex m_Lock;

	u32 m_Request;
	string m_Url;
	string m_FileName;
	u32 m_DownloadedBytes;
	u32 m_CompressionType;
	FZDownloadResult m_Status;
	u32 m_AcquiresCount;
	FZDownloaderThread* m_pThread;
	u32 m_FileSize;

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
	FZGameSpyFileDownloader(string url, string filename, u32 compressionType, FZDownloaderThread* thread);
	virtual ~FZGameSpyFileDownloader();

	bool IsDownloading() override;
	void Flush() override;
};

class FZCurlFileDownloader : public FZFileDownloader
{
	HANDLE _file_hndl;

public:
	FZCurlFileDownloader(string url, string filename, u32 compressionType, FZDownloaderThread* thread);
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
	u32 Count() const;
	FZDownloaderThreadCmd* Get(int i);

private:
	vector<FZDownloaderThreadCmd*> m_Queue;
	u32 m_CurItemsCnt;
};

class FZDownloaderThread
{
public:
	FZDownloaderThread();
	virtual ~FZDownloaderThread();

	bool AddCommand(FZDownloaderThreadCmd* cmd);
	virtual FZFileDownloader* CreateDownloader(const string &url, const string &filename, u32 compressionType) = 0;

	virtual bool StartDownload(FZFileDownloader* dl) = 0;
	virtual bool ProcessDownloads() = 0;
	virtual bool CancelDownload(FZFileDownloader* dl) = 0;

protected:
	std::recursive_mutex m_RecursiveLock;
	FZDownloaderThreadInfoQueue* m_CommandsQueue;
	vector<FZFileDownloader*> m_Downloaders;
	bool m_bNeedTerminate;
	bool m_bThreadActive;
	bool m_bGood;

	void WaitForThreadTermination();
	int FindDownloader(FZFileDownloader* dl);
	void ProcessCommands();

	friend void DownloaderThreadBody(FZDownloaderThread* th);
};

class FZGameSpyDownloaderThread : public FZDownloaderThread
{
	HMODULE m_hDll;

	using gHttpStartupFuncPtr = void(__cdecl* )();
	gHttpStartupFuncPtr m_pXrGsGHttpStartup;

	using gHttpCleanupFuncPtr = void(__cdecl* )();
	gHttpCleanupFuncPtr m_pXrGsGHttpCleanup;

	using gHttpThinkFuncPtr = void(__cdecl* )();
	gHttpThinkFuncPtr m_pXrGsGHttpThink;

	using gHttpSaveFuncPtr = u32(__cdecl* )(char* url, char* filename, u32 blocking, void* completedCallback, void* param);
	gHttpSaveFuncPtr m_pXrGsGHttpSave;

	using gHttpSaveExFuncPtr = u32(__cdecl* )(const char* url, const char* fname, char* headers, void* post, u32 throttle, u32 block, void* progressCB, void* completedCB, void* param);
	gHttpSaveExFuncPtr m_pXrGsGHttpSaveEx;

	using gHttpCancelReqFuncPtr = void(__cdecl* )(u32 request);
	gHttpCancelReqFuncPtr m_pXrGsGHttpCancelRequest;

public:

	FZGameSpyDownloaderThread();
	virtual ~FZGameSpyDownloaderThread();

	FZFileDownloader* CreateDownloader(const string &url, const string &filename, u32 compressionType) override;
	bool StartDownload(FZFileDownloader* dl) override;
	bool ProcessDownloads() override;
	bool CancelDownload(FZFileDownloader* dl) override;
};
