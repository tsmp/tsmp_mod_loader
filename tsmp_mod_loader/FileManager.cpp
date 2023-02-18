#include "FileManager.h"
#include "Common.h"
#include <algorithm>
#include "HttpDownloader.h"
#include <shlobj_core.h>

// TODO: enable curl downloader?
// TODO: implement md5 or remove it

bool GetFileChecks(const string &path, FZCheckParams &OutCheckParams, bool needMD5);
bool IsDummy(const FZCheckParams &c);
FZCheckParams GetDummyChecks();
bool CompareFiles(const FZCheckParams &c1, const FZCheckParams &c2);

const char* FM_LBL = "[FM]";
extern bool g_SkipFullFileCheck;

// ������� ����� �� ���������� ����
bool ForceDirectories(const string &path)
{
	int res = SHCreateDirectoryEx(nullptr, path.c_str(), nullptr);
	return res == ERROR_ALREADY_EXISTS || res == ERROR_SUCCESS;
}

bool DeleteFile(const string &path)
{
	return !!DeleteFile(path.c_str());
}

FZCheckParams GetDummyChecks()
{
	FZCheckParams result;

	result.crc32 = 0;
	result.size = 0;
	result.md5 = "";

	return result;
}

bool IsDummy(const FZCheckParams &c)
{
	return !c.crc32 && !c.size && !c.md5.size();
}

bool CompareFiles(const FZCheckParams &c1, const FZCheckParams &c2)
{
	bool res = c1.size == c2.size;

	if (g_SkipFullFileCheck)
		return res;

	return res && c1.crc32 == c2.crc32;
}

bool FZFiles::ScanDir(const string &dir_path)
{
	string name = m_ParentPath + dir_path + "*.*";

	WIN32_FIND_DATA data;
	HANDLE hndl = FindFirstFile(name.c_str(), &data);

	if (hndl == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		name = data.cFileName;

		if (name == "." || name == "..")
			continue;

		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			ScanDir(dir_path + name + '\\');
		else
		{
			CreateFileData(dir_path + name, "", 0, GetDummyChecks());
			Msg("%s %s", FM_LBL, (dir_path + name).c_str());
		}
	} while (FindNextFile(hndl, &data));

	FindClose(hndl);
	return true;
}

inline std::string trim(const std::string& s)
{
	auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c) {return std::isspace(c); });
	auto wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c) {return std::isspace(c); }).base();
	return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
}

pFZFileItemData FZFiles::CreateFileData(const string &name, const string &url, u32 compression, const FZCheckParams &needChecks)
{
	auto result = new FZFileItemData();

	result->name = trim(name);
	result->url = trim(url);
	result->compressionType = compression;
	result->requiredAction = FZ_FILE_ACTION_UNDEFINED;
	result->target = needChecks;
	result->real = GetDummyChecks();

	m_Files.push_back(result);
	return result;
}

FZFiles::FZFiles()
{
	m_pCallback = nullptr;
	m_pCbUserdata = nullptr;
	m_Mode = FZ_DL_MODE_CURL;
}

FZFiles::~FZFiles()
{
	Clear();
}

void FZFiles::Clear()
{
	m_ParentPath = "";
	
	for (pFZFileItemData item : m_Files)
		delete item;

	m_Files.clear();
}

void FZFiles::Dump( /*severity:FZLogMessageSeverity = FZ_LOG_INFO*/)
{
	Msg("- %s =======File list dump start=======", FM_LBL);

	for (pFZFileItemData data : m_Files)
	{
		if (data)
			Msg("- %s %s, action=%u, size %u (%u), crc32 = %#08x (%#08x), url=%s", FM_LBL, data->name.c_str(), data->requiredAction, data->real.size, data->target.size,
				data->real.crc32, data->target.crc32, data->url.c_str());
	}

	Msg("- %s =======File list dump end=======", FM_LBL);
}

bool FZFiles::ScanPath(string dirPath)
{
	Clear();
	Msg("- %s =======Scanning directory=======", FM_LBL);

	if (dirPath[dirPath.size() - 1] != '\\' && dirPath[dirPath.size() - 1] != '/')
		dirPath += '\\';

	m_ParentPath = dirPath;
	ScanDir("");
	Msg("- %s =======Scanning finished=======", FM_LBL);
	return true;
}

