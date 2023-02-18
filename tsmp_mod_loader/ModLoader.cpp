#include "Common.h"
#include "lowlevel/Abstractions.h"
#include "FileManager.h"
#include "HttpDownloader.h"
#include "CommandLineParser.h"
#include "IniFile.h"
#include <shlwapi.h>
#include <algorithm>
#include <fstream>

#pragma comment(lib, "shlwapi.lib")

using std::endl;

extern bool WinapiDownloadFile(const char* url, const char* path);
extern bool ForceDirectories(const string &path);

extern std::string g_ModParams;
using FZMasterLinkListAddr = vector<string>;

bool g_SkipFullFileCheck = false;

struct FZFsLtxBuilderSettings
{
	bool sharePatchesDir;
	bool fullInstall;
	string configsDir;
};

struct FZModSettings
{
	string rootDir;
	string exeName;
	string modName;
	string binlistUrl;
	string gamelistUrl;
	FZFsLtxBuilderSettings fsltxSettings;
};

struct FZModMirrorsSettings
{
	FZMasterLinkListAddr binlistUrls;
	FZMasterLinkListAddr gamelistUrls;
};

struct FZConfigBackup
{
	string fileName;
	string buf;
	u32 sz;
};

const char* GamedataFilesListName = "gamedata_filelist.ini";
const char* EngineFilesListName = "engine_filelist.ini";
const char* MasterModsListName = "master_mods_filelist.ini";
const char* FsltxName = "fsgame.ltx";
const char* UserltxName = "user.ltx";
const char* UserdataDirName = "userdata\\";
const char* EngineDirName = "bin\\";

string BoolToStr(bool b)
{
	return b ? "true" : "false";
}

bool FileExists(const string &path)
{
	return PathFileExists(path.c_str());
}

FZDownloaderThread* CreateDownloaderThreadForUrl(string url)
{
	const char* httpsStr = "https";

	// TODO: CURL
	//if (IsGameSpyDlForced(g_ModParams) && strncmp(url.c_str(), httpsStr, strlen(httpsStr)))
	{
		Msg("- Creating GS dl thread");
		return new FZGameSpyDownloaderThread();
	}

	//Msg("Creating CURL dl thread");
	//return new FZCurlDownloaderThread();
}

string GenerateMirrorSuffixForIndex(int index)
{
	const char* MASTERLINKS_MIRROR_SUFFIX = "_mirror_";

	if (index > 0)
		return MASTERLINKS_MIRROR_SUFFIX + std::to_string(index);

	return "";
}

enum FZMasterListApproveType
{
	FZ_MASTERLIST_NOT_APPROVED,
	FZ_MASTERLIST_APPROVED,
	FZ_MASTERLIST_ONLY_OLD_CONFIG
};

