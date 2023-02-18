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
	[[nodiscard]] u32 GetSectionsCount() const;
	string GetSectionName(u32 i);

protected:
	string m_FileName;
	vector<string> m_Sections;

	bool InternalGetData(const string &section, const string &key, string &value);
};
