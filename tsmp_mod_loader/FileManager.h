#pragma once
#include "Common.h"

enum FZActualizingStatus
{
	FZ_ACTUALIZING_BEGIN,
	FZ_ACTUALIZING_IN_PROGRESS,
	FZ_ACTUALIZING_VERIFYING_START,
	FZ_ACTUALIZING_VERIFYING,
	FZ_ACTUALIZING_FINISHED,
	FZ_ACTUALIZING_FAILED
};

struct FZFileActualizingProgressInfo
{
	FZActualizingStatus status;
	long long totalModSize;
	long long totalUpToDateSize;
	long long estimatedDlSize;
	long long totalDownloaded;
};

using FZFileActualizingCallback = bool(*)(const FZFileActualizingProgressInfo &info, void* userdata); //return false if need to stop downloading

enum FZFileItemAction
{
	FZ_FILE_ACTION_UNDEFINED,	// неопределенное состояние, при актуализации удаляется
	FZ_FILE_ACTION_NO,			// файл находится в списке синхронизируемых, после проверки состояние признано не требующим обновления
	FZ_FILE_ACTION_DOWNLOAD,	// файл в списке синхронизируемых, требуется перезагрузка файла
	FZ_FILE_ACTION_IGNORE,		// файл не участвует в синхронизации, никакие действия с ним не производятся
	FZ_FILE_ACTION_VERIFY
};

enum FZDlMode
{
	FZ_DL_MODE_CURL,
	FZ_DL_MODE_GAMESPY
};

struct FZCheckParams
{
	u32 size;
	u32 crc32;
	string md5;
};

struct FZFileItemData
{
	string name;
	string url; // учитывается только при FZ_FILE_ACTION_DOWNLOAD
	u32 compressionType;
	FZFileItemAction requiredAction;
	FZCheckParams real;
	FZCheckParams target; // учитывается только при FZ_FILE_ACTION_DOWNLOAD
};

using pFZFileItemData = FZFileItemData*;

class FZFiles
{
protected:
	string m_ParentPath;
	vector<pFZFileItemData> m_Files;

	FZFileActualizingCallback m_pCallback;
	void* m_pCbUserdata;

	FZDlMode m_Mode;

	bool ScanDir(const string &dir_path);                                                                 //сканирует поддиректорию
	pFZFileItemData CreateFileData(const string &name, const string &url, u32 compression, const FZCheckParams &needChecks); //создает новую запись о файле и добавляет в список

public:
	FZFiles();
	virtual ~FZFiles();

	void Clear();                                                                                          //полная очистка данных списка
	void Dump() const;                                                  //вывод текущего состояния списка, отладочная опция
	bool ScanPath(string dirPath);                                                                 //построение списка файлов в указанной директории и ее поддиректориях для последующей актуализации
	bool UpdateFileInfo(string filename, const string &url, u32 compressionType, const FZCheckParams &targetParams);      //обновить сведения о целевых параметрах файла
	bool ActualizeFiles();                                                                          //актуализировать игровые данные
	void SortBySize();                                                                                     //отсортировать (по размеру) для оптимизации скорости скачивания
	bool AddIgnoredFile(const string &filename);                                                           //добавить игнорируемый файл; вызывать после того, как все UpdateFileInfo выполнены
	void SetCallback(FZFileActualizingCallback cb, void* userdata);                                      //добавить колбэк на обновление состояния синхронизации

	u32 EntriesCount() const;                                                                            //число записей о синхронизируемых файлах
	pFZFileItemData GetEntry(u32 i);                                                               //получить копию информации об указанном файле
	void DeleteEntry(u32 i);                                                                          //удалить запись об синхронизации
	void UpdateEntryAction(u32 i, FZFileItemAction action);                                          //обновить действие для файла

	void SetDlMode(FZDlMode mode);
	void Copy(const FZFiles &from);
};

bool GetFileChecks(const string &path, FZCheckParams &OutCheckParams, bool needMD5);
bool CompareFiles(const FZCheckParams &c1, const FZCheckParams &c2);