FZMasterListApproveType DownloadAndParseMasterModsList(FZModSettings &settings, FZModMirrorsSettings &mirrors)
{
	const char* DEBUG_MODE_KEY = "-fz_customlists";
	const char* MASTERLINKS_BINLIST_KEY = "binlist";
	const char* MASTERLINKS_GAMELIST_KEY = "gamelist";
	const char* MASTERLINKS_FULLINSTALL_KEY = "fullinstall";
	const char* MASTERLINKS_SHARED_PATCHES_KEY = "sharedpatches";
	const char* MASTERLINKS_CONFIGS_DIR_KEY = "configsdir";
	const char* MASTERLINKS_EXE_NAME_KEY = "exename";

	FZMasterListApproveType result = FZ_MASTERLIST_NOT_APPROVED;

	std::string masterLinks[]
	{
		"http://stalker-life.com/stalker_files/mods_shoc/tsmp3/ModMasterLinks.txt",
		"https://raw.githubusercontent.com/FreeZoneMods/modmasterlinks/master/links.ini", // TODO: решить вопрос с https
		"http://www.gwrmod.tk/files/mods_links.ini",
		"http://www.stalker-life.ru/mods_links/links.ini",
		"http://stalker.stagila.ru:8080/stcs_emergency/mods_links.ini",
		"http://www.gwrmod.tk/files/mods_links_low_priority.ini"
	};

	bool listDownloaded = false;
	string fullPath = settings.rootDir + MasterModsListName;
	FZDownloaderThread* dlThread = CreateDownloaderThreadForUrl(masterLinks[0]);

	for (std::string &masterLink: masterLinks)
	{
		FZFileDownloader* dl = dlThread->CreateDownloader(masterLink, fullPath, 0);
		listDownloaded = dl->StartSyncDownload();
		delete dl;

		if (listDownloaded)
			break;
	}

	delete dlThread;

	// TODO: fix or remove
	//for (u32 i = 0; i < master_links.size(); i++)
	//{
	//	list_downloaded = WinapiDownloadFile(master_links[i].c_str(), full_path.c_str());

	//	if (list_downloaded)
	//		break;
	//}

	FZModSettings tmpSettings = settings;
	tmpSettings.binlistUrl = GetCustomBinUrl(g_ModParams);
	tmpSettings.gamelistUrl = GetCustomGamedataUrl(g_ModParams);
	tmpSettings.exeName = GetExeName(g_ModParams, "");
	tmpSettings.fsltxSettings.fullInstall = IsFullInstallMode(g_ModParams);
	tmpSettings.fsltxSettings.sharePatchesDir = IsSharedPatches(g_ModParams);
	tmpSettings.fsltxSettings.configsDir = GetConfigsDir(g_ModParams, "");

	FZMasterListApproveType paramsApproved = FZ_MASTERLIST_NOT_APPROVED;	

	if (listDownloaded)
	{
		string tmp1, tmp2;
		bool binlistValid, gamelistValid;
		int j;

		// Мастер-список успешно скачался, будем парсить его содержимое
		FZIniFile cfg(fullPath);
		for (int i = 0; i < cfg.GetSectionsCount(); i++)
		{
			if (cfg.GetSectionName(i) == tmpSettings.modName)
			{
				//Нашли в мастер-конфиге секцию, отвечающую за наш мод
				Msg("Mod %s found in master list", settings.modName.c_str());
				paramsApproved = FZ_MASTERLIST_NOT_APPROVED;

				//Заполняем список всех доступных зеркал, попутно ищем ссылки из binlist и gamelist в списке зеркал
				j = 0;
				if (tmpSettings.gamelistUrl.empty() && tmpSettings.binlistUrl.empty())
				{

					//Юзер не заморачивается указыванием списков, выбираем их сами
					binlistValid = true;
					gamelistValid = true;
				}
				else
				{
					binlistValid = false;
					gamelistValid = false;
				}

				while (true)
				{
					//Конструируем имя параметров зеркала
					tmp1 = GenerateMirrorSuffixForIndex(j);
					tmp2 = MASTERLINKS_GAMELIST_KEY + tmp1;
					tmp1 = MASTERLINKS_BINLIST_KEY + tmp1;

					//Вычитываем параметры зеркала
					tmp1 = cfg.GetStringDef(tmpSettings.modName, tmp1, "");
					tmp2 = cfg.GetStringDef(tmpSettings.modName, tmp2, "");

					//Проверяем и сохраняем
					if (tmp1.empty() && tmp2.empty())
						break;

					Msg("Pushing mirror %u : binlist %s, gamelist", mirrors.binlistUrls.size(), tmp1.c_str(), tmp2.c_str());

					mirrors.binlistUrls.push_back(tmp1);
					mirrors.gamelistUrls.push_back(tmp2);

					if (tmp1 == tmpSettings.binlistUrl)
						binlistValid = true;
					if (tmp2 == tmpSettings.gamelistUrl)
						gamelistValid = true;

					j = j + 1;
				}

				//Убеждаемся, что пользователь не подсунул нам "левую" ссылку
				if (mirrors.binlistUrls.empty() && mirrors.gamelistUrls.empty())
				{
					Msg("! Invalid mod parameters in master links");
					break;
				}

				if (!binlistValid)
				{
					Msg("! The binlist URL specified in mod parameters cant be found in the master links list");
					break;
				}

				if (!gamelistValid)
				{
					Msg("! The gamelist URL specified in mod parameters cant be found in the master links list");
					break;
				}

				//Если пользователь не передал нам в строке запуска ссылок - берем указанные в "основном" зеркале
				if (tmpSettings.gamelistUrl.empty() && tmpSettings.binlistUrl.empty())
				{
					if (!mirrors.gamelistUrls.empty())
						tmpSettings.gamelistUrl = mirrors.gamelistUrls[0];


					if (!mirrors.binlistUrls.empty())
						tmpSettings.binlistUrl = mirrors.binlistUrls[0];

					for (u32 j = 0; j < mirrors.binlistUrls.size() - 1; j++)
					{
						mirrors.gamelistUrls[j] = mirrors.gamelistUrls[j + 1];
						mirrors.binlistUrls[j] = mirrors.binlistUrls[j + 1];
					}

					// TODO: разобраться что тут было
					//mirrors.gamelistUrls.resize(mirrors.gamelistUrls.size() - 1);
					//mirrors.binlistUrls.resize(mirrors.binlistUrls.size() - 1);
				}

				tmpSettings.fsltxSettings.fullInstall = cfg.GetBoolDef(tmpSettings.modName, MASTERLINKS_FULLINSTALL_KEY, false);
				tmpSettings.fsltxSettings.sharePatchesDir = cfg.GetBoolDef(tmpSettings.modName, MASTERLINKS_SHARED_PATCHES_KEY, false);
				tmpSettings.fsltxSettings.configsDir = cfg.GetStringDef(tmpSettings.modName, MASTERLINKS_CONFIGS_DIR_KEY, "");
				tmpSettings.exeName = cfg.GetStringDef(tmpSettings.modName, MASTERLINKS_EXE_NAME_KEY, "");
				paramsApproved = FZ_MASTERLIST_APPROVED;
				break;
			}
			else if (paramsApproved == FZ_MASTERLIST_NOT_APPROVED)
			{
				//Если ссылка на binlist пустая или находится в конфиге какого-либо мода - можно заапрувить ее
				//Однако заканчивать рано - надо перебирать и проверять также следующие секции, так как в них может найтись секция с модом, в которой будут другие движок и/или геймдата!
				binlistValid = tmpSettings.binlistUrl.empty();
				j = 0;
				tmp2 = cfg.GetSectionName(i);

				while (!binlistValid)
				{
					tmp1 = cfg.GetStringDef(tmp2, MASTERLINKS_BINLIST_KEY + GenerateMirrorSuffixForIndex(j), "");

					if (tmp1.empty() && cfg.GetStringDef(tmp2, MASTERLINKS_GAMELIST_KEY + GenerateMirrorSuffixForIndex(j), "").empty())
						break;
					
					binlistValid = tmp1 == tmpSettings.binlistUrl;
					j = j + 1;
				}

				if (binlistValid)
				{
					if (tmpSettings.binlistUrl.empty())
						Msg("No engine mod, approved");
					else
						Msg("Engine %s approved by mod %s", tmpSettings.binlistUrl.c_str(), cfg.GetSectionName(i).c_str());

					paramsApproved = FZ_MASTERLIST_APPROVED;
				}
			}
		}
	}
	else
	{
		//Список почему-то не скачался. Ограничимся геймдатными и скачанными ранее модами.
		Msg("! Cannot download master links!");

		if (tmpSettings.binlistUrl.empty() && !tmpSettings.gamelistUrl.empty())
		{
			Msg("Gamedata mod approved");
			paramsApproved = FZ_MASTERLIST_APPROVED;
		}
		else
		{
			paramsApproved = FZ_MASTERLIST_ONLY_OLD_CONFIG;
			Msg("Only old mods approved");
		}
	}

	string coreParams = VersionAbstraction()->GetCoreParams();

	if (paramsApproved != FZ_MASTERLIST_NOT_APPROVED || coreParams.find(DEBUG_MODE_KEY) != string::npos)
	{
		settings = tmpSettings;

		std::replace(settings.exeName.begin(), settings.exeName.end(), '\\', '_');
		std::replace(settings.exeName.begin(), settings.exeName.end(), '/', '_');

		if (coreParams.find(DEBUG_MODE_KEY) != string::npos)
		{
			Msg("Debug mode - force approve");
			result = FZ_MASTERLIST_APPROVED;
		}
		else
			result = paramsApproved;
	}

	return result;
}

