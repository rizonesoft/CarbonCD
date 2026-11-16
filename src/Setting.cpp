#include "StdAfx.h"
#include "Resource.h"
#include "setting.h"
#include "LanguageDialog.h"

CSetting::CSetting(void)
    : m_SettingFile(_T(""))
{
    m_TestReadMode = false;
    m_IgnoreError = false;
    m_SwapChannel = false;
    m_AutoDetectMethod = true;
    m_Speed_Audio = 0xff;
    m_Speed_Data = 0xff;
    m_ReadAudioMethod = 1;
    m_SubQMethod = 0;
    m_CopyTempFile = "";
    m_BurstErrorCount = 5;
    m_BurstErrorSkip = 100;
    {
        LPSTR p;
        int l, fg;
        char Buffer[2048];
        p = GetCommandLine();
        fg = 0;

        if (*p == '\"')
        {
            p++;
            fg = 1;
        }

        l = lstrlen(p);

        if (l > 1)
        {
            lstrcpy(Buffer, p);
            p = Buffer;

            if (fg == 1)
            {
                while (*p != '\"') { p++; }
            }

            else
            {
                while (*p != '\0' && *p != ' ') { p++; }
            }

            while (*p != '\\' && p > Buffer) { p--; }

            *p = '\0';
            lstrcat(Buffer, "\\");
        }

        else
        {
            lstrcpy(Buffer, ".\\");
        }

        m_Dir = Buffer;
        m_SettingFile.Format("%sCarbonCD.ini", Buffer);
    }
    //  Load();
}

CSetting::~CSetting(void)
{
    Save();
}

int CSetting::BoolToInt(bool value)
{
    return (value == true) ? 1 : 0;
}

bool CSetting::IntToBool(int value)
{
    return (value != 0) ? true : false;
}

void CSetting::WriteInt(LPCSTR lpAppName, LPCSTR lpKeyName, int Value)
{
    CString cs;
    cs.Format("%d", Value);
    WritePrivateProfileString(lpAppName, lpKeyName, cs, m_SettingFile);
}

void CSetting::WriteString(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR Value)
{
    WritePrivateProfileString(lpAppName, lpKeyName, Value, m_SettingFile);
}

int CSetting::ReadInt(LPCSTR lpAppName, LPCSTR lpKeyName, int DefaultValue)
{
    return GetPrivateProfileInt(lpAppName, lpKeyName, DefaultValue, m_SettingFile);
}

void CSetting::ReadString(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR DefaultValue, CString& String)
{
    char Buffer[1024];
    GetPrivateProfileString(lpAppName, lpKeyName, DefaultValue, Buffer, 1024, m_SettingFile);
    Buffer[1023] = '\0';
    String = Buffer;
}

