#include "Common.h"
#include "lowlevel/Abstractions.h"
#include "FileManager.h"
#include "HttpDownloader.h"
#include "IniFile.h"
#include <shlwapi.h>
#include <algorithm>
#include <fstream>

#pragma comment(lib, "shlwapi.lib")

using std::vector;
using std::endl;
using u32 = unsigned int;
HMODULE CurrentModule;

extern bool ForceShowMessage(string cmd);
extern bool IsGameSpyDlForced(string cmdline);
extern bool IsMirrorsDisabled(string cmdline);
extern string GetServerIp(string cmdline);
extern int GetServerPort(string cmdline);
extern bool IsCmdLineNameNameNeeded(string cmdline);
extern string GetCustomBinUrl(string cmdline);
extern string GetPassword(string cmdline);
extern string GetCustomGamedataUrl(string cmdline);
extern string GetExeName(string cmdline, string defVal);
extern bool IsFullInstallMode(string cmdline);
extern bool IsSharedPatches(string cmdline);
extern string GetConfigsDir(string cmdline, string defVal);
extern bool ForceDirectories(const string& path);

extern bool WinapiDownloadFile(const char* url, const char* path);
extern void KillMutex();

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	CurrentModule = hModule;
	return TRUE;
}

enum class FZDllModFunResult : u32
{
	FZ_DLL_MOD_FUN_SUCCESS_LOCK = 0,    //Мод успешно загрузился, требуется залочить клиента по name_lock
	FZ_DLL_MOD_FUN_SUCCESS_NOLOCK = 1,  //Успех, лочить клиента (с использованием name_lock) пока не надо
	FZ_DLL_MOD_FUN_FAILURE = 2			//Ошибка загрузки мода
};

using FZMasterLinkListAddr = vector<string>;

struct FZFsLtxBuilderSettings
{
	bool share_patches_dir;
	bool full_install;
	string configs_dir;
};

struct FZModSettings
{
	string root_dir;
	string exe_name;
	string modname;
	string binlist_url;
	string gamelist_url;
	FZFsLtxBuilderSettings fsltx_settings;
};

struct FZModMirrorsSettings
{
	FZMasterLinkListAddr binlist_urls;
	FZMasterLinkListAddr gamelist_urls;
};

struct FZConfigBackup
{
	string filename;
	string buf;
	u32 sz;
};

std::string _mod_rel_path;
std::string _mod_name;
std::string _mod_params;
std::string mod_dir_prefix = ".svn\\";

HINSTANCE _dll_handle;
HANDLE _fz_loader_semaphore_handle;

const char* gamedata_files_list_name = "gamedata_filelist.ini";
const char* engine_files_list_name = "engine_filelist.ini";
const char* master_mods_list_name = "master_mods_filelist.ini";
const char* fsltx_name = "fsgame.ltx";
const char* userltx_name = "user.ltx";
const char* userdata_dir_name = "userdata\\";
const char* engine_dir_name = "bin\\";
const char* patches_dir_name = "patches\\";
const char* mp_dir_name = "mp\\";
//const char* additional_keys_line_file  = "mod_key_line.txt";

const char* fz_loader_semaphore_name = "Local\\FREEZONE_STK_MOD_LOADER_SEMAPHORE";
const char* fz_loader_modules_mutex_name = "Local\\FREEZONE_STK_MOD_LOADER_MODULES_MUTEX";

string BoolToStr(bool b)
{
	return b ? "true" : "false";
}

bool FileExists(string path)
{
	return PathFileExists(path.c_str());
}

FZDownloaderThread* CreateDownloaderThreadForUrl(string url)
{
	const char* httpsStr = "https";

	// TODO: CURL
	//if (IsGameSpyDlForced(_mod_params) && strncmp(url.c_str(), httpsStr, strlen(httpsStr)))
	{
		Msg("- Creating GS dl thread");
		return new FZGameSpyDownloaderThread();
	}

	//Msg("Creating CURL dl thread");
	//return new FZCurlDownloaderThread();
}

//procedure PushToArray(var a : FZMasterLinkListAddr; s:string);
//var
//i : integer;
//begin
//i : = length(a);
//setlength(a, i + 1);
//a[i]: = s;
//end;


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

