#pragma once

using CueSheet = struct tCueSheet
{
    DWORD m_StartLBA;
    DWORD m_PregapLBA;
    DWORD m_Frames;
    DWORD m_PregapFrames;
    int m_TrackType;
};

#define TRACK_AUDIO     0
#define TRACK_MODE1_RAW 1

class CCueSheetParser
{
protected:
    LPCSTR m_CueSheet;
    LPCSTR m_CuePoint;
    CString m_ImageDir;
    CString m_CueLine;
    int m_TrackCount;
    CueSheet m_Cue[99];
    CString m_ImageFile;
    CString m_ErrorMessage;
    DWORD m_ImageSize;
    BYTE m_MMCCue[1600]; //   200 items * 8 bytes
    int m_MMCCueEntry;
    int m_DataMode;

public:
    CCueSheetParser(void);
    ~CCueSheetParser(void);
    bool Parse(LPCSTR CueSheet, LPCSTR ImageDir, DWORD ImageSize);
    bool AbstractCueLine(void);
    bool StringCompare(LPCSTR String1, LPCSTR String2, int Length);
    LPCSTR GetErrorMessage(void);
    bool CheckCue(void);
    int CreateMMCCueSheet(DWORD LeadInLBA);
    BYTE* GetMMCCueSheet(void);
    int GetEntryCount(void);
    DWORD GetTotalFrames(void);
    BYTE ToHex(BYTE Data);
    LPCSTR GetImageFileName(void);
    int GetDataMode(void);
};
