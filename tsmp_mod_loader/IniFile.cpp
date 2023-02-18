#include "Common.h"
#include "IniFile.h"

extern string trim(const string &s);
extern void uniassert(bool cond, const string &descr);

bool TryHexToInt(const string &hexString, u32& outVal)
{
	return sscanf_s(hexString.c_str(), "%x", &outVal);
}

bool GetNextParam(string & data, string & buf, char separator)
{
	u32 p = 0;

	for (u32 i = 1; i < data.size(); i++)
	{
		if (data[i] == separator)
		{
			p = i;
			break;
		}
	}

	if (!p)
		return false;

	buf = string(data.begin(), data.end() - data.length() + p); // leftstr
	data = string(data.begin() + p + 1, data.end()); // rightstr
	return true;
}

FZIniFile::FZIniFile(string &filename): m_FileName(std::move(filename))
{
	u32 bufferSize = 128;

	string buffer;
	bool notEnoughBufferSize;

	do
	{
		bufferSize *= 2;
		buffer.resize(bufferSize);

		// пишет в buffer все секции из файла, разделенные через нуль терминатор. Возвращает bufferSize-2 если размера буфера не хватило.
        const u32 res = GetPrivateProfileString(0, 0, 0, buffer.data(), bufferSize, m_FileName.c_str());
		notEnoughBufferSize = (res == bufferSize - 2);
	} while (notEnoughBufferSize);

	const char* start = buffer.c_str();

	while (start[0] != '\0')
	{
		m_Sections.push_back(start);
		start += strlen(start) + 1;
	}
}

int FZIniFile::GetIntDef(const string &section, const string &key, int def)
{
	string val;

	if(!InternalGetData(section, key, val))
		return def;

	return std::stoi(val);
}

string FZIniFile::GetStringDef(const string &section, const string &key, const string &def)
{
	string val;

	if (!InternalGetData(section, key, val))
		return def;

	return val;
}

bool FZIniFile::GetHex(const string &section, const string &key, u32 &val)
{
	string str;
	u32 outval = 0;

	if (!InternalGetData(section, key, str))
		return false;
	
	if (!TryHexToInt(str, outval))
		return false;

	val = outval;
	return true;
}

bool FZIniFile::GetBoolDef(const string &section, const string &key, bool def)
{
	string val;

	if (!InternalGetData(section, key, val))
		return def;

	for (char &c : val)	
		c = tolower(c);
	
	if (val == "true" || val == "on" || val == "1")
		return true;

	return false;
}

u32 FZIniFile::GetSectionsCount() const
{
	return m_Sections.size();
}

string FZIniFile::GetSectionName(u32 i)
{
	uniassert(i < GetSectionsCount(), "Invalid section index");
	return m_Sections[i];
}

bool FZIniFile::InternalGetData(const string &section, const string &key, string &value)
{
	u32 res;
	u32 bufferSize = 128;

	string buffer;
	bool notEnoughBufferSize;

	do
	{
		bufferSize *= 2;
		buffer.resize(bufferSize);

		// пишет в buffer все секции из файла, разделенные через нуль терминатор. Возвращает bufferSize-1 если размера буфера не хватило.
		res = GetPrivateProfileString(section.c_str(), key.c_str(), 0, buffer.data(), bufferSize, m_FileName.c_str());
		notEnoughBufferSize = (res == bufferSize - 1);
		buffer.resize(res);
	} while (notEnoughBufferSize);

	
	buffer += ';';
	GetNextParam(buffer, value, ';');
	value = trim(value);
	return !!res;
}