void CSetting::Load(void)
{
    //   general
    m_DriveNo = ReadInt("General", "DriveNo", 0);
    ReadString("General", "AspiDLL", "", m_AspiDLL);
    ReadString("General", "LogFile", "", m_AutoLogFile);
    ReadString("General", "Language", "", m_LangFile);
    ReadString("General", "Skin", "", m_SkinFile);
    ReadString("General", "WavOnSuccess", "", m_WavOnSuccess);
    ReadString("General", "WavOnFail", "", m_WavOnFail);
    ReadString("General", "LastAccessFile", "", m_LastAccessFile);
    m_ShowLogWnd = ReadInt("General", "LogWindow", 0);
    m_ShowTocWnd = ReadInt("General", "TOCWindow", 1);
    m_Speed_SubQ = ReadInt("General", "SubQSpeed", 0xff);
    m_PriorityClass = ReadInt("General", "PriorityClass", 0);
    m_UseSPTI = ReadInt("General", "UseSPTI", 1);
    m_MainDlgPos.x = ReadInt("General", "MainDlgX", 100);
    m_MainDlgPos.y = ReadInt("General", "MainDlgy", 100);

    if (m_MainDlgPos.x < 0) { m_MainDlgPos.x = 0; }

    if (m_MainDlgPos.y < 0) { m_MainDlgPos.y = 0; }

    m_TocWnd.left = ReadInt("General", "TocWndX", 100);
    m_TocWnd.top = ReadInt("General", "TocWndY", 350);
    m_TocWnd.right = ReadInt("General", "TocWndCX", 600);
    m_TocWnd.bottom = ReadInt("General", "TocWndCY", 300);

    if (m_TocWnd.left < 0) { m_TocWnd.left = 0; }

    if (m_TocWnd.top < 0) { m_TocWnd.top = 0; }

    if (m_TocWnd.right < 0) { m_TocWnd.right = 0; }

    if (m_TocWnd.bottom < 0) { m_TocWnd.bottom = 0; }

    m_LogWnd.left = ReadInt("General", "LogWndX", 30);
    m_LogWnd.top = ReadInt("General", "LogWndY", 30);
    m_LogWnd.right = ReadInt("General", "LogWndCX", 400);
    m_LogWnd.bottom = ReadInt("General", "LogWndCY", 200);

    if (m_LogWnd.left < 0) { m_LogWnd.left = 0; }

    if (m_LogWnd.top < 0) { m_LogWnd.top = 0; }

    if (m_LogWnd.right < 0) { m_LogWnd.right = 0; }

    if (m_LogWnd.bottom < 0) { m_LogWnd.bottom = 0; }

    //   read setting
    m_TestReadMode = IntToBool(ReadInt("ReadSetting", "TestReadMode", 0));
    m_IgnoreError = IntToBool(ReadInt("ReadSetting", "IgnoreError", 0));
    m_FastErrorSkip = IntToBool(ReadInt("ReadSetting", "FastErrorSkip", 0));
    m_SwapChannel = IntToBool(ReadInt("ReadSetting", "SwapAudioChannel", 0));
    m_AutoDetectMethod = IntToBool(ReadInt("ReadSetting", "AutoDetectCommand", 1));
    m_Speed_Audio = ReadInt("ReadSetting", "AudioSpeed", 0xff);
    m_Speed_Data = ReadInt("ReadSetting", "DataSpeed", 0xff);
    m_ReadAudioMethod = ReadInt("ReadSetting", "ReadCommand", 1);
    m_SubQMethod = ReadInt("ReadSetting", "SubQMethod", 0);
    m_AnalyzeSubQ = IntToBool(ReadInt("ReadSetting", "AnalyzeSubQ", 0));
    m_BurstErrorScan = ReadInt("ReadSetting", "BurstErrorScan", 0);
    //  m_MultiSession = ReadInt("ReadSetting","MultiSession",0);
    m_AnalyzePregap = ReadInt("ReadSetting", "AnalyzePregap", 0);
    m_ExtSetting_Read = ReadInt("ReadSetting", "ExtendedSettings", 0);
    m_ReadEngine = ReadInt("ReadSetting", "Engine", 0);
    //   write setting
    m_Write_BurnProof = ReadInt("WriteSetting", "BurnProof", 1);
    m_Write_EjectTray = ReadInt("WriteSetting", "EjectTray", 1);
    m_Write_TestMode = ReadInt("WriteSetting", "TestMode", 0);
    m_Write_Opc = ReadInt("WriteSetting", "PowerCalibration", 1);
    m_Write_CheckDrive = ReadInt("WriteSetting", "CheckDrive", 1);
    m_Write_WritingMode = ReadInt("WriteSetting", "WritingMode", 0);
    m_Write_AutoDetectMethod = ReadInt("WriteSetting", "AutoDetectCommand", 1);
    m_Write_Speed = ReadInt("WriteSetting", "Speed", 255);
    m_Write_Buffer = ReadInt("WriteSetting", "Buffer", 15);
    m_ExtSetting_Write = ReadInt("WriteSetting", "ExtendedSettings", 0);
    {
        OSVERSIONINFO osver;
        osver.dwOSVersionInfoSize = sizeof (osver);

        if (GetVersionEx(&osver))
        {
            if (osver.dwPlatformId != VER_PLATFORM_WIN32_NT)
            {
                m_UseSPTI = 0;
            }
        }
    }
    //   copy setting
    ReadString("CopySetting", "TemporaryFile", "", m_CopyTempFile);
    //   mastering setting
    m_Mastering_AlwaysOnTop = ReadInt("MasteringSetting", "AlwaysOnTop", 0);
    m_Mastering_NotifyTruncated = ReadInt("MasteringSetting", "NotifyTruncated", 1);
#if COPY_PROTECTION
    m_CopyProtectionSize = ReadInt("MasteringSetting", "CopyProtectionSize", 0);
#else
    m_CopyProtectionSize = 0;
#endif

    //   load language
    if (m_LangFile == "")
    {
        CLanguageDialog dlg;

        if (dlg.DoModal() == IDCANCEL)
        {
            m_LangFile.Format("%sEnglish.cml", m_Dir);
        }
    }

    else if (m_LangFile[0] == '\\'
        || (m_LangFile[0] >= 'a' && m_LangFile[0] <= 'z' && m_LangFile[1] == ':')
        || (m_LangFile[0] >= 'A' && m_LangFile[0] <= 'Z' && m_LangFile[1] == ':'))
    {
    }
    else
    {
        CString cs;
        cs.Format("%s%s", m_Dir, m_LangFile);
        m_LangFile = cs;
    }

    m_Lang.read_language(m_LangFile);
    SetPriority();
}

