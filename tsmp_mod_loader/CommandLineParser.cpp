#include "Common.h"

bool Find(const char* strToSearch, const string &sourceStr, u32 &position)
{
    position = sourceStr.find(strToSearch);
    return position != string::npos;
}

bool Find(const char* strToSearch, const string &sourceStr)
{
    u32 position = sourceStr.find(strToSearch);
    return position != string::npos;
}

string ExtractArgString(u32 startPos, const char* argKey, const string &params)
{
    string result;
    u32 paramsSize = params.size();

    for (u32 i = startPos + strlen(argKey); i < paramsSize; ++i)
    {
        if (params[i] == ' ')
            break;
        else
            result += params[i];
    }

    return result;
}

int GetServerPort(string cmdline)
{
    const char* portArg = "-srvport ";
    string modParams = ' ' + cmdline + ' ';

    u32 pos;

	if (!Find(portArg, modParams, pos))
		return -1;

    string portStr = ExtractArgString(pos, portArg, modParams);
	return stoi(portStr);
}

string GetConfigsDir(string cmdline, string defVal)
{
    const char* configsKey = " -configsdir ";
    string modParams = ' ' + cmdline + ' ';
    u32 pos;

    if (!Find(configsKey, modParams, pos))
        return defVal;

    return ExtractArgString(pos, configsKey, modParams);
}

string GetExeName(string cmdline, string defVal)
{
    const char* exeKey = " -exename ";
    string modParams = ' ' + cmdline + ' ';
    u32 pos;

    if (!Find(exeKey, modParams, pos))
        return defVal;

    return ExtractArgString(pos, exeKey, modParams);
}

string GetCustomGamedataUrl(string cmdline)
{
    const char* gamedataKey = " -gamelist ";
    string modParams = ' ' + cmdline + ' ';
    u32 pos;

    if (!Find(gamedataKey, modParams, pos))
        return "";

    return ExtractArgString(pos, gamedataKey, modParams);
}

string GetCustomBinUrl(string cmdline)
{
    const char* binKey = " -binlist ";
    string modParams = ' ' + cmdline + ' ';
    u32 pos;

    if (!Find(binKey, modParams, pos))
        return "";

    return ExtractArgString(pos, binKey, modParams);
}

string GetPassword(string cmdline)
{
    const char* pswKey = " -psw ";
    string modParams = ' ' + cmdline + ' ';
    u32 pos;

    if (!Find(pswKey, modParams, pos))
        return "";

    return ExtractArgString(pos, pswKey, modParams);
}

string GetServerIp(string cmdline)
{
    const char* SRV_IP = "-srv ";
    const char* SRV_DOMAIN= "-srvname ";

    string modParams = cmdline + ' ';
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

bool IsGameSpyDlForced(string cmdline)
{
    return Find(" -gamespymode ", ' ' + cmdline + ' ');
}

bool IsSharedPatches(string cmdline)
{
    return Find(" -sharedpatches ", ' ' + cmdline + ' ');
}

//
//function GetLogSeverity(cmdline: PAnsiChar):FZLogMessageSeverity;
//const
//  PARAM:string= ' -logsev ';
//
//  def_severity:FZLogMessageSeverity = FZ_LOG_INFO;
//var
//  posit, i, tmpres:integer;
//  mod_params, tmp:string;
//begin
//  result:=def_severity;
//
//  mod_params:=cmdline+' ';
//  posit:=Pos(PARAM, mod_params);
//  if posit>0 then begin
//    tmp:='';
//    for i:=posit+length(PARAM) to length(mod_params) do begin
//      if mod_params[i]=' ' then begin
//        break;
//      end else begin
//        tmp:=tmp+mod_params[i];
//      end;
//    end;
//    tmpres:=strtointdef(tmp, integer(def_severity));
//
//    if tmpres>integer(FZ_LOG_SILENT) then begin
//      tmpres:=integer(FZ_LOG_SILENT);
//    end;
//
//    result:=FZLogMessageSeverity(tmpres);
//  end;
//end;

bool IsCmdLineNameNameNeeded(string cmdline)
{
    return Find(" -includename ", ' ' + cmdline + ' ');
}

bool ForceShowMessage(string cmdline)
{
    return Find(" -preservemessage ", ' ' + cmdline + ' ');
}

bool IsMirrorsDisabled(string cmdline)
{
    return Find(" -nomirrors ", ' ' + cmdline + ' ');
}

bool IsFullInstallMode(string cmdline)
{
    return Find(" -fullinstall ", ' ' + cmdline + ' ');
}
