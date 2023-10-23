#pragma once
#include "Common.h"

inline bool Find(const char* strToSearch, const string &sourceStr, u32 &position)
{
    position = sourceStr.find(strToSearch);
    return position != string::npos;
}

inline bool Find(const char* strToSearch, const string &sourceStr)
{
    const u32 position = sourceStr.find(strToSearch);
    return position != string::npos;
}

inline string ExtractArgString(const u32 startPos, const char* argKey, const string &params)
{
    string result;
    const u32 paramsSize = params.size();

    for (u32 i = startPos + strlen(argKey); i < paramsSize; ++i)
    {
        if (params[i] == ' ')
            break;

        result += params[i];
    }

    return result;
}

inline int GetServerPort(const string &cmdline)
{
    const char* portArg = "-srvport ";
    const string modParams = ' ' + cmdline + ' ';

    u32 pos;

	if (!Find(portArg, modParams, pos))
		return -1;

    const string portStr = ExtractArgString(pos, portArg, modParams);
	return stoi(portStr);
}

inline string GetConfigsDir(const string &cmdline, const string &defVal)
{
    const char* configsKey = " -configsdir ";
    const string modParams = ' ' + cmdline + ' ';
    u32 pos;

    if (!Find(configsKey, modParams, pos))
        return defVal;

    return ExtractArgString(pos, configsKey, modParams);
}

inline string GetExeName(const string &cmdline, const string &defVal)
{
    const char* exeKey = " -exename ";
    const string modParams = ' ' + cmdline + ' ';
    u32 pos;

    if (!Find(exeKey, modParams, pos))
        return defVal;

    return ExtractArgString(pos, exeKey, modParams);
}

inline string GetCustomGamedataUrl(const string &cmdline)
{
    const char* gamedataKey = " -gamelist ";
    const string modParams = ' ' + cmdline + ' ';
    u32 pos;

    if (!Find(gamedataKey, modParams, pos))
        return "";

    return ExtractArgString(pos, gamedataKey, modParams);
}

inline string GetCustomBinUrl(const string &cmdline)
{
    const char* binKey = " -binlist ";
    const string modParams = ' ' + cmdline + ' ';
    u32 pos;

    if (!Find(binKey, modParams, pos))
        return "";

    return ExtractArgString(pos, binKey, modParams);
}

inline string GetPassword(const string &cmdline)
{
    const char* pswKey = " -psw ";
    const string modParams = ' ' + cmdline + ' ';
    u32 pos;

    if (!Find(pswKey, modParams, pos))
        return "";

    return ExtractArgString(pos, pswKey, modParams);
}

inline string GetServerIp(const string &cmdline)
{
    const char* SRV_IP = "-srv ";
    //const char* SRV_DOMAIN = "-srvname ";

    const string modParams = cmdline + ' ';
    u32 ipPos;

    if (Find(SRV_IP, modParams, ipPos))
    {
        string ipStr = ExtractArgString(ipPos, SRV_IP, modParams);
        Msg("- Use direct IP %s", ipStr.c_str());
        return ipStr;
    }

    // TODO: доменное имя сервера

    //u32 domainPos;
    //if (!Find(SRV_DOMAIN, modParams, domainPos))
    {
        Msg("! Parameters contain no server address!");
        return "";
    }
    
// rec:PDNS_RECORD;
//  result:='';
//  mod_params:=cmdline+' ';
//  rec:=nil;
//
//    //если в параметрах есть доменное имя сервера - используем его
//  posit:=Pos(SRV_DOMAIN, mod_params);
//  if posit>0 then begin
//    FZLogMgr.Get.Write('Use domain name', FZ_LOG_INFO);
//    for i:=posit+length(SRV_DOMAIN) to length(mod_params) do begin
//      if mod_params[i]=' ' then begin
//        break;
//      end else begin
//        result:=result+mod_params[i];
//      end;
//    end;
//    res:=DnsQuery(PAnsiChar(result), DNS_TYPE_A, DNS_QUERY_STANDARD, nil, @rec, nil);
//    if res <> 0 then begin
//      FZLogMgr.Get.Write('Cannot resolve '+result, FZ_LOG_ERROR);
//      result:='';
//    end else begin
//      ip:=rec^.Data.A.IpAddress;
//      result:=inttostr(ip and $FF)+'.'+inttostr((ip and $FF00) shr 8)+'.'+inttostr((ip and $FF0000) shr 16)+'.'+inttostr((ip and $FF000000) shr 24);
//      FZLogMgr.Get.Write('Received IP '+result, FZ_LOG_INFO);
//    end;
//    if (rec<>nil) then begin
//      DnsRecordListFree(rec, DnsFreeRecordList);
//    end;
//    exit;
//  end;
//  FZLogMgr.Get.Write('Parameters contain no server address!', FZ_LOG_ERROR);
//end;
}

inline bool IsGameSpyDlForced(const string &cmdline)
{
    return Find(" -gamespymode ", ' ' + cmdline + ' ');
}

inline bool IsSharedPatches(const string &cmdline)
{
    return Find(" -sharedpatches ", ' ' + cmdline + ' ');
}

inline bool IsCmdLineNameNameNeeded(const string &cmdline)
{
    return Find(" -includename ", ' ' + cmdline + ' ');
}

inline bool ForceShowMessage(const string &cmdline)
{
    return Find(" -preservemessage ", ' ' + cmdline + ' ');
}

inline bool IsMirrorsDisabled(const string &cmdline)
{
    return Find(" -nomirrors ", ' ' + cmdline + ' ');
}

inline bool IsFullInstallMode(const string &cmdline)
{
    return Find(" -fullinstall ", ' ' + cmdline + ' ');
}

inline bool SkipFullFileCheck(const string &cmdline)
{
    return Find(" -skipfullcheck ", ' ' + cmdline + ' ');
}

inline bool Allow64bitEngine(const string& cmdline)
{
    return Find(" -allow64bitengine ", ' ' + cmdline + ' ');
}

inline bool AllowErrorReportsSend(const string& cmdline)
{
    return Find(" -sendErrorRepots ", ' ' + cmdline + ' ');
}