bool DownloadAndApplyFileList(const string &url, const string &listFilename, const string &root_dir, FZMasterListApproveType masterlinksType, FZFiles &fileList, bool updateProgress)
{	
	const u32 MAX_NO_UPDATE_DELTA = 1000;
	string filepath = root_dir + listFilename;

	if (masterlinksType == FZ_MASTERLIST_ONLY_OLD_CONFIG)
	{
		//Не загружаем ничего! В параметрах URL вообще может ничего не быть
		//Просто пытаемся использовать старую конфигурацию
	}
	else if (masterlinksType == FZ_MASTERLIST_NOT_APPROVED)
	{
		Msg("! Master links dont allow the mod");
		return false;
	}
	else
	{
		if (url.empty())
		{
			Msg("! No list URL specified");
			return false;
		}

		Msg("Downloading list %s to %s", url.c_str(), filepath.c_str());

		FZDownloaderThread* thread = CreateDownloaderThreadForUrl(url);
		FZFileDownloader* dl = thread->CreateDownloader(url, filepath, 0);
		const bool result = dl->StartSyncDownload();
		delete dl;
		delete thread;

		if (!result)
		{
			Msg("! Downloading list failed");
			return false;
		}
	}

	FZIniFile cfg(filepath);
	int filesCount = cfg.GetIntDef("main", "files_count", 0);

	if (!filesCount)
	{
		Msg("! No files in file list");
		return false;
	}

	const u32 startTime = GetCurrentTime();
	u32 lastUpdateTime = startTime;

	for (int i = 0; i < filesCount; i++)
	{
		string section = "file_" + std::to_string(i);
		Msg("- Parsing section %s", section.c_str());
		string fileName = cfg.GetStringDef(section, "path", "");

		if (fileName.empty())
		{
			Msg("! Invalid name for file %u", i);
			return false;
		}

		if (cfg.GetBoolDef(section, "ignore", false))
		{
			if (!fileList.AddIgnoredFile(fileName))
			{
				Msg("! Cannot add to ignored file %u (%s)", i, fileName.c_str());
				return false;
			}
		}
		else
		{
			string fileurl = cfg.GetStringDef(section, "url", "");

			if (fileurl.empty())
			{
				Msg("! Invalid url for file %u", i);
				return false;
			}

			FZCheckParams fileCheckParams;
			fileCheckParams.crc32 = 0;
			const u32 compression = cfg.GetIntDef(section, "compression", 0);

			if (!cfg.GetHex(section, "crc32", fileCheckParams.crc32))
			{
				Msg("! Invalid crc32 for file %u", i);
				return false;
			}

			fileCheckParams.size = cfg.GetIntDef(section, "size", 0);

			if (!fileCheckParams.size)
			{
				Msg("! Invalid size for file %u", i);
				return false;
			}

			//fileCheckParams.md5 = LowerCase(cfg.GetStringDef(section, "md5", ""));

			if (!fileList.UpdateFileInfo(fileName, fileurl, compression, fileCheckParams))
			{
				Msg("! Cannot update file info %u (%s)", i, fileName.c_str());
				return false;
			}
		}

		if (VersionAbstraction()->CheckForUserCancelDownload())
		{
			Msg("! Stop applying file list - user-cancelled");
			return false;
		}

		if (updateProgress)
		{
			if (GetCurrentTime() - lastUpdateTime > MAX_NO_UPDATE_DELTA)
			{
				VersionAbstraction()->SetVisualProgress(static_cast<float>(100 * i) / filesCount);
				lastUpdateTime = GetCurrentTime();
			}
		}
	}

	Msg("- File list %s processed, time %u ms", listFilename.c_str(), GetCurrentTime() - startTime);
	return true;
}

