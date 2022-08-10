#pragma once

class FZAbstractGameVersion
{
public:
    virtual string GetCoreParams() = 0;
    virtual string GetCoreApplicationPath() = 0;

    virtual void ShowMpMainMenu() = 0;
    virtual void AssignStatus(string str) = 0;

    virtual bool CheckForUserCancelDownload() = 0;
    virtual bool CheckForLevelExist() = 0;

    virtual string UpdatePath(string root, string appendix) = 0;
    virtual bool PathExists(string root) = 0;

    virtual bool StartVisualDownload() = 0;
    virtual bool StopVisualDownload() = 0;  //main_menu.m_sPDProgress.IsInProgress:=0;

    virtual void SetVisualProgress(float progress) = 0;
    virtual void ExecuteConsoleCommand(string cmd) = 0;
    
    virtual string GetEngineExeFileName() = 0;
    virtual void* GetEngineExeModuleAddress() = 0;

    virtual bool ThreadSpawn(void* proc, void* args, string name = "", u32 stack = 0) = 0;
    virtual void AbortConnection() = 0;
    virtual void Log(string txt) = 0;
    
    virtual string GetPlayerName() = 0;
    virtual bool IsServerListUpdateActive() = 0;
    virtual bool IsMessageActive() = 0;

    virtual void TriggerMessage() = 0;
    virtual void PrepareForMessageShowing() = 0;
    virtual void ResetMasterServerError() = 0;
};

void InitAbstractions();
FZAbstractGameVersion* VersionAbstraction();
void FreeAbstractions();
