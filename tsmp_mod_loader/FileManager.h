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
	long long total_mod_size;
	long long total_up_to_date_size;
	long long estimated_dl_size;
	long long total_downloaded;
};

typedef bool (*FZFileActualizingCallback)(const FZFileActualizingProgressInfo &info, void* userdata); //return false if need to stop downloading

enum FZFileItemAction
{
	FZ_FILE_ACTION_UNDEFINED,	// �������������� ���������, ��� ������������ ���������
	FZ_FILE_ACTION_NO,			// ���� ��������� � ������ ����������������, ����� �������� ��������� �������� �� ��������� ����������
	FZ_FILE_ACTION_DOWNLOAD,	// ���� � ������ ����������������, ��������� ������������ �����
	FZ_FILE_ACTION_IGNORE,		// ���� �� ��������� � �������������, ������� �������� � ��� �� ������������
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
	string url; // ����������� ������ ��� FZ_FILE_ACTION_DOWNLOAD
	u32 compression_type;
	FZFileItemAction required_action;
	FZCheckParams real;
	FZCheckParams target; // ����������� ������ ��� FZ_FILE_ACTION_DOWNLOAD
};

using pFZFileItemData = FZFileItemData*;

class FZFiles
{
protected:
	string _parent_path;
	vector<pFZFileItemData> _files;

	FZFileActualizingCallback _callback;
	void* _cb_userdata;

	FZDlMode _mode;

	bool _ScanDir(const string &dir_path);                                                                 //��������� �������������
	pFZFileItemData _CreateFileData(const string &name, const string &url, u32 compression, const FZCheckParams &need_checks); //������� ����� ������ � ����� � ��������� � ������

public:
	FZFiles();
	virtual ~FZFiles();

	void Clear();                                                                                          //������ ������� ������ ������
	void Dump(/*FZLogMessageSeverity  severity : = FZ_LOG_INFO*/);                                                  //����� �������� ��������� ������, ���������� �����
	bool ScanPath(string dir_path);                                                                 //���������� ������ ������ � ��������� ���������� � �� �������������� ��� ����������� ������������
	bool UpdateFileInfo(string filename, const string &url, u32 compression_type, const FZCheckParams &targetParams);      //�������� �������� � ������� ���������� �����
	bool ActualizeFiles();                                                                          //��������������� ������� ������
	void SortBySize();                                                                                     //������������� (�� �������) ��� ����������� �������� ����������
	bool AddIgnoredFile(const string &filename);                                                           //�������� ������������ ����; �������� ����� ����, ��� ��� UpdateFileInfo ���������
	void SetCallback(FZFileActualizingCallback cb, void* userdata);                                      //�������� ������ �� ���������� ��������� �������������

	int EntriesCount();                                                                            //����� ������� � ���������������� ������
	pFZFileItemData GetEntry(u32 i);                                                               //�������� ����� ���������� �� ��������� �����
	void DeleteEntry(u32 i);                                                                          //������� ������ �� �������������
	void UpdateEntryAction(u32 i, FZFileItemAction action);                                          //�������� �������� ��� �����

	void SetDlMode(FZDlMode mode);
	void Copy(const FZFiles &from);
};

bool GetFileChecks(const string &path, FZCheckParams &OutCheckParams, bool needMD5);
bool CompareFiles(const FZCheckParams &c1, const FZCheckParams &c2);