bool DownloadCallback(const FZFileActualizingProgressInfo &info, void* userdata)
{
	float progress = 0;

	if (info.totalModSize > 0)
	{
		long long ready = info.totalDownloaded + info.totalUpToDateSize;

		if (ready > 0)
			progress = (static_cast<float>(ready) / info.totalModSize) * 100;
	}

	auto lastDownloadedBytes = reinterpret_cast<long long*>(userdata);

	if (*lastDownloadedBytes != info.totalDownloaded)
	{
		if (info.status != FZ_ACTUALIZING_VERIFYING_START && info.status != FZ_ACTUALIZING_VERIFYING)
		{
			//Msg("Downloaded %lld, state %u", info.totalDownloaded, static_cast<u32>(info.status));
		}
		else
		{
			if (info.status == FZ_ACTUALIZING_VERIFYING_START)
				VersionAbstraction()->AssignStatus("Verifying downloaded content...");

			//Msg("Verified %lld, state %u", info.totalDownloaded, static_cast<u32>(info.status));
		}

		*lastDownloadedBytes = info.totalDownloaded;
	}

	VersionAbstraction()->SetVisualProgress(progress);
	return !VersionAbstraction()->CheckForUserCancelDownload();
}

bool BuildFsGameInternal(const string &fileName, const FZFsLtxBuilderSettings &settings)
{
	std::ofstream f;
	f.open(fileName);

	if (!f.is_open())
		return false;

	f << "$mnt_point$=false|false|$fs_root$|gamedata\\" << endl;
	f << "$app_data_root$=false |false |$fs_root$|" << UserdataDirName << endl;
	f << "$parent_app_data_root$=false |false|" << VersionAbstraction()->UpdatePath("$app_data_root$", "") << endl;
	f << "$parent_game_root$=false|false|" << VersionAbstraction()->UpdatePath("$fs_root$", "") << endl;
	f << "$game_data$=false|true|$fs_root$|gamedata\\" << endl;
	f << "$game_ai$=true|false|$game_data$|ai\\" << endl;
	f << "$game_spawn$=true|false|$game_data$|spawns\\" << endl;
	f << "$game_levels$=true|false|$game_data$|levels\\" << endl;
	f << "$game_meshes$=true|true|$game_data$|meshes\\|*.ogf;*.omf|Game Object files" << endl;
	f << "$game_anims$=true|true|$game_data$|anims\\|*.anm;*.anms|Animation files" << endl;
	f << "$game_dm$=true|true|$game_data$|meshes\\|*.dm|Detail Model files" << endl;
	f << "$game_shaders$=true|true|$game_data$|shaders\\" << endl;
	f << "$game_sounds$=true|true|$game_data$|sounds\\" << endl;
	f << "$game_textures$=true|true|$game_data$|textures\\" << endl;

	if (!settings.configsDir.empty())
		f << "$game_config$=true|false|$game_data$|" + settings.configsDir + "\\" << endl;
	else
		f << "$game_config$=true|false|$game_data$|config\\" << endl;

	f << "$game_weathers$=true|false|$game_config$|environment\\weathers" << endl;
	f << "$game_weather_effects$=true|false|$game_config$|environment\\weather_effects" << endl;
	f << "$textures$=true|true|$game_data$|textures\\" << endl;
	f << "$level$=false|false|$game_levels$" << endl;
	f << "$game_scripts$=true|false|$game_data$|scripts\\|*.script|Game script files" << endl;
	f << "$logs$=true|false|$app_data_root$|logs\\" << endl;
	f << "$screenshots$=true|false|$app_data_root$|screenshots\\" << endl;
	f << "$game_saves$=true|false|$app_data_root$|savedgames\\" << endl;
	f << "$mod_dir$=false|false|$fs_root$|mods\\" << endl;
	f << "$downloads$=false|false|$app_data_root$" << endl;
	return true;
}

