#pragma once

class CSubCodeGenerator
{
public:
    CSubCodeGenerator(void);
    ~CSubCodeGenerator(void);
    void SetCueSheet(const BYTE* MMCCueSheet, int EntryCount);

protected:
    const BYTE* m_CueSheet;
    int m_CueEntryCount;
    int m_CurrentEntry;
    int m_TocCounter;
    int m_3Counter;
    DWORD m_CurrentLBA;
    DWORD m_RelativeLBA;
    BYTE m_RawToc[100 * 16];
    int m_TocCount;
    int m_P_Count;
    WORD m_SubcodeCRCTable[256];

public:
    DWORD GenerateSub16(BYTE* Buffer);
    void LBAtoMSF(BYTE& m, BYTE& s, BYTE& f, DWORD lba);
    void CreateToc(void);
    BYTE ToHex(BYTE Bin);
    BYTE ToBin(BYTE Hex);
    void CalcCRC(BYTE* Buffer);
};