void CSetting::Save(void)
{
    BYTE* q;
    LPCSTR lf;
    //   split language file name
    q = (BYTE*)static_cast<LPCSTR>(m_LangFile);
    lf = nullptr;

    while (*q != '\0')
    {
        if ((*q >= 0x80 && *q <= 0x9f) || *q > 0xe0)
        {
            q++;
        }

        else if (*q == '\\')
        {
            lf = (LPCSTR)(q + 1);
        }

        q++;
    }

    if (lf == nullptr)
    {
        lf = m_LangFile;
    }

    //   general
    WriteInt("General", "DriveNo", m_DriveNo);
    WriteString("General", "AspiDLL", m_AspiDLL);
    WriteString("General", "LogFile", m_AutoLogFile);
    WriteString("General", "Language", lf);
    WriteString("General", "Skin", m_SkinFile);
    WriteString("General", "WavOnSuccess", m_WavOnSuccess);
    WriteString("General", "WavOnFail", m_WavOnFail);
    WriteString("General", "LastAccessFile", m_LastAccessFile);
    WriteInt("General", "LogWindow", m_ShowLogWnd);
    WriteInt("General", "TOCWindow", m_ShowTocWnd);
    WriteInt("General", "SubQSpeed", m_Speed_SubQ);
    WriteInt("General", "MainDlgX", m_MainDlgPos.x);
    WriteInt("General", "MainDlgy", m_MainDlgPos.y);
    WriteInt("General", "TocWndX", m_TocWnd.left);
    WriteInt("General", "TocWndY", m_TocWnd.top);
    WriteInt("General", "TocWndCX", m_TocWnd.right);
    WriteInt("General", "TocWndCY", m_TocWnd.bottom);
    WriteInt("General", "LogWndX", m_LogWnd.left);
    WriteInt("General", "LogWndY", m_LogWnd.top);
    WriteInt("General", "LogWndCX", m_LogWnd.right);
    WriteInt("General", "LogWndCY", m_LogWnd.bottom);
    WriteInt("General", "PriorityClass", m_PriorityClass);
    WriteInt("General", "UseSPTI", m_UseSPTI);
    //   read setting
    WriteInt("ReadSetting", "TestReadMode", BoolToInt(m_TestReadMode));
    WriteInt("ReadSetting", "IgnoreError", BoolToInt(m_IgnoreError));
    WriteInt("ReadSetting", "FastErrorSkip", BoolToInt(m_FastErrorSkip));
    WriteInt("ReadSetting", "SwapAudioChannel", BoolToInt(m_SwapChannel));
    WriteInt("ReadSetting", "AutoDetectCommand", BoolToInt(m_AutoDetectMethod));
    WriteInt("ReadSetting", "AudioSpeed", m_Speed_Audio);
    WriteInt("ReadSetting", "DataSpeed", m_Speed_Data);
    WriteInt("ReadSetting", "ReadCommand", m_ReadAudioMethod);
    WriteInt("ReadSetting", "SubQMethod", m_SubQMethod);
    WriteInt("ReadSetting", "AnalyzeSubQ", BoolToInt(m_AnalyzeSubQ));
    WriteInt("ReadSetting", "BurstErrorScan", m_BurstErrorScan);
    //  WriteInt("ReadSetting","MultiSession",m_MultiSession);
    WriteInt("ReadSetting", "AnalyzePregap", m_AnalyzePregap);
    WriteInt("ReadSetting", "ExtendedSettings", m_ExtSetting_Read);
    WriteInt("ReadSetting", "Engine", m_ReadEngine);
    WriteInt("WriteSetting", "BurnProof", m_Write_BurnProof);
    WriteInt("WriteSetting", "EjectTray", m_Write_EjectTray);
    WriteInt("WriteSetting", "TestMode", m_Write_TestMode);
    WriteInt("WriteSetting", "PowerCalibration", m_Write_Opc);
    WriteInt("WriteSetting", "CheckDrive", m_Write_CheckDrive);
    WriteInt("WriteSetting", "WritingMode", m_Write_WritingMode);
    WriteInt("WriteSetting", "AutoDetectCommand", m_Write_AutoDetectMethod);
    WriteInt("WriteSetting", "Speed", m_Write_Speed);
    WriteInt("WriteSetting", "Buffer", m_Write_Buffer);
    WriteInt("WriteSetting", "ExtendedSettings", m_ExtSetting_Write);
    WriteString("CopySetting", "TemporaryFile", m_CopyTempFile);
    WriteInt("MasteringSetting", "AlwaysOnTop", m_Mastering_AlwaysOnTop);
    WriteInt("MasteringSetting", "NotifyTruncated", m_Mastering_NotifyTruncated);
#if COPY_PROTECTION
    WriteInt("MasteringSetting", "CopyProtectionSize", m_CopyProtectionSize);
#endif
}

void CSetting::SetPriority(void)
{
    if (m_PriorityClass == 0)
    {
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
    }

    else if (m_PriorityClass == 1)
    {
        SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
    }

    else if (m_PriorityClass == 2)
    {
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    }
}