bool BuildFsGame(const string& filename, const FZFsLtxBuilderSettings& settings)
{
	VersionAbstraction()->AssignStatus("Building fsltx...");
	Msg("Building fsltx");
	VersionAbstraction()->SetVisualProgress(100);

	if (!BuildFsGameInternal(filename, settings))
	{
		Msg("! Building fsltx failed!");
		return false;
	}

	return true;
}

bool CopyFileIfValid(const string &srcPath, const string &dstPath, const FZCheckParams &targetParams)
{
	FZCheckParams fileCheckParams;

	if (!GetFileChecks(srcPath, fileCheckParams, !targetParams.md5.empty()))
		return false;
	
	if (!CompareFiles(fileCheckParams, targetParams))
	{
		Msg("! Checksum or size not equal to target");
		return false;
	}

	string dstDir = dstPath;

	while (dstDir[dstDir.size() - 1] != '\\' && dstDir[dstDir.size() - 1] != '/')
		dstDir.resize(dstDir.size() - 1);

	ForceDirectories(dstDir);

	if (!CopyFile(srcPath.c_str(), dstPath.c_str(), false))
	{
		Msg("! Cannot copy file %s to %s", srcPath.c_str(), dstPath.c_str());
		return false;
	}

	Msg(" Copied %s to %s", srcPath.c_str(), dstPath.c_str());
	return true;
}

void PreprocessFiles(FZFiles &files, const string &modRoot)
{
	const char* NO_PRELOAD = "-fz_nopreload";

	// TODO: what is it?
	bool disablePreload = true; // VersionAbstraction()->GetCoreParams().find(NO_PRELOAD) != string::npos;

	files.AddIgnoredFile(GamedataFilesListName);
	files.AddIgnoredFile(EngineFilesListName);
	u32 userdataDirStrLen = strlen(UserdataDirName);
	u32 engineDirStrLen = strlen(EngineDirName);

	for (int i = files.EntriesCount() - 1; i >= 0; i--)
	{
		string filename, src, dst;
		pFZFileItemData e = files.GetEntry(i);

		if (!strncmp(e->name.c_str(), UserdataDirName, userdataDirStrLen) && e->requiredAction == FZ_FILE_ACTION_UNDEFINED)
		{
			//спасаем файлы юзердаты от удаления
			files.UpdateEntryAction(i, FZ_FILE_ACTION_IGNORE);
		}
		else if (!strncmp(e->name.c_str(), EngineDirName, engineDirStrLen) && e->requiredAction == FZ_FILE_ACTION_DOWNLOAD)
		{
			if (!disablePreload)
			{
				//Проверим, есть ли уже такой файл в текущем движке
				string core_root = VersionAbstraction()->GetCoreApplicationPath();
				filename = e->name;
				filename.erase(0, engineDirStrLen);
				src = core_root + filename;
				dst = modRoot + e->name;

				if (CopyFileIfValid(src, dst, e->target))
					files.UpdateEntryAction(i, FZ_FILE_ACTION_NO);
			}
		}
	}
}

