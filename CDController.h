#pragma once
#include "Aspi.h"
#include "MMCReader.h"
#include "MMCSubcodeReader.h"
#include "CDWriter.h"

class CCDController
{
public:
    CCDController(void);
    virtual ~CCDController(void);

protected:
    CAspi* m_Aspi;
    //  CAspiDriver m_AspiDriver;
    //  CSptiDriver m_SptiDriver;
public:
    CAspi* GetAspiCtrl(void);

protected:
    TableOfContents m_Toc;
    CCDWriter m_Writer;
    CMMCReader m_Reader;
    CMMCSubcodeReader m_SubReader;

public:
    bool ReadTOC(void);
    TableOfContents* GetTOC(void);
    bool GetDriveName(CString& String);
    bool ReadCDRaw(MSFAddress MSF, BYTE* Buffer);
    bool ReadCDAudio(MSFAddress MSF, BYTE* Buffer);
    void SetSpeed(BYTE ReadSpeed, BYTE WriteSpeed);
    DWORD GetErrorStatus(void);
    bool ReadSubQ(MSFAddress msf, BYTE* Buffer);
    bool SetErrorCorrectMode(bool CorrectFlag);
    bool ReadRawSub(MSFAddress& MSF, BYTE* Buffer, int Method);
    bool ReadATIP(BYTE* Buffer);
    int ReadCDText(BYTE* Buffer);
    LPCSTR GetWriteError(void);
    bool ParseCueSheetFile(LPCSTR FileName);
    bool ParseCueSheet(LPCSTR cue, DWORD ImageSize);
    void FinishWriting(void);
    DWORD GetLeadInSize(void);
    bool WriteRaw(BYTE* Buffer);
    bool WriteRawLeadIn(void);
    bool WriteRawGap(void);
    bool StartWriting(bool ModeMS);
    bool OPC(void);
    bool SetWritingParams(int WritingMode, bool BurnProof, bool TestMode, int BufferingFrames);
    bool LoadTray(bool LoadingMode);
    LPCSTR GetImageFileName(void);
    DWORD GetImageFrames(void);
    void GetWriteErrorParams(BYTE& SK, BYTE& ASC, BYTE& ASCQ);
    void ForceScramble(BYTE* Buffer);
    bool IsCDR(void);
    void AbortWriting(void);
    bool EraseMedia(bool FastErase);
    int GetWritingMode(void);
    int GetBufferSize(void);
    bool CheckDisc(void);
    bool WriteRaw96(BYTE* Buffer, DWORD lba);
    void InitializeAspi(void);
    void SetReadingBCDMode(bool TransBCD);
};

#define READTOC_METHOD_NORMAL   0
#define READTOC_METHOD_SESSION  1
