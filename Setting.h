#pragma once

#include "Language.h"

#define COPY_PROTECTION TRUE
#define ALPHA_MODE      FALSE

class CSetting
{
public:
    CSetting(void);
    ~CSetting(void);
    int m_Speed_Audio;
    int m_Speed_Data;
    int m_Speed_SubQ;
    int m_ReadAudioMethod;
    int m_SubQMethod;
    int m_BurstErrorCount;
    int m_BurstErrorSkip;
    bool m_TestReadMode;
    bool m_IgnoreError;
    bool m_FastErrorSkip;
    bool m_SwapChannel;
    bool m_AutoDetectMethod;
    bool m_AnalyzeSubQ;
    BOOL m_BurstErrorScan;
    BOOL m_Mastering_AlwaysOnTop;
    BOOL m_Mastering_NotifyTruncated;
    void Load(void);
    void Save(void);
    int m_DriveNo;
    BOOL m_Write_BurnProof;
    BOOL m_Write_EjectTray;
    BOOL m_Write_Opc;
    BOOL m_Write_TestMode;
    BOOL m_Write_CheckDrive;
    BOOL m_Write_AutoDetectMethod;
    int m_Write_WritingMode;
    BYTE m_Write_Speed;
    int m_Write_Buffer;
    CString m_CopyTempFile;
    CString m_LastAccessFile;
    CString m_AspiDLL;
    CString m_AutoLogFile;
    CString m_Dir;
    BOOL m_ShowLogWnd;
    BOOL m_ShowTocWnd;
    CString m_LangFile;
    CString m_SkinFile;
    CLanguage m_Lang;
    POINT m_MainDlgPos;
    RECT m_TocWnd;
    RECT m_LogWnd;
    BOOL m_AnalyzePregap;
    int m_PriorityClass;
    BOOL m_UseSPTI;
    int m_CopyProtectionSize;
    BOOL m_ExtSetting_Read;
    BOOL m_ExtSetting_Write;
    CString m_WavOnSuccess;
    CString m_WavOnFail;
    BOOL m_ReadEngine;

protected:
    CString m_SettingFile;
    int BoolToInt(bool value);
    bool IntToBool(int value);
    void WriteInt(LPCSTR lpAppName, LPCSTR lpKeyName, int Value);
    void WriteString(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR Value);
    int ReadInt(LPCSTR lpAppName, LPCSTR lpKeyName, int DefaultValue);
    void ReadString(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR DefaultValue, CString& String);

public:
    void SetPriority(void);
};

extern CSetting theSetting;
#define MSG(i) (theSetting.m_Lang.m_Str[LP_MESSAGE + i])
#define ERROR_MSG (theSetting.m_Lang.m_Str[3])
#define WARNING_MSG (theSetting.m_Lang.m_Str[4])
#define CONF_MSG (theSetting.m_Lang.m_Str[5])
