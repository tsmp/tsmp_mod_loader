#pragma once

class FZIniFile
{
public:
	FZIniFile(string &filename);
	virtual ~FZIniFile() = default;

	int GetIntDef(const string &section, const string &key, int def);
	string GetStringDef(const string &section, const string &key, const string &def);
	bool GetHex(const string &section, const string &key, u32 &val);
	bool GetBoolDef(const string &section, const string &Key, bool def = false);
	int GetSectionsCount();
	string GetSectionName(int i);

protected:
	string _filename;
	vector<string> _sections;

	bool _GetData(const string &section, const string &key, string &value);
};
