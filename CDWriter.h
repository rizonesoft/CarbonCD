#pragma once

#include "CueSheetParser.h"
//#include "Scrambler.h"
#include "Aspi.h"
#include "MMCWriter.h"
#include "SubCodeGenerator.h"
#include "CheckSector.h"

#define WRITEMODE_2048      0
#define WRITEMODE_RAW_16    1
#define WRITEMODE_RAW_96    2
#define WRITEMODE_RAW_P96   3

#define LOADING_IN          true
#define LOADING_LOAD        true
#define LOADING_EJECT       false
#define LOADING_UNLOAD      false

class CCDWriter
{
public:
    CCDWriter(void);
    ~CCDWriter(void);

    LPCSTR GetErrorMessage(void);
    bool ParseCueSheetFile(LPCSTR FileName);
    bool ParseCueSheet(LPCSTR CueSheet, DWORD ImageSize);
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
    void GetErrorParams(BYTE& SK, BYTE& ASC, BYTE& ASCQ);
    void FlushBuffer(void);

protected:
    CCueSheetParser m_CueSheetParser;
    int m_DataMode;
    CMMCWriter m_MMCWriter;
    CSubCodeGenerator m_SubCode;

    CString m_ErrorMessage;
    int m_LeadInCounter;
    int m_WritingMode;
    BYTE m_WriteBuffer[2352 + 96 + 30];

    //   Scrambler
    CCheckSector m_EDC;
    bool m_LeadInMode;
    BYTE m_ScrambleTable[2340];
    void GenerateZeroArea(BYTE* Buffer, DWORD lba);
    void GenerateZeroAreaLeadIn(BYTE* Buffer, DWORD lba);
    void Scramble(BYTE* Buffer);
    void Sub16To96(BYTE* Buffer);

public:
    void ForceScramble(BYTE* Buffer);
    void Initialize(CAspi* aspi);
    bool IsCDR(void);
    bool SetWriteSpeed(BYTE Speed);
    void AbortWriting(void);
    bool EraseMedia(bool FastErase);
    int GetWritingMode(void);
    int GetBufferSize(void);
    bool CheckDisc(void);
    bool WriteRaw96(BYTE* Buffer, DWORD lba);
};
