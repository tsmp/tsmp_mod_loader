#include "FileManager.h"
#include "Common.h"
#include <algorithm>
#include "HttpDownloader.h"
#include <shlobj_core.h>

// TODO: enable curl downloader?
// TODO: implement md5 or remove it

bool GetFileChecks(string path, FZCheckParams &OutCheckParams, bool needMD5);
bool IsDummy(const FZCheckParams &c);
FZCheckParams GetDummyChecks();
bool CompareFiles(const FZCheckParams &c1, const FZCheckParams &c2);

const char* FM_LBL = "[FM]";
extern bool g_SkipFullFileCheck;

// Создает папки по указанному пути
bool ForceDirectories(const string &path)
{
	int res = SHCreateDirectoryEx(nullptr, path.c_str(), nullptr);
	return res == ERROR_ALREADY_EXISTS || res == ERROR_SUCCESS;
}

bool _DeleteFile(string path)
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

bool FZFiles::_ScanDir(string dir_path)
{
	string name = _parent_path + dir_path + "*.*";

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
			_ScanDir(dir_path + name + '\\');
		else
		{
			_CreateFileData(dir_path + name, "", 0, GetDummyChecks());
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

pFZFileItemData FZFiles::_CreateFileData(string name, string url, u32 compression, FZCheckParams need_checks)
{
	pFZFileItemData result = new FZFileItemData();

	result->name = trim(name);
	result->url = trim(url);
	result->compression_type = compression;
	result->required_action = FZ_FILE_ACTION_UNDEFINED;
	result->target = need_checks;
	result->real = GetDummyChecks();

	_files.push_back(result);
	return result;
}

FZFiles::FZFiles()
{
	_callback = nullptr;
	_cb_userdata = nullptr;
	_mode = FZ_DL_MODE_CURL;
}

FZFiles::~FZFiles()
{
	Clear();
}

void FZFiles::Clear()
{
	_parent_path = "";
	
	for (pFZFileItemData item : _files)
		delete item;

	_files.clear();
}

void FZFiles::Dump( /*severity:FZLogMessageSeverity = FZ_LOG_INFO*/)
{
	Msg("- %s =======File list dump start=======", FM_LBL);

	for (pFZFileItemData data : _files)
	{
		if (data)
			Msg("- %s %s, action=%u, size %u (%u), crc32 = %#08x (%#08x), url=%s", FM_LBL, data->name.c_str(), data->required_action, data->real.size, data->target.size,
				data->real.crc32, data->target.crc32, data->url.c_str());
	}

	Msg("- %s =======File list dump end=======", FM_LBL);
}

bool FZFiles::ScanPath(string dir_path)
{
	Clear();
	Msg("- %s =======Scanning directory=======", FM_LBL);

	if (dir_path[dir_path.size() - 1] != '\\' && dir_path[dir_path.size() - 1] != '//')
		dir_path += '\\';

	_parent_path = dir_path;
	_ScanDir("");
	Msg("- %s =======Scanning finished=======", FM_LBL);
	return true;
}

bool FZFiles::UpdateFileInfo(string filename, string url, u32 compression_type, FZCheckParams targetParams)
{
	Msg("- %s Updating file info for %s, size = %u, crc = %#08x, url = %s, compression %u",
		FM_LBL, filename.c_str(), targetParams.size, targetParams.crc32, /* md5=[' + targetParams.md5 + ']' */  url.c_str(), compression_type);

	filename = trim(filename);

	if (filename.find("..") != string::npos)
	{
		Msg("! %s File path cannot contain ..", FM_LBL);
		return false;
	}

	//Пробуем найти файл среди существующих
	pFZFileItemData filedata = nullptr;

	for (pFZFileItemData item : _files)
	{
		if (item && item->name == filename)
		{
			filedata = item;
			break;
		}
	}

	if (!filedata)
	{
		//Файла нет в списке. Однозначно надо качать.
		filedata = _CreateFileData(filename, url, compression_type, targetParams);
		filedata->required_action = FZ_FILE_ACTION_DOWNLOAD;
		Msg("- %s Created new file list entry", FM_LBL);
	}
	else
	{
		//Файл есть в списке. Проверяем.
		if (IsDummy(filedata->real))
		{
			//посчитаем его CRC32, если ранее не считался
			if (!GetFileChecks(_parent_path + filedata->name, filedata->real, !targetParams.md5.empty()))
			{
				filedata->real.crc32 = 0;
				filedata->real.size = 0;
				filedata->real.md5 = "";
			}

			Msg("- %s Current file info: CRC32= crc = %#08x, size=%u, ", FM_LBL, filedata->real.crc32, filedata->real.size/*, md5 = [' + filedata.real.md5 + ']'*/);
		}

		if (CompareFiles(filedata->real, targetParams))
		{
			filedata->required_action = FZ_FILE_ACTION_NO;
			Msg("- %s Entry exists, file up-to-date", FM_LBL);
		}
		else
		{
			filedata->required_action = FZ_FILE_ACTION_DOWNLOAD;
			Msg("- %s Entry exists, file outdated", FM_LBL);
		}

		filedata->target = targetParams;
		filedata->url = url;
		filedata->compression_type = compression_type;
	}

	return true;
}

void FZFiles::SortBySize()
{
	if (_files.size() < 2)
		return;

	//сортируем так, чтобы самые большие файлы оказались в конце
	for (int i = _files.size() - 1; i > 0; i--)
	{
		int max = i;

		//Ищем самый большой файл
		for (int j = i - 1; j >= 0; j--)
		{
			if (!_files[j])
				continue;

			if (!_files[max] || _files[max]->target.size < _files[i]->target.size)
				max = j;
		}

		if (i != max)
		{
			pFZFileItemData temp = _files[i];
			_files[i] = _files[max];
			_files[max] = temp;
		}
	}
}

bool FZFiles::ActualizeFiles()
{
	const u32 MAX_ACTIVE_DOWNLOADERS = 4;  //for safety

	long long total_dl_size, total_actual_size, downloaded_now, downloaded_total;
	total_dl_size = total_actual_size = downloaded_total = 0;

	for (u32 i = 0; i < _files.size(); i++)
	{
		pFZFileItemData filedata = _files[i];

		if (!filedata)
			continue;

		if (filedata->required_action == FZ_FILE_ACTION_UNDEFINED)
		{
			//такого файла не было в списке. Сносим.
			Msg("- %s Deleting file %s", FM_LBL, filedata->name.c_str());

			if (!_DeleteFile(_parent_path + filedata->name))
			{
				Msg("! %s Failed to delete ", FM_LBL, filedata->name.c_str());
				return false;
			}

			delete filedata;
			_files.erase(_files.begin() + i);
		}
		else if (filedata->required_action == FZ_FILE_ACTION_DOWNLOAD)
		{
			total_dl_size = total_dl_size + filedata->target.size;
			string str = _parent_path + filedata->name;

			while (str[str.size() - 1] != '\\' && str[str.size() - 1] != '//')
				str.resize(str.size() - 1);

			if (!ForceDirectories(str))
			{
				Msg("! %s Cannot create directory %s", FM_LBL, str.c_str());
				return false;
			}
		}
		else if (filedata->required_action == FZ_FILE_ACTION_NO)
		{
			total_actual_size = total_actual_size + filedata->target.size;
		}
		else if (filedata->required_action == FZ_FILE_ACTION_IGNORE)
		{
			Msg("- %s Ignoring file %s", FM_LBL, filedata->name.c_str());
		}
		else
		{
			Msg("! %s Unknown action for %s", FM_LBL, filedata->name.c_str());
			return false;
		}
	}

	Msg("- %s Total up-to-date size %u", FM_LBL, total_actual_size);
	Msg("- %s Total downloads size %u", FM_LBL, total_dl_size);

	Msg("- %s Starting downloads", FM_LBL);
	bool result = true;

	//Вызовем колбэк для сообщения о начале стадии загрузки
	FZFileActualizingProgressInfo cb_info;
	cb_info.total_downloaded = 0;
	cb_info.status = FZ_ACTUALIZING_BEGIN;
	cb_info.estimated_dl_size = total_dl_size;
	cb_info.total_up_to_date_size = total_actual_size;
	cb_info.total_mod_size = total_dl_size + total_actual_size;

	if (_callback)
		result = _callback(cb_info, _cb_userdata);

	FZDownloaderThread* thread;

	//Начнем загрузку
	//if (_mode==FZ_DL_MODE_GAMESPY)
	thread = new FZGameSpyDownloaderThread();
	//else
	//    thread=new FZCurlDownloaderThread();

	int last_file_index = _files.size() - 1;

	vector<FZFileDownloader*> downloaders;
	downloaders.reserve(MAX_ACTIVE_DOWNLOADERS);

	for (int i = 0; i < MAX_ACTIVE_DOWNLOADERS; i++)
		downloaders.push_back(nullptr);

	bool finished = false;
	downloaded_total = 0;
	cb_info.status = FZ_ACTUALIZING_IN_PROGRESS;

	while (!finished)
	{
		if (!result)
		{
			//Загрузка прервана.
			Msg("! %s Actualizing cancelled", FM_LBL);
			break;
		}

		finished = true; //флаг сбросится при активной загрузке
		downloaded_now = 0;

		//смотрим на текущий статус слотов загрузки
		for (u32 i = 0; i < downloaders.size(); i++)
		{
			if (!downloaders[i])
			{
				//Слот свободен. Поставим туда что-нибудь, если найдем
				while (last_file_index >= 0)
				{
					pFZFileItemData filedata = _files[last_file_index];
					last_file_index = last_file_index - 1; //сдвигаем индекс на необработанный файл

					if (filedata && filedata->required_action == FZ_FILE_ACTION_DOWNLOAD)
					{
						//файл для загрузки найден, помещаем его в слот.
						Msg("- %s Starting download of %s", FM_LBL, filedata->url.c_str());
						finished = false;

						downloaders[i] = thread->CreateDownloader(filedata->url, _parent_path + filedata->name, filedata->compression_type);
						result = downloaders[i]->StartAsyncDownload();

						if (!result)
							Msg("! %s Cannot start download for %s", FM_LBL, filedata->url.c_str());
						else
							filedata->required_action = FZ_FILE_ACTION_VERIFY;

						break;
					}
				}
			}
			else if (downloaders[i]->IsBusy())
			{
				//Слот активен. Обновим информацию о прогрессе
				finished = false;
				downloaded_now = downloaded_now + downloaders[i]->DownloadedBytes();
			}
			else
			{
				//Слот завершил работу. Освободим его.
				Msg("- %s Need free slot contained %s", FM_LBL, downloaders[i]->GetFilename().c_str());
				result = downloaders[i]->IsSuccessful();
				downloaded_total = downloaded_total + downloaders[i]->DownloadedBytes();

				if (!result)
					Msg("! %s Download failed for %s", FM_LBL, downloaders[i]->GetFilename());;


				delete downloaders[i];
				downloaders[i] = nullptr;

				//но могут быть еще файлы в очереди на загрузку, поэтому, не спешим выставлять в true
				finished = false;
			}
		}

		//Вызовем колбэк прогресса
		cb_info.total_downloaded = downloaded_now + downloaded_total;
		if (_callback)
			result = _callback(cb_info, _cb_userdata);

		if (result)
			Sleep(100);
	}

	//Останавливаем всех их
	Msg("- %s Request stop", FM_LBL);

	for (FZFileDownloader* loader : downloaders)
	{
		if (loader)
			loader->RequestStop();
	}

	//и удаляем их
	Msg("- %s Delete downloaders", FM_LBL);
	for (FZFileDownloader* loader : downloaders)
		delete loader;

	downloaders.clear();
	delete thread;

	if (result)
	{
		//убедимся, что все требуемое скачалось корректно
		if (total_dl_size > 0)
		{
			Msg("- %s Verifying downloaded", FM_LBL);

			cb_info.status = FZ_ACTUALIZING_VERIFYING_START;
			cb_info.total_downloaded = 0;
			result = _callback(cb_info, _cb_userdata);

			for (pFZFileItemData filedata : _files)
			{
				if (!result)
					break;

				if (filedata && filedata->required_action == FZ_FILE_ACTION_VERIFY)
				{
					if (GetFileChecks(_parent_path + filedata->name, filedata->real, !filedata->target.md5.empty()))
					{
						if (!CompareFiles(filedata->real, filedata->target))
						{
							Msg("! %s File NOT synchronized: %s", FM_LBL, filedata->name.c_str());
							filedata->required_action = FZ_FILE_ACTION_DOWNLOAD;
							result = false;

						}
					}
					else
					{
						Msg("! %s Cannot check %s", FM_LBL, filedata->name.c_str());

						filedata->required_action = FZ_FILE_ACTION_DOWNLOAD;
						result = false;
					}
				}
				else if (filedata && filedata->required_action == FZ_FILE_ACTION_DOWNLOAD)
				{
					Msg("! %s File %s has FZ_FILE_ACTION_DOWNLOAD state after successful synchronization??? A bug suspected!", FM_LBL, filedata->name.c_str());
					result = false;
				}

				if (result)
				{
					cb_info.status = FZ_ACTUALIZING_VERIFYING;
					cb_info.total_downloaded = cb_info.total_downloaded + filedata->target.size;
					result = _callback(cb_info, _cb_userdata);
				}
			}
		}

		//вызываем колбэк окончания
		if (result)
		{
			Msg("- %s Run finish callback", FM_LBL);
			cb_info.status = FZ_ACTUALIZING_FINISHED;
			cb_info.total_downloaded = downloaded_total;
			result = _callback(cb_info, _cb_userdata);
		}
		else
		{
			Msg("- %s Run fail callback", FM_LBL);
			cb_info.status = FZ_ACTUALIZING_FAILED;
			cb_info.total_downloaded = 0;
			_callback(cb_info, _cb_userdata);
		}

		Msg("- %s All downloads finished", FM_LBL);
	}

	return result;
}

bool FZFiles::AddIgnoredFile(string filename)
{
	//Пробуем найти файл среди существующих
	pFZFileItemData filedata = nullptr;

	for (pFZFileItemData data : _files)
	{
		if (data && data->name == filename)
		{
			filedata = data;
			break;
		}
	}

	if (filedata)
		filedata->required_action = FZ_FILE_ACTION_IGNORE;

	return true;
}

void FZFiles::SetCallback(FZFileActualizingCallback cb, void* userdata)
{
	_cb_userdata = userdata;
	_callback = cb;
}

int FZFiles::EntriesCount()
{
	return _files.size();
}

pFZFileItemData FZFiles::GetEntry(u32 i)
{
	if (i < _files.size())
		return _files[i];

	return nullptr;
}

void FZFiles::DeleteEntry(u32 i)
{
	if (i >= _files.size())
		return;

	delete _files[i];
	_files.erase(_files.begin() + i);
}

void FZFiles::UpdateEntryAction(u32 i, FZFileItemAction action)
{
	if (i < _files.size())
		_files[i]->required_action = action;
}

void FZFiles::SetDlMode(FZDlMode mode)
{
	_mode = mode;
}

void FZFiles::Copy(const FZFiles &from)
{
	Clear();

	_cb_userdata = from._cb_userdata;
	_callback = from._callback;
	_parent_path = from._parent_path;
	_mode = from._mode;

	for (pFZFileItemData filedata2 : from._files)
	{
		pFZFileItemData filedata = new FZFileItemData();
		*filedata = *filedata2;
		_files.push_back(filedata);
	}
}
