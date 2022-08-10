#pragma once

class FZIniFile
{
public:
	FZIniFile(string filename);
	virtual ~FZIniFile();

	int GetIntDef(string section, string key, int def);
	string GetStringDef(string section, string key, string def);
	bool GetHex(string section, string key, u32 &val);
	bool GetBoolDef(string section, string Key, bool def = false);
	int GetSectionsCount();
	string GetSectionName(int i);

protected:
	string _filename;
	vector<string> _sections;

	bool _GetData(string section, string key, string& value);
};