FZMasterListApproveType DownloadAndParseMasterModsList(FZModSettings& settings, FZModMirrorsSettings& mirrors)
{
	FZMasterLinkListAddr master_links;
	bool list_downloaded;
	FZDownloaderThread* dlThread;
	FZFileDownloader* dl;

	string full_path;
	FZMasterListApproveType params_approved;
	string core_params;

	FZModSettings tmp_settings;
	string tmp1, tmp2;
	bool binlist_valid, gamelist_valid;

	const char* DEBUG_MODE_KEY = "-fz_customlists";
	const char*
		MASTERLINKS_BINLIST_KEY = "binlist";
	const char* MASTERLINKS_GAMELIST_KEY = "gamelist";
	const char* MASTERLINKS_FULLINSTALL_KEY = "fullinstall";
	const char* MASTERLINKS_SHARED_PATCHES_KEY = "sharedpatches";
	const char* MASTERLINKS_CONFIGS_DIR_KEY = "configsdir";
	const char* MASTERLINKS_EXE_NAME_KEY = "exename";

	FZMasterListApproveType result = FZ_MASTERLIST_NOT_APPROVED;

	master_links.push_back("http://stalker-life.com/stalker_files/mods_shoc/tsmp3/ModMasterLinks.txt");
	master_links.push_back("https://raw.githubusercontent.com/FreeZoneMods/modmasterlinks/master/links.ini"); // TODO: решить вопрос с https
	master_links.push_back("http://www.gwrmod.tk/files/mods_links.ini");
	master_links.push_back("http://www.stalker-life.ru/mods_links/links.ini");
	master_links.push_back("http://stalker.stagila.ru:8080/stcs_emergency/mods_links.ini");
	master_links.push_back("http://www.gwrmod.tk/files/mods_links_low_priority.ini");

	list_downloaded = false;
	full_path = settings.root_dir + master_mods_list_name;

	dlThread = CreateDownloaderThreadForUrl(master_links[0]);
	for (u32 i = 0; i < master_links.size(); i++)
	{

		dl = dlThread->CreateDownloader(master_links[i], full_path, 0);
		list_downloaded = dl->StartSyncDownload();
		delete dl;
		if (list_downloaded)
			break;
	}

	delete dlThread;

	//for (u32 i = 0; i < master_links.size(); i++)
	//{
	//	list_downloaded = WinapiDownloadFile(master_links[i].c_str(), full_path.c_str());

	//	if (list_downloaded)
	//		break;
	//}

	tmp_settings = settings;
	tmp_settings.binlist_url = GetCustomBinUrl(_mod_params);
	tmp_settings.gamelist_url = GetCustomGamedataUrl(_mod_params);
	tmp_settings.exe_name = GetExeName(_mod_params, "");
	tmp_settings.fsltx_settings.full_install = IsFullInstallMode(_mod_params);
	tmp_settings.fsltx_settings.share_patches_dir = IsSharedPatches(_mod_params);
	tmp_settings.fsltx_settings.configs_dir = GetConfigsDir(_mod_params, "");

	params_approved = FZ_MASTERLIST_NOT_APPROVED;
	int j;

	if (list_downloaded)
	{
		// Мастер-список успешно скачался, будем парсить его содержимое
		FZIniFile cfg(full_path);
		for (int i = 0; i < cfg.GetSectionsCount(); i++)
		{
			if (cfg.GetSectionName(i) == tmp_settings.modname)
			{
				//Нашли в мастер-конфиге секцию, отвечающую за наш мод
				Msg("Mod %s found in master list", settings.modname.c_str());
				params_approved = FZ_MASTERLIST_NOT_APPROVED;

				//Заполняем список всех доступных зеркал, попутно ищем ссылки из binlist и gamelist в списке зеркал
				j = 0;
				if (tmp_settings.gamelist_url.empty() && tmp_settings.binlist_url.empty())
				{

					//Юзер не заморачивается указыванием списков, выбираем их сами
					binlist_valid = true;
					gamelist_valid = true;
				}
				else
				{
					binlist_valid = false;
					gamelist_valid = false;
				}

				while (true)
				{
					//Конструируем имя параметров зеркала
					tmp1 = GenerateMirrorSuffixForIndex(j);
					tmp2 = MASTERLINKS_GAMELIST_KEY + tmp1;
					tmp1 = MASTERLINKS_BINLIST_KEY + tmp1;

					//Вычитываем параметры зеркала
					tmp1 = cfg.GetStringDef(tmp_settings.modname, tmp1, "");
					tmp2 = cfg.GetStringDef(tmp_settings.modname, tmp2, "");

					//Проверяем и сохраняем
					if (tmp1.empty() && tmp2.empty())
						break;

					Msg("Pushing mirror %u : binlist %s, gamelist", mirrors.binlist_urls.size(), tmp1.c_str(), tmp2.c_str());

					mirrors.binlist_urls.push_back(tmp1);
					mirrors.gamelist_urls.push_back(tmp2);

					if (tmp1 == tmp_settings.binlist_url)
						binlist_valid = true;
					if (tmp2 == tmp_settings.gamelist_url)
						gamelist_valid = true;

					j = j + 1;
				}

				//Убеждаемся, что пользователь не подсунул нам "левую" ссылку
				if (mirrors.binlist_urls.empty() && mirrors.gamelist_urls.empty())
				{
					Msg("! Invalid mod parameters in master links");
					break;
				}

				if (!binlist_valid)
				{
					Msg("! The binlist URL specified in mod parameters cant be found in the master links list");
					break;
				}

				if (!gamelist_valid)
				{
					Msg("! The gamelist URL specified in mod parameters cant be found in the master links list");
					break;
				}

				//Если пользователь не передал нам в строке запуска ссылок - берем указанные в "основном" зеркале
				if (tmp_settings.gamelist_url.empty() && tmp_settings.binlist_url.empty())
				{
					if (!mirrors.gamelist_urls.empty())
						tmp_settings.gamelist_url = mirrors.gamelist_urls[0];


					if (!mirrors.binlist_urls.empty())
						tmp_settings.binlist_url = mirrors.binlist_urls[0];

					for (u32 j = 0; j < mirrors.binlist_urls.size() - 1; j++)
					{
						mirrors.gamelist_urls[j] = mirrors.gamelist_urls[j + 1];
						mirrors.binlist_urls[j] = mirrors.binlist_urls[j + 1];
					}

					// TODO: разобраться что тут было
					//mirrors.gamelist_urls.resize(mirrors.gamelist_urls.size() - 1);
					//mirrors.binlist_urls.resize(mirrors.binlist_urls.size() - 1);
				}

				tmp_settings.fsltx_settings.full_install = cfg.GetBoolDef(tmp_settings.modname, MASTERLINKS_FULLINSTALL_KEY, false);
				tmp_settings.fsltx_settings.share_patches_dir = cfg.GetBoolDef(tmp_settings.modname, MASTERLINKS_SHARED_PATCHES_KEY, false);
				tmp_settings.fsltx_settings.configs_dir = cfg.GetStringDef(tmp_settings.modname, MASTERLINKS_CONFIGS_DIR_KEY, "");
				tmp_settings.exe_name = cfg.GetStringDef(tmp_settings.modname, MASTERLINKS_EXE_NAME_KEY, "");
				params_approved = FZ_MASTERLIST_APPROVED;
				break;
			}
			else if (params_approved == FZ_MASTERLIST_NOT_APPROVED)
			{
				//Если ссылка на binlist пустая или находится в конфиге какого-либо мода - можно заапрувить ее
				//Однако заканчивать рано - надо перебирать и проверять также следующие секции, так как в них может найтись секция с модом, в которой будут другие движок и/или геймдата!
				binlist_valid = tmp_settings.binlist_url.empty();
				j = 0;
				tmp2 = cfg.GetSectionName(i);

				while (!binlist_valid)
				{
					tmp1 = cfg.GetStringDef(tmp2, MASTERLINKS_BINLIST_KEY + GenerateMirrorSuffixForIndex(j), "");
					if (tmp1.empty() && cfg.GetStringDef(tmp2, MASTERLINKS_GAMELIST_KEY + GenerateMirrorSuffixForIndex(j), "").empty())
						break;
					binlist_valid = tmp1 == tmp_settings.binlist_url;
					j = j + 1;
				}

				if (binlist_valid)
				{
					if (tmp_settings.binlist_url.empty())
						Msg("No engine mod, approved");
					else
						Msg("Engine %s approved by mod %s", tmp_settings.binlist_url.c_str(), cfg.GetSectionName(i).c_str());

					params_approved = FZ_MASTERLIST_APPROVED;
				}
			}
		}
	}
	else
	{
		//Список почему-то не скачался. Ограничимся геймдатными и скачанными ранее модами.
		Msg("! Cannot download master links!");
		if (tmp_settings.binlist_url.empty() && !tmp_settings.gamelist_url.empty())
		{
			Msg("Gamedata mod approved");
			params_approved = FZ_MASTERLIST_APPROVED;
		}
		else
		{

			params_approved = FZ_MASTERLIST_ONLY_OLD_CONFIG;
			Msg("Only old mods approved");

		}
	}

	core_params = VersionAbstraction()->GetCoreParams();

	if (params_approved != FZ_MASTERLIST_NOT_APPROVED || core_params.find(DEBUG_MODE_KEY) != string::npos)
	{
		settings = tmp_settings;

		std::replace(settings.exe_name.begin(), settings.exe_name.end(), '\\', '_');
		std::replace(settings.exe_name.begin(), settings.exe_name.end(), '/', '_');
		//std::replace(settings.exe_name.begin(), settings.exe_name.end(), "..", "__");

		if (core_params.find(DEBUG_MODE_KEY) != string::npos)
		{
			Msg("Debug mode - force approve");
			result = FZ_MASTERLIST_APPROVED;
		}
		else
			result = params_approved;
	}

	return result;
}