bool FZFiles::UpdateFileInfo(string filename, const string &url, u32 compressionType, const FZCheckParams &targetParams)
{
	Msg("- %s Updating file info for %s, size = %u, crc = %#08x, url = %s, compression %u",
		FM_LBL, filename.c_str(), targetParams.size, targetParams.crc32, /* md5=[' + targetParams.md5 + ']' */  url.c_str(), compressionType);

	filename = trim(filename);

	if (filename.find("..") != string::npos)
	{
		Msg("! %s File path cannot contain ..", FM_LBL);
		return false;
	}

	//������� ����� ���� ����� ������������
	pFZFileItemData filedata = nullptr;

	for (pFZFileItemData item : m_Files)
	{
		if (item && item->name == filename)
		{
			filedata = item;
			break;
		}
	}

	if (!filedata)
	{
		//����� ��� � ������. ���������� ���� ������.
		filedata = CreateFileData(filename, url, compressionType, targetParams);
		filedata->requiredAction = FZ_FILE_ACTION_DOWNLOAD;
		Msg("- %s Created new file list entry", FM_LBL);
	}
	else
	{
		//���� ���� � ������. ���������.
		if (IsDummy(filedata->real))
		{
			//��������� ��� CRC32, ���� ����� �� ��������
			if (!GetFileChecks(m_ParentPath + filedata->name, filedata->real, !targetParams.md5.empty()))
			{
				filedata->real.crc32 = 0;
				filedata->real.size = 0;
				filedata->real.md5 = "";
			}

			Msg("- %s Current file info: CRC32= crc = %#08x, size=%u, ", FM_LBL, filedata->real.crc32, filedata->real.size/*, md5 = [' + filedata.real.md5 + ']'*/);
		}

		if (CompareFiles(filedata->real, targetParams))
		{
			filedata->requiredAction = FZ_FILE_ACTION_NO;
			Msg("- %s Entry exists, file up-to-date", FM_LBL);
		}
		else
		{
			filedata->requiredAction = FZ_FILE_ACTION_DOWNLOAD;
			Msg("- %s Entry exists, file outdated", FM_LBL);
		}

		filedata->target = targetParams;
		filedata->url = url;
		filedata->compressionType = compressionType;
	}

	return true;
}

void FZFiles::SortBySize()
{
	if (m_Files.size() < 2)
		return;

	//��������� ���, ����� ����� ������� ����� ��������� � �����
	for (int i = m_Files.size() - 1; i > 0; i--)
	{
		int max = i;

		//���� ����� ������� ����
		for (int j = i - 1; j >= 0; j--)
		{
			if (!m_Files[j])
				continue;

			if (!m_Files[max] || m_Files[max]->target.size < m_Files[i]->target.size)
				max = j;
		}

		if (i != max)
		{
			pFZFileItemData temp = m_Files[i];
			m_Files[i] = m_Files[max];
			m_Files[max] = temp;
		}
	}
}

