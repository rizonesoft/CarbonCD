#pragma once

#define SUBTYPE_LEADIN  0
#define SUBTYPE_PREGAP  1
#define SUBTYPE_LEADOUT 2
#include "CheckSector.h"

class CSubcodeGeneratorMS
{
public:
    CSubcodeGeneratorMS(void);
    ~CSubcodeGeneratorMS(void);

protected:
    CCheckSector m_EDC;
    CString m_ErrorMessage;
    int GetPrivateProfileHex(LPCSTR AppName, LPCSTR KeyName, int DefValue, LPCSTR FileName);
    BYTE m_Entry[2200];
    int m_EntryCount;
    int m_SessionCount;
    int m_Scrambled;
    int m_CDTextLength;
    DWORD m_CurrentLBA;
    DWORD m_RelativeLBA;
    int m_CurrentSession;
    BYTE m_Toc[1100];
    BYTE m_TocInt[110];
    int m_TocCount;
    int m_TocIntCount;
    int m_TocPosCounter;
    int m_TocIntPosCounter;
    int m_Toc3Counter;
    BYTE m_Sub96[96];
    BYTE m_FirstTrack;
    DWORD m_LeadIn;
    BYTE m_PregapDelta;
    WORD m_SubcodeCRCTable[256];
    BYTE m_CDText[5000];
    int m_CDTextEntries;
    int m_CDTextCounter;

public:
    BYTE m_PreGapMode[100];
    CString m_ImgFileName;
    CString m_SubFileName;
    CString m_PreFileName;
    CString m_ImageVersion;
    CString m_ProductName;
    CString m_VendorName;
    CString m_Revision;
    DWORD m_LeadInLBA[100];
    DWORD m_PregapLBA[100];
    DWORD m_MainDataLBA[100];
    DWORD m_LeadOutLBA[100];
    bool m_CDM_Extension;
    BOOL m_AbnormalImageSize;

    LPCSTR GetErrorMessage(void);
    bool ParseFile(LPCSTR FileName);
    int GetSessionCount(void);
    void ResetGenerator(DWORD StartLBA, int SubcodeType, int SessionNo);
    void CalcPositions(DWORD LeadInLBA);
    BYTE* GenerateLeadIn(void);
    void CalcCRC(BYTE* Buffer);
    void EncodeSub96(BYTE* dest, BYTE* src);
    BYTE* GeneratePreGap(void);
    BYTE* GenerateLeadOut(void);
    void CreateZeroData(BYTE* Buffer, DWORD LBA);
    void ModifyAddress(void);
    void CalcCDTextCRC(BYTE* Buffer);
};