bool DownloadAndApplyFileList(string url, string list_filename, string root_dir, FZMasterListApproveType masterlinks_type, FZFiles &fileList, bool update_progress)
{
	FZFileDownloader* dl;
	string filepath;
	int i, files_count;

	string section;
	FZCheckParams fileCheckParams;
	string fileurl;
	string filename;
	u32 compression;
	FZDownloaderThread* thread;
	u32 starttime, last_update_time;
	const u32 MAX_NO_UPDATE_DELTA = 5000;

	bool result = false;
	filepath = root_dir + list_filename;

	if (masterlinks_type == FZ_MASTERLIST_ONLY_OLD_CONFIG)
	{
		//Не загружаем ничего! В параметрах URL вообще может ничего не быть
		//Просто пытаемся использовать старую конфигурацию
	}
	else if (masterlinks_type == FZ_MASTERLIST_NOT_APPROVED)
	{
		Msg("! Master links dont allow the mod");
		return result;
	}
	else
	{
		if (url.empty())
		{
			Msg("! No list URL specified");
			return result;
		}

		Msg("Downloading list %s to %s", url.c_str(), filepath.c_str());

		thread = CreateDownloaderThreadForUrl(url);
		dl = thread->CreateDownloader(url, filepath, 0);
		result = dl->StartSyncDownload();
		delete dl;
		delete thread;

		if (!result)
		{
			Msg("! Downloading list failed");
			return false;
		}
	}

	result = false;

	FZIniFile cfg(filepath);
	files_count = cfg.GetIntDef("main", "files_count", 0);
	if (!files_count)
	{
		Msg("! No files in file list");
		return false;
	}

	starttime = GetCurrentTime();
	last_update_time = starttime;

	for (i = 0; i < files_count; i++)
	{

		section = "file_" + std::to_string(i);
		Msg("- Parsing section %s", section.c_str());

		filename = cfg.GetStringDef(section, "path", "");
		if (filename.empty())
		{

			Msg("! Invalid name for file %u", i);
			return false;
		}


		if (cfg.GetBoolDef(section, "ignore", false))
		{
			if (!fileList.AddIgnoredFile(filename))
			{
				Msg("! Cannot add to ignored file %u (%s)", i, filename.c_str());
				return false;
			}
		}
		else
		{
			fileurl = cfg.GetStringDef(section, "url", "");
			if (fileurl.empty())
			{
				Msg("! Invalid url for file %u", i);
				return false;
			}

			compression = cfg.GetIntDef(section, "compression", 0);

			fileCheckParams.crc32 = 0;
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

			if (!fileList.UpdateFileInfo(filename, fileurl, compression, fileCheckParams))
			{
				Msg("! Cannot update file info %u (%s)", i, filename.c_str());
				return false;
			}
		}

		if (VersionAbstraction()->CheckForUserCancelDownload())
		{
			Msg("! Stop applying file list - user-cancelled");
			return false;
		}

		if (update_progress)
		{
			if (GetCurrentTime() - last_update_time > MAX_NO_UPDATE_DELTA)
			{
				VersionAbstraction()->SetVisualProgress(static_cast<float>(100 * i) / files_count);
				last_update_time = GetCurrentTime();
			}
		}
	}

	Msg("- File list %s processed, time %u ms", list_filename.c_str(), GetCurrentTime() - starttime);
	return true;
}

bool DownloadCallback(FZFileActualizingProgressInfo info, void* userdata)
{
	float progress = 0;

	if (info.total_mod_size > 0)
	{
		long long ready = info.total_downloaded + info.total_up_to_date_size;

		if (ready > 0 || ready <= info.total_mod_size)
			progress = (static_cast<float>(ready) / info.total_mod_size) * 100;
	}

	long long* last_downloaded_bytes = reinterpret_cast<long long*>(userdata);

	if (*last_downloaded_bytes != info.total_downloaded)
	{
		if (info.status != FZ_ACTUALIZING_VERIFYING_START && info.status != FZ_ACTUALIZING_VERIFYING)
		{
			//Msg("Downloaded %lld, state %u", info.total_downloaded, static_cast<u32>(info.status));
		}
		else
		{
			if (info.status == FZ_ACTUALIZING_VERIFYING_START)
				VersionAbstraction()->AssignStatus("Verifying downloaded content...");

			//Msg("Verified %lld, state %u", info.total_downloaded, static_cast<u32>(info.status));
		}

		*last_downloaded_bytes = info.total_downloaded;
	}

	VersionAbstraction()->SetVisualProgress(progress);
	return !VersionAbstraction()->CheckForUserCancelDownload();
}