bool FZFiles::ActualizeFiles()
{
	const u32 MAX_ACTIVE_DOWNLOADERS = 4;  //for safety

	long long totalActualSize, downloadedTotal;
	long long totalDlSize = totalActualSize = downloadedTotal = 0;

	for (u32 i = 0; i < m_Files.size(); i++)
	{
		pFZFileItemData filedata = m_Files[i];

		if (!filedata)
			continue;

		if (filedata->requiredAction == FZ_FILE_ACTION_UNDEFINED)
		{
			//������ ����� �� ���� � ������. ������.
			Msg("- %s Deleting file %s", FM_LBL, filedata->name.c_str());

			if (!DeleteFile(m_ParentPath + filedata->name))
			{
				Msg("! %s Failed to delete ", FM_LBL, filedata->name.c_str());
				return false;
			}

			delete filedata;
			m_Files.erase(m_Files.begin() + i);
		}
		else if (filedata->requiredAction == FZ_FILE_ACTION_DOWNLOAD)
		{
			totalDlSize = totalDlSize + filedata->target.size;
			string str = m_ParentPath + filedata->name;

			while (str[str.size() - 1] != '\\' && str[str.size() - 1] != '/')
				str.resize(str.size() - 1);

			if (!ForceDirectories(str))
			{
				Msg("! %s Cannot create directory %s", FM_LBL, str.c_str());
				return false;
			}
		}
		else if (filedata->requiredAction == FZ_FILE_ACTION_NO)
		{
			totalActualSize = totalActualSize + filedata->target.size;
		}
		else if (filedata->requiredAction == FZ_FILE_ACTION_IGNORE)
		{
			Msg("- %s Ignoring file %s", FM_LBL, filedata->name.c_str());
		}
		else
		{
			Msg("! %s Unknown action for %s", FM_LBL, filedata->name.c_str());
			return false;
		}
	}

	Msg("- %s Total up-to-date size %u", FM_LBL, totalActualSize);
	Msg("- %s Total downloads size %u", FM_LBL, totalDlSize);

	Msg("- %s Starting downloads", FM_LBL);
	bool result = true;

	//������� ������ ��� ��������� � ������ ������ ��������
	FZFileActualizingProgressInfo cbInfo;
	cbInfo.totalDownloaded = 0;
	cbInfo.status = FZ_ACTUALIZING_BEGIN;
	cbInfo.estimatedDlSize = totalDlSize;
	cbInfo.totalUpToDateSize = totalActualSize;
	cbInfo.totalModSize = totalDlSize + totalActualSize;

	if (m_pCallback)
		result = m_pCallback(cbInfo, m_pCbUserdata);
	
	//������ ��������
	//if (m_Mode==FZ_DL_MODE_GAMESPY)
	auto thread = new FZGameSpyDownloaderThread();
	//else
	//    thread=new FZCurlDownloaderThread();

	int lastFileIndex = m_Files.size() - 1;

	vector<FZFileDownloader*> downloaders;
	downloaders.reserve(MAX_ACTIVE_DOWNLOADERS);

	for (int i = 0; i < MAX_ACTIVE_DOWNLOADERS; i++)
		downloaders.push_back(nullptr);

	bool finished = false;
	cbInfo.status = FZ_ACTUALIZING_IN_PROGRESS;

	while (!finished)
	{
		if (!result)
		{
			//�������� ��������.
			Msg("! %s Actualizing cancelled", FM_LBL);
			break;
		}

		finished = true; //���� ��������� ��� �������� ��������
		long long downloadedNow = 0;

		//������� �� ������� ������ ������ ��������
		for (u32 i = 0; i < downloaders.size(); i++)
		{
			if (!downloaders[i])
			{
				//���� ��������. �������� ���� ���-������, ���� ������
				while (lastFileIndex >= 0)
				{
					pFZFileItemData filedata = m_Files[lastFileIndex];
					lastFileIndex = lastFileIndex - 1; //�������� ������ �� �������������� ����

					if (filedata && filedata->requiredAction == FZ_FILE_ACTION_DOWNLOAD)
					{
						//���� ��� �������� ������, �������� ��� � ����.
						Msg("- %s Starting download of %s", FM_LBL, filedata->url.c_str());
						finished = false;

						downloaders[i] = thread->CreateDownloader(filedata->url, m_ParentPath + filedata->name, filedata->compressionType);
						result = downloaders[i]->StartAsyncDownload();

						if (!result)
							Msg("! %s Cannot start download for %s", FM_LBL, filedata->url.c_str());
						else
							filedata->requiredAction = FZ_FILE_ACTION_VERIFY;

						break;
					}
				}
			}
			else if (downloaders[i]->IsBusy())
			{
				//���� �������. ������� ���������� � ���������
				finished = false;
				downloadedNow = downloadedNow + downloaders[i]->DownloadedBytes();
			}
			else
			{
				//���� �������� ������. ��������� ���.
				Msg("- %s Need free slot contained %s", FM_LBL, downloaders[i]->GetFilename().c_str());
				result = downloaders[i]->IsSuccessful();
				downloadedTotal = downloadedTotal + downloaders[i]->DownloadedBytes();

				if (!result)
					Msg("! %s Download failed for %s", FM_LBL, downloaders[i]->GetFilename().c_str());;

				delete downloaders[i];
				downloaders[i] = nullptr;

				//�� ����� ���� ��� ����� � ������� �� ��������, �������, �� ������ ���������� � true
				finished = false;
			}
		}

		//������� ������ ���������
		cbInfo.totalDownloaded = downloadedNow + downloadedTotal;

		if (m_pCallback)
			result = m_pCallback(cbInfo, m_pCbUserdata);

		if (result)
			Sleep(100);
	}

	//������������� ���� ��
	Msg("- %s Request stop", FM_LBL);

	for (FZFileDownloader* loader : downloaders)
	{
		if (loader)
			loader->RequestStop();
	}

	//� ������� ��
	Msg("- %s Delete downloaders", FM_LBL);
	for (FZFileDownloader* loader : downloaders)
		delete loader;

	downloaders.clear();
	delete thread;

	if (result)
	{
		//��������, ��� ��� ��������� ��������� ���������
		if (totalDlSize > 0)
		{
			Msg("- %s Verifying downloaded", FM_LBL);

			cbInfo.status = FZ_ACTUALIZING_VERIFYING_START;
			cbInfo.totalDownloaded = 0;
			result = m_pCallback(cbInfo, m_pCbUserdata);

			for (pFZFileItemData filedata : m_Files)
			{
				if (!result)
					break;

				if (filedata && filedata->requiredAction == FZ_FILE_ACTION_VERIFY)
				{
					if (GetFileChecks(m_ParentPath + filedata->name, filedata->real, !filedata->target.md5.empty()))
					{
						if (!CompareFiles(filedata->real, filedata->target))
						{
							Msg("! %s File NOT synchronized: %s", FM_LBL, filedata->name.c_str());
							filedata->requiredAction = FZ_FILE_ACTION_DOWNLOAD;
							result = false;

						}
					}
					else
					{
						Msg("! %s Cannot check %s", FM_LBL, filedata->name.c_str());

						filedata->requiredAction = FZ_FILE_ACTION_DOWNLOAD;
						result = false;
					}
				}
				else if (filedata && filedata->requiredAction == FZ_FILE_ACTION_DOWNLOAD)
				{
					Msg("! %s File %s has FZ_FILE_ACTION_DOWNLOAD state after successful synchronization??? A bug suspected!", FM_LBL, filedata->name.c_str());
					result = false;
				}

				if (result)
				{
					cbInfo.status = FZ_ACTUALIZING_VERIFYING;
					cbInfo.totalDownloaded = cbInfo.totalDownloaded + filedata->target.size;
					result = m_pCallback(cbInfo, m_pCbUserdata);
				}
			}
		}

		//�������� ������ ���������
		if (result)
		{
			Msg("- %s Run finish callback", FM_LBL);
			cbInfo.status = FZ_ACTUALIZING_FINISHED;
			cbInfo.totalDownloaded = downloadedTotal;
			result = m_pCallback(cbInfo, m_pCbUserdata);
		}
		else
		{
			Msg("- %s Run fail callback", FM_LBL);
			cbInfo.status = FZ_ACTUALIZING_FAILED;
			cbInfo.totalDownloaded = 0;
			m_pCallback(cbInfo, m_pCbUserdata);
		}

		Msg("- %s All downloads finished", FM_LBL);
	}

	return result;
}

