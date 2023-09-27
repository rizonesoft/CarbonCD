#pragma once

#include "Aspi.h"
#include "PBBuffer.h"

class CMMCWriter
{
public:
    CMMCWriter(void);
    ~CMMCWriter(void);

protected:
    CAspi* m_Aspi;
    DWORD m_LeadInLBA;
    DWORD m_LeadInSize;
    BYTE* m_Buffer;
    CPBBuffer m_PBuf1;
    CPBBuffer m_PBuf2;
    //  BYTE m_Buf1[(2352+96)*27 + 0x0f];
    //  BYTE m_Buf2[(2352+96)*27 + 0x0f];
    BYTE* m_Buffer1;
    BYTE* m_Buffer2;
    int m_MaxFrames;
    int m_BufferingFrames;
    int m_FrameSize;
    DWORD m_BufferLBA;
    DWORD m_BufferPoint;
    bool m_FirstWriteFlag;
    int m_WritingMode;
    BYTE m_SK;
    BYTE m_ASC;
    BYTE m_ASCQ;
    bool Write(BYTE* Buffer, DWORD lba, DWORD WriteLen);

public:
    void Initialize(CAspi* aspi);
    bool CheckDisc(void);
    DWORD GetLeadInLBA(void);
    DWORD GetLeadInSize(void);
    bool SetWriteParam(int WriteMode, bool BurnProof, bool TestMode, int BufferingFrames);
    void ResetParams(void);
    int TestUnitReady(void);
    bool WriteBuffering(BYTE* Buffer, DWORD lba);
    bool WriteBufferingEx(BYTE* Buffer, DWORD lba, DWORD Size);
    bool FlushBuffer(void);
    bool SetCueSheet(BYTE* Buffer, int EntryNumber);
    bool PerformPowerCalibration(void);
    bool PreventMediaRemoval(bool BlockFlag);
    bool ReWind(void);
    bool LoadTray(BYTE LoUnlo);
    bool IsCDR(void);
    bool SetWriteSpeed(BYTE Speed);
    bool EraseMedia(bool FastErase);
    int GetBufferSize(void);
    void GetErrorParams(BYTE& SK, BYTE& ASC, BYTE& ASCQ);
    bool ReadTrackInfo(BYTE* Buffer);
};