bool BuildFsGame(string filename, FZFsLtxBuilderSettings settings)
{
	string tmp;
	std::ofstream f;
	f.open(filename);

	if (!f.is_open())
		return false;

	f << "$mnt_point$=false|false|$fs_root$|gamedata\\" << endl;
	f << "$app_data_root$=false |false |$fs_root$|" << userdata_dir_name << endl;
	f << "$parent_app_data_root$=false |false|" << VersionAbstraction()->UpdatePath("$app_data_root$", "") << endl;
	f << "$parent_game_root$=false|false|" << VersionAbstraction()->UpdatePath("$fs_root$", "") << endl;

	if (settings.full_install)
	{
		f << "$arch_dir$=false| false| $fs_root$" << endl;
		f << "$game_arch_mp$=false| false| $fs_root$| mp\\" << endl;
		f << "$arch_dir_levels$=false| false| $fs_root$| levels\\" << endl;
		f << "$arch_dir_resources$=false| false| $fs_root$| resources\\" << endl;
		f << "$arch_dir_localization$=false| false| $fs_root$| localization\\" << endl;
	}
	else
	{
		if (VersionAbstraction()->PathExists("$arch_dir"))
			f << "$arch_dir$=false| false|" << VersionAbstraction()->UpdatePath("$arch_dir$", "") << endl;

		if (VersionAbstraction()->PathExists("$game_arch_mp$"))
		{
			//SACE3 обладает нехорошей привычкой писать сюда db-файлы, одна ошибка - и неработоспособный клиент
			//У нас "безопасное" место для записи - это юзердата (даже в случае ошибки - брикнем мод, не игру)
			//Маппим $game_arch_mp$ в юзердату, а чтобы игра подхватывала оригинальные файлы с картами -
			//создадим еще одну запись
			f << "$game_arch_mp$=false| false|$app_data_root$" << endl;
			f << "$game_arch_mp_parent$=false| false|" << VersionAbstraction()->UpdatePath("$game_arch_mp$", "") << endl;
		}

		if (VersionAbstraction()->PathExists("$arch_dir_levels$"))
			f << "$arch_dir_levels$=false| false|" << VersionAbstraction()->UpdatePath("$arch_dir_levels$", "") << endl;

		if (VersionAbstraction()->PathExists("$arch_dir_resources$"))
			f << "$arch_dir_resources$=false| false|" << VersionAbstraction()->UpdatePath("$arch_dir_resources$", "") << endl;

		if (VersionAbstraction()->PathExists("$arch_dir_localization$"))
			f << "$arch_dir_localization$=false| false|" << VersionAbstraction()->UpdatePath("$arch_dir_localization$", "") << endl;
	}

	if (VersionAbstraction()->PathExists("$arch_dir_patches$") && settings.share_patches_dir)
	{
		f << "$arch_dir_patches$=false| false|" << VersionAbstraction()->UpdatePath("$arch_dir_patches$", "") << endl;
		f << "$arch_dir_second_patches$=false|false|$fs_root$|patches\\" << endl;
	}
	else
		f << "$arch_dir_patches$=false|false|$fs_root$|patches\\" << endl;

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

	if (!settings.configs_dir.empty())
		f << "$game_config$=true|false|$game_data$|" + settings.configs_dir + "\\" << endl;
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

bool CopyFileIfValid(string src_path, string dst_path, FZCheckParams targetParams)
{
	FZCheckParams fileCheckParams;
	string dst_dir;
	bool result = false;

	if (GetFileChecks(src_path, &fileCheckParams, !targetParams.md5.empty()))
	{
		if (CompareFiles(fileCheckParams, targetParams))
		{
			dst_dir = dst_path;

			while (dst_dir[dst_dir.size() - 1] != '\\' && dst_dir[dst_dir.size() - 1] != '/')
			{
				dst_dir.resize(dst_dir.size() - 1);
			}
			ForceDirectories(dst_dir);

			if (CopyFile(src_path.c_str(), dst_path.c_str(), false))
			{
				Msg(" Copied %s to %s", src_path.c_str(), dst_path.c_str());
				result = true;
			}
			else
			{
				Msg("! Cannot copy file %s to %s", src_path.c_str(), dst_path.c_str());
			}
		}
		else
		{
			Msg("! Checksum or size not equal to target");
		}
	}

	return result;
}

void PreprocessFiles(FZFiles &files, string mod_root)
{
	const char* NO_PRELOAD = "-fz_nopreload";
	int i;
	pFZFileItemData e;
	string core_root;
	string filename;
	string src, dst;
	bool disable_preload = true; // VersionAbstraction()->GetCoreParams().find(NO_PRELOAD) != string::npos;

	files.AddIgnoredFile(gamedata_files_list_name);
	files.AddIgnoredFile(engine_files_list_name);

	for (i = files.EntriesCount() - 1; i >= 0; i--)
	{
		e = files.GetEntry(i);

		if (!strncmp(e->name.c_str(), userdata_dir_name, strlen(userdata_dir_name)) && e->required_action == FZ_FILE_ACTION_UNDEFINED)
		{
			//спасаем файлы юзердаты от удаления
			files.UpdateEntryAction(i, FZ_FILE_ACTION_IGNORE);
		}
		else if (!strncmp(e->name.c_str(), engine_dir_name, strlen(engine_dir_name)) && e->required_action == FZ_FILE_ACTION_DOWNLOAD)
		{
			if (!disable_preload)
			{
				//Проверим, есть ли уже такой файл в текущем движке
				core_root = VersionAbstraction()->GetCoreApplicationPath();
				filename = e->name;
				filename.erase(0, strlen(engine_dir_name));
				src = core_root + filename;
				dst = mod_root + e->name;

				if (CopyFileIfValid(src, dst, e->target))
					files.UpdateEntryAction(i, FZ_FILE_ACTION_NO);
			}
		}
		else if (!strncmp(e->name.c_str(), patches_dir_name, strlen(patches_dir_name)) && e->required_action == FZ_FILE_ACTION_DOWNLOAD)
		{
			if (!disable_preload && VersionAbstraction()->PathExists("$arch_dir_patches$"))
			{
				//Проверим, есть ли уже такой файл в текущей копии игры
				filename = e->name;
				filename.erase(0, strlen(patches_dir_name));
				src = VersionAbstraction()->UpdatePath("$arch_dir_patches$", filename);
				dst = mod_root + e->name;

				if (CopyFileIfValid(src, dst, e->target))
					files.UpdateEntryAction(i, FZ_FILE_ACTION_NO);
			}
		}
		else if (!strncmp(e->name.c_str(), mp_dir_name, strlen(mp_dir_name)) && e->required_action == FZ_FILE_ACTION_DOWNLOAD)
		{
			if (!disable_preload && VersionAbstraction()->PathExists("$game_arch_mp$"))
			{
				//Проверим, есть ли уже такой файл в текущей копии игры
				filename = e->name;
				filename.erase(0, strlen(mp_dir_name));
				src = VersionAbstraction()->UpdatePath("$game_arch_mp$", filename);
				dst = mod_root + e->name;

				if (CopyFileIfValid(src, dst, e->target))
					files.UpdateEntryAction(i, FZ_FILE_ACTION_NO);
			}
		}
	}
}

FZConfigBackup CreateConfigBackup(string filename)
{
	HANDLE f;
	u32 sz;
	DWORD readcnt;
	bool success;
	const u32 MAX_LEN = 1 * 1024 * 1024;

	FZConfigBackup result;
	//result.filename: = '';
	//result.buf = nullptr;
	result.sz = 0;

	if (filename.empty())
		return result;

	f = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	Msg("Backup config %s, handle %u", filename.c_str(), reinterpret_cast<u32>(f));

	if (f == INVALID_HANDLE_VALUE)
	{
		Msg("! Error opening file %s", filename.c_str());
		return result;
	}

	//__try
	{
		success = false;
		readcnt = 0;

		sz = GetFileSize(f, nullptr);
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
		result.filename = filename;
		success = true;
	}
	//__except (EXCEPTION_EXECUTE_HANDLER)
	//{
		//Msg("! Something went wrong :(");
		//result.filename = "";
		//result.buf = "";
		//result.sz = 0;
	//}

	CloseHandle(f);
	return result;
}

bool FreeConfigBackup(FZConfigBackup backup, bool need_restore)
{
	HANDLE f;
	DWORD writecnt;
	bool result = false;

	if (need_restore)
	{
		f = CreateFile(backup.filename.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		Msg("Restoring backup config %s, handle %u", backup.filename.c_str(), reinterpret_cast<u32>(f));

		if (f == INVALID_HANDLE_VALUE)
			Msg("! Error opening file %s", backup.filename.c_str());
		else
		{
			writecnt = 0;

			if (!WriteFile(f, backup.buf.data(), backup.sz, &writecnt, nullptr) || backup.sz != writecnt)
				Msg("! Error writing file %s", backup.filename.c_str());
			else
				result = true;

			CloseHandle(f);
		}
	}

	return result;
}

extern bool ForceDirectories(const string& path);

bool DoWork(string modName, string modPath) //Выполняется в отдельном потоке
{
	string ip;
	int port;

	long long last_downloaded_bytes;


	//files, files_cp:FZFiles;


	string cmdline, cmdapp, workingdir;
	string srcname, dstname;
	string fullPathToCurEngine;

	string playername;

	//add_params_file:textfile;
	string add_params;
	int mirror_id;
	bool flag;

	FZConfigBackup old_gamelist, old_binlist;

	bool message_initially_shown = false;

	if (ForceShowMessage(_mod_params))
	{
		message_initially_shown = VersionAbstraction()->IsMessageActive();
		Msg("Initial message status is %s", message_initially_shown ? "true" : "false");
	}

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

	if (ForceShowMessage(_mod_params))
	{
		if (message_initially_shown)
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

	//Получим путь к корневой (установочной) диектории мода
	FZModSettings mod_settings;
	mod_settings.modname = modName;
	mod_settings.root_dir = VersionAbstraction()->UpdatePath("$app_data_root$", modPath);

	if (mod_settings.root_dir[mod_settings.root_dir.size() - 1] != '\\' && mod_settings.root_dir[mod_settings.root_dir.size() - 1] != '//')
		mod_settings.root_dir += '\\';

	Msg("- Path to mod is %s", mod_settings.root_dir.c_str());

	if (!ForceDirectories(mod_settings.root_dir))
	{
		Msg("! Cannot create root directory");
		return false;
	}

	VersionAbstraction()->AssignStatus("Parsing master links list...");
	FZModMirrorsSettings mirrors;

	FZMasterListApproveType masterlinks_parse_result = DownloadAndParseMasterModsList(mod_settings, mirrors);

	if (masterlinks_parse_result == FZ_MASTERLIST_NOT_APPROVED)
	{
		Msg("! Master links disallow running the mod!");
		return false;
	}

	VersionAbstraction()->AssignStatus("Scanning directory...");
	//Просканируем корневую директорию на содержимое

	FZFiles files;

	// TODO: CURL
	//if (IsGameSpyDlForced(_mod_params))
	files.SetDlMode(FZ_DL_MODE_GAMESPY);
	//else
	//	files.SetDlMode(FZ_DL_MODE_CURL);

	last_downloaded_bytes = 0;
	files.SetCallback(DownloadCallback, &last_downloaded_bytes);

	if (!files.ScanPath(mod_settings.root_dir))
	{
		Msg("! Scanning root directory failed!");
		return false;
	}

	//Создадим копию текущего состояния - пригодится при переборе зеркал
	FZFiles files_cp;
	files_cp.Copy(files);

	//Также прочитаем и запомним содержимое binlist и gamelist - чтобы попробовать использовать старый конфиг, если ни одно из зеркал не окажется доступным.
	old_gamelist = CreateConfigBackup(mod_settings.root_dir + gamedata_files_list_name);
	old_binlist = CreateConfigBackup(mod_settings.root_dir + engine_files_list_name);

	mirror_id = 0;

	do
	{
		files.Copy(files_cp);

		//Загрузим с сервера требуемую конфигурацию корневой директории и сопоставим ее с текущей
		Msg("- =======Processing URLs combination %d =======", mirror_id);
		Msg("binlist: %s", mod_settings.binlist_url.c_str());
		Msg("gamelist: %s", mod_settings.gamelist_url.c_str());

		flag = true;

		if (masterlinks_parse_result != FZ_MASTERLIST_ONLY_OLD_CONFIG && mod_settings.gamelist_url.empty())
		{
			Msg("! Empty game files list URL found!");
			flag = false;
		}

		if (flag)
		{
			VersionAbstraction()->AssignStatus("Verifying resources...");
			VersionAbstraction()->SetVisualProgress(0);

			if (!DownloadAndApplyFileList(mod_settings.gamelist_url, gamedata_files_list_name, mod_settings.root_dir, masterlinks_parse_result, files, true))
			{
				Msg("! Applying game files list failed!");
				flag = false;
			}
		}

		if (flag)
		{
			VersionAbstraction()->AssignStatus("Verifying engine...");

			if (!mod_settings.binlist_url.empty())
			{
				if (!DownloadAndApplyFileList(mod_settings.binlist_url, engine_files_list_name, mod_settings.root_dir, masterlinks_parse_result, files, false))
				{
					Msg("! Applying engine files list failed!");
					flag = false;
				}
			}
		}

		if (flag || masterlinks_parse_result == FZ_MASTERLIST_ONLY_OLD_CONFIG || IsMirrorsDisabled(_mod_params))
			break;

		//Попытка использовать зеркало окончилась неудачей - пробуем следующее
		if (mirror_id < mirrors.binlist_urls.size())
			mod_settings.binlist_url = mirrors.binlist_urls[mirror_id];

		if (mirror_id < mirrors.gamelist_urls.size())
			mod_settings.gamelist_url = mirrors.gamelist_urls[mirror_id];

		mirror_id = mirror_id + 1;
	} while (mirror_id > mirrors.binlist_urls.size() || mirror_id > mirrors.gamelist_urls.size());  //Внимание! Тут все верно! Не ставить больше либо равно - первая итерация берет ссылки не из mirrors!

	//Если не удалось скачать ни с одного из зеркал - пробуем запуститься с тем, что уже есть у нас
	if (!flag && masterlinks_parse_result != FZ_MASTERLIST_ONLY_OLD_CONFIG)
	{
		Msg("! Mirrors unavailable, trying to apply backup");
		files.Copy(files_cp);

		if (FreeConfigBackup(old_gamelist, true))
			flag = DownloadAndApplyFileList("", gamedata_files_list_name, mod_settings.root_dir, FZ_MASTERLIST_ONLY_OLD_CONFIG, files, true);

		if (flag && old_binlist.sz != 0)
			flag = FreeConfigBackup(old_binlist, true) && DownloadAndApplyFileList("", engine_files_list_name, mod_settings.root_dir, FZ_MASTERLIST_ONLY_OLD_CONFIG, files, false);
		else
			FreeConfigBackup(old_binlist, false);
	}
	else
	{
		FreeConfigBackup(old_binlist, false);
		FreeConfigBackup(old_gamelist, false);
	}

	mirrors.binlist_urls.clear();
	mirrors.gamelist_urls.clear();

	if (!flag)
	{
		Msg("! Cannot apply lists from the mirrors!");
		return false;
	}

	//удалим файлы из юзердаты из списка синхронизируемых; скопируем доступные файлы вместо загрузки их
	Msg("- =======Preprocessing files=======");
	VersionAbstraction()->AssignStatus("Preprocessing files...");

	PreprocessFiles(files, mod_settings.root_dir);
	Msg("=======Sorting files=======");
	files.SortBySize();
	files.Dump(/*FZ_LOG_INFO*/);
	VersionAbstraction()->AssignStatus("Downloading content...");

	//Выполним синхронизацию файлов
	Msg("=======Actualizing game data=======");
	if (!files.ActualizeFiles())
	{
		Msg("! Actualizing files failed!");
		return false;
	}

	//Готово
	VersionAbstraction()->AssignStatus("Building fsltx...");
	Msg("Building fsltx");
	VersionAbstraction()->SetVisualProgress(100);

	//Обновим fsgame
	Msg("full_install %s, shared patches %s", BoolToStr(mod_settings.fsltx_settings.full_install).c_str(), BoolToStr(mod_settings.fsltx_settings.share_patches_dir).c_str());

	if (!BuildFsGame(mod_settings.root_dir + fsltx_name, mod_settings.fsltx_settings))
	{
		Msg("! Building fsltx failed!");
		return false;
	}

	VersionAbstraction()->AssignStatus("Building userltx...");

	//если user.ltx отсутствует в userdata - нужно сделать его там
	if (!FileExists(mod_settings.root_dir + userdata_dir_name + userltx_name))
	{
		Msg("Building userltx");
		//в случае с SACE команда на сохранение не срабатывает, поэтому сначала скопируем файл
		dstname = mod_settings.root_dir + userdata_dir_name;
		ForceDirectories(dstname);
		dstname = dstname + userltx_name;
		srcname = VersionAbstraction()->UpdatePath("$app_data_root$", "user.ltx");
		Msg("Copy from %s to %s", srcname.c_str(), dstname.c_str());
		CopyFile(srcname.c_str(), dstname.c_str(), false);
		VersionAbstraction()->ExecuteConsoleCommand("cfg_save " + dstname);
	}

	VersionAbstraction()->AssignStatus("Running game...");

	//Надо стартовать игру с модом
	ip = GetServerIp(_mod_params);

	if (ip.empty())
	{
		Msg("! Cannot determine IP address of the server");
		return false;
	}

	//Подготовимся к перезапуску
	Msg("Prepare to restart client");
	port = GetServerPort(_mod_params);
	if (port < 0 || port > 65535)
	{
		Msg("! Cannot determine port");
		return false;
	}

	if (IsCmdLineNameNameNeeded(_mod_params))
	{
		playername = VersionAbstraction()->GetPlayerName();
		Msg("Using player name %s", playername.c_str());
		playername = "/name=" + playername;
	}

	string psw = GetPassword(_mod_params);

	if (!psw.empty())
		psw = "/psw=" + psw;

	//assignfile(add_params_file, additional_keys_line_file);
	//try
	//	reset(add_params_file);
	//try
	//	readln(add_params_file, add_params);
	//finally
	//	closefile(add_params_file);
	//end;
	//except
	//	add_params : = '';
	//end;

	Msg("User-defined restart params: %s", add_params);

	if (!mod_settings.binlist_url.empty())
	{
		// Нестандартный двиг мода
		cmdapp = mod_settings.root_dir + "bin\\";

		if (!mod_settings.exe_name.empty())
		{
			cmdapp = cmdapp + mod_settings.exe_name;
			cmdline = mod_settings.exe_name;
		}
		else 
		{
			cmdapp = cmdapp + VersionAbstraction()->GetEngineExeFileName();
			cmdline = VersionAbstraction()->GetEngineExeFileName();
		}

		//-fzmod - показывает имя мода; -fz_nomod - тключает загрузку модов (чтобы не впасть в рекурсию/старая версия)
		//так как проверка на имя мода идет первой, то все должно работать
		cmdline = cmdline + ' ' + add_params + " -fz_nomod -fzmod " + mod_settings.modname + " -start client(" + ip + "/port=" + std::to_string(port) + playername + psw + ')';
		workingdir = mod_settings.root_dir;
	}
	//	else
	//	{
	//		begin
	//		// Используем текущий двиг
	//		sz : = 128;
	//fullPathToCurEngine: = nil;
	//	repeat
	//		if fullPathToCurEngine <> nil then FreeMem(fullPathToCurEngine, sz);
	//sz: = sz * 2;
	//	GetMem(fullPathToCurEngine, sz);
	//	if fullPathToCurEngine = nil then exit;
	//	until GetModuleFileName(VersionAbstraction().GetEngineExeModuleAddress(), fullPathToCurEngine, sz) < sz - 1;
	//cmdapp: = fullPathToCurEngine;
	//workingdir: = mod_settings.root_dir;
	//cmdline: = VersionAbstraction().GetEngineExeFileName() + ' ' + add_params + ' -fz_nomod -fzmod ' + mod_settings.modname + ' -wosace -start client(' + ip + '/port=' + inttostr(port) + playername + ')';
	//	FreeMem(fullPathToCurEngine, sz);
	//	end;

		//Точка невозврата. Убедимся, что пользователь не отменил загрузку
	if (VersionAbstraction()->CheckForUserCancelDownload())
	{
		Msg("! Cancelled by user");
		return false;
	}

	Msg("cmdapp: %s", cmdapp.c_str());
	Msg("cmdline: %s", cmdline.c_str());

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	FillMemory(&si, sizeof(si), 0);
	FillMemory(&pi, sizeof(pi), 0);
	si.cb = sizeof(si);

	//Прибьем блокирующий запуск нескольких копий сталкера мьютекс
	KillMutex();

	//Запустим клиента
	if (!CreateProcess(cmdapp.c_str(), cmdline.data(), 0, 0, false, CREATE_SUSPENDED, 0, workingdir.c_str(), &si, &pi))
	{
		Msg("! Cannot run application");
		return false;
	}
	else
	{
		ResumeThread(pi.hThread);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return true;
	}
}

void DecompressorLogger(string text)
{
	Msg("- [DECOMPR] %s", text.c_str());
}

bool InitModules()
{
	InitAbstractions();
	Msg("- Abstractions Inited!");

	//result: = result and Decompressor.Init(@DecompressorLogger);
	// Init high-level
	//result: = result and LogMgr.Init();

	//FZLogMgr.Get.SetSeverity(FZ_LOG_INFO);
	//FZLogMgr.Get.Write('Modules inited', FZ_LOG_INFO);

	return true;
}

void FreeModules()
{
	Msg("- Free modules");
	//Decompressor.Free();
	//LogMgr.Free();
	FreeAbstractions();
}

bool ThreadBody_internal()
{
	//Убедимся, что нам разрешено выделить ресурсы
	HANDLE mutex = CreateMutex(nullptr, FALSE, fz_loader_modules_mutex_name);

	if (!mutex || mutex == INVALID_HANDLE_VALUE)
		return false;

	if (WaitForSingleObject(mutex, INFINITE) != WAIT_OBJECT_0)
	{
		CloseHandle(mutex);
		return false;
	}

	if (!InitModules())
	{
		ReleaseMutex(mutex);
		CloseHandle(mutex);
		return false;
	}

	Msg("- FreeZone Mod Loader TSMP v1");
	Msg("- Build date: %s", __DATE__);
	Msg("- Mod name is %s", _mod_name.c_str());
	Msg("- Mod params %s", _mod_params.c_str());
	Msg("Working thread started");

	if (!DoWork(_mod_name, _mod_rel_path))
	{
		Msg("! Loading failed!");
		VersionAbstraction()->SetVisualProgress(0);

		if (VersionAbstraction()->IsMessageActive())
			VersionAbstraction()->TriggerMessage();

		VersionAbstraction()->AssignStatus("Downloading failed.Try again.");

		u32 i = 0;

		while (i < 10000 && !VersionAbstraction()->CheckForUserCancelDownload() && !VersionAbstraction()->CheckForLevelExist())
		{
			Sleep(1);
			i = i + 1;
		}

		VersionAbstraction()->StopVisualDownload();
	}

	Msg("- Releasing resources");
	VersionAbstraction()->ExecuteConsoleCommand("flush");

	//FreeStringMemory();
	FreeModules();

	ReleaseMutex(mutex);
	CloseHandle(mutex);
	return true;
}

//Похоже, компиль не просекает, что FreeLibraryAndExitThread не возвращает управление. Из-за этого локальные переменные оказываются
//не зачищены, и это рушит нам приложение. Для решения вопроса делаем свой асмовый враппер, лишенный указанных недостатков.
u32 ThreadBody()
{
	__asm
	{
		call ThreadBody_internal

		push[_dll_handle]
		push eax

		//Хэндл ДЛЛ надо занулить до освобождения семафора, но саму ДЛЛ выгрузить уже в самом конце - поэтому он сохранен в стеке
		mov _dll_handle, 0

		push[_fz_loader_semaphore_handle] // для вызова CloseHandle
		push 0
		push 1
		push[_fz_loader_semaphore_handle]
		mov[_fz_loader_semaphore_handle], 0
		call ReleaseSemaphore
		call CloseHandle

		pop eax //Результат работы
		pop ebx //сохраненный хэндл
		cmp al, 0
		je error_happen
		push 0
		call GetCurrentProcess
		push eax
		call TerminateProcess

		error_happen :
		push 0
			push ebx
			call FreeLibraryAndExitThread
	}
}

bool RunModLoad()
{
	char path[MAX_PATH];
	GetModuleFileName(CurrentModule, path, MAX_PATH);

	//Захватим ДЛЛ для предотвращения выгрузки во время работы потока загрузчика
	Msg("Path to loader is: %s", path);
	_dll_handle = LoadLibrary(path);

	if (!_dll_handle)
	{
		Msg("! Cannot acquire DLL %s", path);
		return false;
	}

	//Начинаем синхронизацию файлов мода в отдельном потоке
	Msg("Starting working thread");

	if (!VersionAbstraction()->ThreadSpawn(ThreadBody, nullptr))
	{
		Msg("! Cannot start thread");
		FreeLibrary(_dll_handle);
		_dll_handle = 0;
		return false;
	}

	return true;
}

void AbortConnection()
{
	Msg("Aborting connection");
	VersionAbstraction()->AbortConnection();
}

bool ValidateInput(char* modName, char* modParams)
{
	const u32 MAX_NAME_SIZE = 4096;
	const u32 MAX_PARAMS_SIZE = 4096;
	const string allowed_symbols_in_mod_name = "1234567890abcdefghijklmnopqrstuvwxyz_";
	const u32 modStrLen = strnlen_s(modName, MAX_NAME_SIZE);

	if (modStrLen + mod_dir_prefix.size() > MAX_NAME_SIZE)
	{
		Msg("! Too long mod name, exiting");
		return false;
	}

	if (strnlen_s(modParams, MAX_PARAMS_SIZE) >= MAX_PARAMS_SIZE)
	{
		Msg("! Too long mod params, exiting");
		return false;
	}

	for (u32 i = 0; i < modStrLen; i++)
	{
		if (allowed_symbols_in_mod_name.find(modName[i]) == string::npos)
		{
			Msg("! Invalid mod name, exiting");
			return false;
		}
	}

	return true;
}

FZDllModFunResult ModLoad_internal(char* modName, char* modParams)
{
	FZDllModFunResult result = FZDllModFunResult::FZ_DLL_MOD_FUN_FAILURE;
	HANDLE mutex = CreateMutex(nullptr, FALSE, fz_loader_modules_mutex_name);

	if (!mutex || mutex == INVALID_HANDLE_VALUE || WaitForSingleObject(mutex, 0) != WAIT_OBJECT_0)
		return result;

	//Отлично, основной поток закачки не стартует, пока мы не отпустим мьютекс
	if (InitModules())
	{
		AbortConnection();

		if (ValidateInput(modName, modParams))
		{
			_mod_name = modName;
			_mod_params = modParams;

			//FZLogMgr.Get.SetSeverity(GetLogSeverity(mod_params));

			//Благодаря этому хаку с префиксом, игра не полезет подгружать файлы мода при запуске оригинального клиента
			_mod_rel_path = mod_dir_prefix;
			_mod_rel_path += modName;

			if (RunModLoad())
			{
				// Не лочимся - загрузка может окончиться неудачно либо быть отменена
				// кроме того, повторный коннект при активной загрузке и выставленной инфе о моде приведет к неожиданным результатам
				result = FZDllModFunResult::FZ_DLL_MOD_FUN_SUCCESS_NOLOCK;
			}
		}

		//Основной поток закачки сам проинициализирует их заново - если не освобождать, происходит какая-то фигня при освобождении из другого потока.
		FreeModules();
	}

	ReleaseMutex(mutex);
	CloseHandle(mutex);
	return result;
}

//
////Схема работы загрузчика с ипользованием с мастер-списка модов:
//// 1) Скачиваем мастер-список модов
//// 2) Если мастер-список скачан и мод с таким названием есть в списке - используем ссылки на движок и геймдату
////    из этого списка; если заданы кастомные и не совпадающие с теми, которые в списке - ругаемся и не работаем
//// 3) Если мастер-список скачан, но мода с таким названием в нем нет - убеждаемся, что ссылка на движок либо не
////    задана (используется текущий), либо есть среди других модов, либо на клиенте активен дебаг-режим. Если не
////    выполняется ничего из этого - ругаемся и не работаем, если выполнено - используем указанные ссылки
//// 4) Если мастер-список НЕ скачан - убеждаемся, что ссылка на движок либо не задана (надо использовать текущий),
////    либо активен дебаг-режим на клиенте. Если это не выполнено - ругаемся и не работаем, иначе используем
////    предоставленные пользователем ссылки.
//// 5) Скачиваем сначала геймдатный список, затем движковый (чтобы не дать возможность переопределить в первом файлы второго)
//// 6) Актуализируем файлы и рестартим клиент
//
////Доступные ключи запуска, передающиеся в mod_params:
////-binlist <URL> - ссылка на адрес, по которому берется список файлов движка (для работы требуется запуск клиента с ключлм -fz_custom_bin)
////-gamelist <URL> - ссылка на адрес, по которому берется список файлов мода (геймдатных\патчей)
////-srv <IP> - IP-адрес сервера, к которому необходимо присоединиться после запуска мода
////-srvname <domainname> - доменное имя, по которому располагается сервер. Можно использовать вместо параметра -srv в случае динамического IP сервера
////-port <number> - порт сервера
////-gamespymode - стараться использовать загрузку средствами GameSpy
////-fullinstall - мод представляет собой самостоятельную копию игры, связь с файлами оригинальной не требуется
////-sharedpatches - использовать общую с инсталляцией игры директорию патчей
////-logsev <number> - уровень серьезности логируемых сообщений, по умолчанию FZ_LOG_ERROR
////-configsdir <string> - директория конфигов
////-exename <string> - имя исполняемого файла мода
////-includename - включить в строку запуска мода параметр /name= с именем игрока
////-preservemessage - показывать окно с сообщением о загрузке мода
////-nomirrors - запретить скачивание списков файлов мода с URL, отличающихся от указанных в ключах -binlist/-gamelist

extern "C" __declspec(dllexport) u32 __stdcall ModLoad(char* modName, char* modParams)
{
	HANDLE semaphore = CreateSemaphore(nullptr, 1, 1, fz_loader_semaphore_name);
	FZDllModFunResult result = FZDllModFunResult::FZ_DLL_MOD_FUN_FAILURE;

	if (!semaphore || semaphore == INVALID_HANDLE_VALUE)
		return static_cast<u32>(result);

	if (WaitForSingleObject(semaphore, 0) == WAIT_OBJECT_0)
	{
		// Отлично, семафор наш. Сохраним хендл на него для последующего освобождения
		_fz_loader_semaphore_handle = semaphore;
		_dll_handle = 0;

		result = ModLoad_internal(modName, modParams);

		// В случае успеха семафор будет разлочен в другом треде после окончания загрузки.
		if (result == FZDllModFunResult::FZ_DLL_MOD_FUN_FAILURE && _dll_handle)
		{
			FreeLibrary(_dll_handle);
			_dll_handle = 0;
		}

		_fz_loader_semaphore_handle = INVALID_HANDLE_VALUE;
		ReleaseSemaphore(semaphore, 1, nullptr);
		CloseHandle(semaphore);
	}
	else
		CloseHandle(semaphore); // Не повезло, сворачиваемся.

	return static_cast<u32>(result);
}