bool FZFiles::AddIgnoredFile(const string &filename)
{
	//������� ����� ���� ����� ������������
	pFZFileItemData filedata = nullptr;

	for (pFZFileItemData data : m_Files)
	{
		if (data && data->name == filename)
		{
			filedata = data;
			break;
		}
	}

	if (filedata)
		filedata->requiredAction = FZ_FILE_ACTION_IGNORE;

	return true;
}

void FZFiles::SetCallback(FZFileActualizingCallback cb, void* userdata)
{
	m_pCbUserdata = userdata;
	m_pCallback = cb;
}

int FZFiles::EntriesCount()
{
	return m_Files.size();
}

pFZFileItemData FZFiles::GetEntry(u32 i)
{
	if (i < m_Files.size())
		return m_Files[i];

	return nullptr;
}

void FZFiles::DeleteEntry(u32 i)
{
	if (i >= m_Files.size())
		return;

	delete m_Files[i];
	m_Files.erase(m_Files.begin() + i);
}

void FZFiles::UpdateEntryAction(u32 i, FZFileItemAction action)
{
	if (i < m_Files.size())
		m_Files[i]->requiredAction = action;
}

void FZFiles::SetDlMode(FZDlMode mode)
{
	m_Mode = mode;
}

void FZFiles::Copy(const FZFiles &from)
{
	Clear();

	m_pCbUserdata = from.m_pCbUserdata;
	m_pCallback = from.m_pCallback;
	m_ParentPath = from.m_ParentPath;
	m_Mode = from.m_Mode;

	for (pFZFileItemData filedata2 : from.m_Files)
	{
		pFZFileItemData filedata = new FZFileItemData();
		*filedata = *filedata2;
		m_Files.push_back(filedata);
	}
}