FZConfigBackup CreateConfigBackup(const string &filename)
{
	const u32 MAX_LEN = 1 * 1024 * 1024;

	FZConfigBackup result;
	result.sz = 0;

	if (filename.empty())
		return result;

	HANDLE f = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	Msg("Backup config %s, handle %u", filename.c_str(), reinterpret_cast<u32>(f));

	if (f == INVALID_HANDLE_VALUE)
	{
		Msg("! Error opening file %s", filename.c_str());
		return result;
	}

	//__try
	{
		DWORD readcnt = 0;

		u32 sz = GetFileSize(f, nullptr);
		Msg("Size of the config file %s is %u", filename.c_str(), sz);

		if (sz > MAX_LEN)
		{
			Msg("! Config %s is too large, skip it", filename.c_str());
			return result;
		}

		result.buf.reserve(sz + 1);

		if (!ReadFile(f, result.buf.data(), sz, &readcnt, nullptr) || sz != readcnt)
		{
			Msg("! Error reading file %s", filename.c_str());
			return result;
		}

		result.sz = sz;
		result.fileName = filename;
	}
	//__except (EXCEPTION_EXECUTE_HANDLER)
	//{
		//Msg("! Something went wrong :(");
		//result.fileName = "";
		//result.buf = "";
		//result.sz = 0;
	//}

	CloseHandle(f);
	return result;
}

bool FreeConfigBackup(const FZConfigBackup &backup, bool needRestore)
{
	if (!needRestore)
		return false;

	bool result = false;
	HANDLE f = CreateFile(backup.fileName.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	Msg("Restoring backup config %s, handle %u", backup.fileName.c_str(), reinterpret_cast<u32>(f));

	if (f == INVALID_HANDLE_VALUE)
		Msg("! Error opening file %s", backup.fileName.c_str());
	else
	{
		DWORD writecnt = 0;

		if (!WriteFile(f, backup.buf.data(), backup.sz, &writecnt, nullptr) || backup.sz != writecnt)
			Msg("! Error writing file %s", backup.fileName.c_str());
		else
			result = true;

		CloseHandle(f);
	}

	return result;
}

void BuildUserLtx(const FZModSettings &settings)
{
	VersionAbstraction()->AssignStatus("Building userltx...");

	//если user.ltx отсутствует в userdata - нужно сделать его там
	if (!FileExists(settings.rootDir + UserdataDirName + UserltxName))
	{
		Msg("Building userltx");
		//в случае с SACE команда на сохранение не срабатывает, поэтому сначала скопируем файл
		string dstName = settings.rootDir + UserdataDirName;
		ForceDirectories(dstName);
		dstName = dstName + UserltxName;
		string srcname = VersionAbstraction()->UpdatePath("$app_data_root$", "user.ltx");
		Msg("Copy from %s to %s", srcname.c_str(), dstName.c_str());
		CopyFile(srcname.c_str(), dstName.c_str(), false);
		VersionAbstraction()->ExecuteConsoleCommand("cfg_save " + dstName);
	}
}

void ShowMessageBox()
{
	if (!ForceShowMessage(g_ModParams))
		return;

	const bool messageInitiallyShown = VersionAbstraction()->IsMessageActive();
	Msg("Initial message status is %s", BoolToStr(messageInitiallyShown).c_str());	

	if (messageInitiallyShown)
	{
		//Ждем, пока исчезнет сообщение о коннекте к мастер-серверу
		while (VersionAbstraction()->IsServerListUpdateActive())
			Sleep(1);
	}

	//Дождемся подходящего для показа момента
	Msg("Prepare for message displaying");
	VersionAbstraction()->PrepareForMessageShowing();

	//Включим его обратно
	Msg("Activating message");
	VersionAbstraction()->TriggerMessage();
}

bool RunClient(const FZModSettings &settings)
{
	//Надо стартовать игру с модом
	VersionAbstraction()->AssignStatus("Running game...");	
	string ip = GetServerIp(g_ModParams);

	if (ip.empty())
	{
		Msg("! Cannot determine IP address of the server");
		return false;
	}

	//Подготовимся к перезапуску
	Msg("Prepare to restart client");
	int port = GetServerPort(g_ModParams);

	if (port < 0 || port > 65535)
	{
		Msg("! Cannot determine port");
		return false;
	}

	string playerName;

	if (IsCmdLineNameNameNeeded(g_ModParams))
	{
		playerName = VersionAbstraction()->GetPlayerName();
		Msg("Using player name %s", playerName.c_str());
		playerName = "/name=" + playerName;
	}

	string psw = GetPassword(g_ModParams);
	string cmdApp, cmdLine, workingDir;

	if (!psw.empty())
		psw = "/psw=" + psw;

	if (!settings.binlistUrl.empty())
	{
		// Нестандартный двиг мода
		cmdApp = settings.rootDir + "bin\\";

		if (!settings.exeName.empty())
		{
			cmdApp = cmdApp + settings.exeName;
			cmdLine = settings.exeName;
		}
		else
		{
			cmdApp = cmdApp + VersionAbstraction()->GetEngineExeFileName();
			cmdLine = VersionAbstraction()->GetEngineExeFileName();
		}

		cmdLine = cmdLine + " -fz_nomod -fzmod -start client(" + ip + "/port=" + std::to_string(port) + playerName + psw + ')';
		workingDir = settings.rootDir;
	}

	//Точка невозврата. Убедимся, что пользователь не отменил загрузку
	if (VersionAbstraction()->CheckForUserCancelDownload())
	{
		Msg("! Cancelled by user");
		return false;
	}

	Msg("cmdapp: %s", cmdApp.c_str());
	Msg("cmdline: %s", cmdLine.c_str());

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	FillMemory(&si, sizeof(si), 0);
	FillMemory(&pi, sizeof(pi), 0);
	si.cb = sizeof(si);

	// Запустим клиента
	if (!CreateProcess(cmdApp.c_str(), cmdLine.data(), 0, 0, false, CREATE_SUSPENDED, 0, workingDir.c_str(), &si, &pi))
	{
		Msg("! Cannot run application");
		return false;
	}

	ResumeThread(pi.hThread);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
}

bool GetFileLists(FZFiles &filesCp, FZFiles& files, FZModSettings &modSettings, FZMasterListApproveType masterlinksParseResult, FZModMirrorsSettings &mirrors)
{
	int mirrorId = 0;
	bool flag;

	do
	{
		files.Copy(filesCp);

		//Загрузим с сервера требуемую конфигурацию корневой директории и сопоставим ее с текущей
		Msg("- =======Processing URLs combination %d =======", mirrorId);
		Msg("binlist: %s", modSettings.binlistUrl.c_str());
		Msg("gamelist: %s", modSettings.gamelistUrl.c_str());

		flag = true;

		if (masterlinksParseResult != FZ_MASTERLIST_ONLY_OLD_CONFIG && modSettings.gamelistUrl.empty())
		{
			Msg("! Empty game files list URL found!");
			flag = false;
		}

		if (flag)
		{
			VersionAbstraction()->AssignStatus("Verifying resources...");
			VersionAbstraction()->SetVisualProgress(0);

			if (!DownloadAndApplyFileList(modSettings.gamelistUrl, GamedataFilesListName, modSettings.rootDir, masterlinksParseResult, files, true))
			{
				Msg("! Applying game files list failed!");
				flag = false;
			}
		}

		if (flag)
		{
			VersionAbstraction()->AssignStatus("Verifying engine...");

			if (!modSettings.binlistUrl.empty())
			{
				if (!DownloadAndApplyFileList(modSettings.binlistUrl, EngineFilesListName, modSettings.rootDir, masterlinksParseResult, files, false))
				{
					Msg("! Applying engine files list failed!");
					flag = false;
				}
			}
		}

		if (flag || masterlinksParseResult == FZ_MASTERLIST_ONLY_OLD_CONFIG || IsMirrorsDisabled(g_ModParams))
			break;

		//Попытка использовать зеркало окончилась неудачей - пробуем следующее
		if (mirrorId < mirrors.binlistUrls.size())
			modSettings.binlistUrl = mirrors.binlistUrls[mirrorId];

		if (mirrorId < mirrors.gamelistUrls.size())
			modSettings.gamelistUrl = mirrors.gamelistUrls[mirrorId];

		mirrorId = mirrorId + 1;
	} while (mirrorId > mirrors.binlistUrls.size() || mirrorId > mirrors.gamelistUrls.size());  //Внимание! Тут все верно! Не ставить больше либо равно - первая итерация берет ссылки не из mirrors!

	return flag;
}

bool PrepareGUI()
{
	//Пока идет коннект(существует уровень) - не начинаем работу
	while (VersionAbstraction()->CheckForLevelExist())
		Sleep(10);

	//Пауза для нормального обновления мастер-листа
	Sleep(100); //Слип тут чтобы поток обновления гарантированно запустился - мало ли

	while (VersionAbstraction()->IsServerListUpdateActive())
		Sleep(1);

	Msg("Starting visual download");

	if (!VersionAbstraction()->StartVisualDownload())
	{
		Msg("! Cannot start visual download");
		return false;
	}

	VersionAbstraction()->AssignStatus("Preparing synchronization...");
	VersionAbstraction()->ResetMasterServerError();

	ShowMessageBox();
	return true;
}


bool InitModSettings(FZModSettings& modSettings, const string& modName, const string& modPath)
{
	modSettings.modName = modName;

	//Получим путь к корневой (установочной) директории мода
	modSettings.rootDir = VersionAbstraction()->UpdatePath("$app_data_root$", modPath);

	if (modSettings.rootDir[modSettings.rootDir.size() - 1] != '\\' && modSettings.rootDir[modSettings.rootDir.size() - 1] != '/')
		modSettings.rootDir += '\\';

	Msg("- Path to mod is %s", modSettings.rootDir.c_str());

	if (!ForceDirectories(modSettings.rootDir))
	{
		Msg("! Cannot create root directory");
		return false;
	}

	return true;
}

bool UpdateModFiles(FZModSettings &modSettings)
{
	VersionAbstraction()->AssignStatus("Parsing master links list...");
	FZModMirrorsSettings mirrors;
	FZMasterListApproveType masterlinksParseResult = DownloadAndParseMasterModsList(modSettings, mirrors);

	if (masterlinksParseResult == FZ_MASTERLIST_NOT_APPROVED)
	{
		Msg("! Master links disallow running the mod!");
		return false;
	}

	VersionAbstraction()->AssignStatus("Scanning directory...");
	//Просканируем корневую директорию на содержимое

	FZFiles files;

	// TODO: CURL
	//if (IsGameSpyDlForced(g_ModParams))
	files.SetDlMode(FZ_DL_MODE_GAMESPY);
	//else
	//	files.SetDlMode(FZ_DL_MODE_CURL);

	long long lastDownloadedBytes = 0;
	files.SetCallback(DownloadCallback, &lastDownloadedBytes);

	if (!files.ScanPath(modSettings.rootDir))
	{
		Msg("! Scanning root directory failed!");
		return false;
	}

	//Создадим копию текущего состояния - пригодится при переборе зеркал
	FZFiles filesCp;
	filesCp.Copy(files);

	//Также прочитаем и запомним содержимое binlist и gamelist - чтобы попробовать использовать старый конфиг, если ни одно из зеркал не окажется доступным.
	FZConfigBackup oldGamelist = CreateConfigBackup(modSettings.rootDir + GamedataFilesListName);
	FZConfigBackup oldBinlist = CreateConfigBackup(modSettings.rootDir + EngineFilesListName);

	bool flag = GetFileLists(filesCp, files, modSettings, masterlinksParseResult, mirrors);

	//Если не удалось скачать ни с одного из зеркал - пробуем запуститься с тем, что уже есть у нас
	if (!flag && masterlinksParseResult != FZ_MASTERLIST_ONLY_OLD_CONFIG)
	{
		Msg("! Mirrors unavailable, trying to apply backup");
		files.Copy(filesCp);

		if (FreeConfigBackup(oldGamelist, true))
			flag = DownloadAndApplyFileList("", GamedataFilesListName, modSettings.rootDir, FZ_MASTERLIST_ONLY_OLD_CONFIG, files, true);

		if (flag && oldBinlist.sz != 0)
			flag = FreeConfigBackup(oldBinlist, true) && DownloadAndApplyFileList("", EngineFilesListName, modSettings.rootDir, FZ_MASTERLIST_ONLY_OLD_CONFIG, files, false);
		else
			FreeConfigBackup(oldBinlist, false);
	}
	else
	{
		FreeConfigBackup(oldBinlist, false);
		FreeConfigBackup(oldGamelist, false);
	}

	mirrors.binlistUrls.clear();
	mirrors.gamelistUrls.clear();

	if (!flag)
	{
		Msg("! Cannot apply lists from the mirrors!");
		return false;
	}

	//удалим файлы из юзердаты из списка синхронизируемых; скопируем доступные файлы вместо загрузки их
	Msg("- =======Preprocessing files=======");
	VersionAbstraction()->AssignStatus("Preprocessing files...");

	PreprocessFiles(files, modSettings.rootDir);
	Msg("=======Sorting files=======");
	files.SortBySize();
	files.Dump();
	VersionAbstraction()->AssignStatus("Downloading content...");

	//Выполним синхронизацию файлов
	Msg("=======Actualizing game data=======");

	if (files.ActualizeFiles())
		return true;

	Msg("! Actualizing files failed!");
	return false;
}

//Выполняется в отдельном потоке
bool DoWork(const string &modName, const string &modPath)
{
	if (!PrepareGUI())
		return false;

	g_SkipFullFileCheck = SkipFullFileCheck(g_ModParams);
	FZModSettings modSettings;

	if (!InitModSettings(modSettings, modName, modPath))
		return false;

	if (!UpdateModFiles(modSettings))
		return false;

	Msg("fullInstall %s, shared patches %s", BoolToStr(modSettings.fsltxSettings.fullInstall).c_str(), BoolToStr(modSettings.fsltxSettings.sharePatchesDir).c_str());

	if (!BuildFsGame(modSettings.rootDir + FsltxName, modSettings.fsltxSettings))
		return false;

	BuildUserLtx(modSettings);	
	return RunClient(modSettings);
}
