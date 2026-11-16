#pragma once

#define SECTOR_OK       0
#define SECTOR_BADSYNC  1
#define SECTOR_BADMSF   2
#define SECTOR_BADEDC   3
#define SECTOR_BADECC   4

class CCheckSector
{
public:
    CCheckSector();
    virtual ~CCheckSector();

protected:
    DWORD CalcEDC(BYTE* Buffer);
    int m_Constraint;
    BYTE m_EccP[172];
    BYTE m_EccQ[104];
    int m_OriginalM;
    int m_OriginalS;
    int m_OriginalF;
    DWORD m_OriginalEDC;
    DWORD m_CalcEDC;
    void CalcEccP(BYTE* buffer);
    void CalcEccQ(BYTE* buffer);
    DWORD m_EdcCRCTable[256];
    BYTE m_ParityCRC_Table[256];
    BYTE m_ParityCRC_RevTable[256];
    BYTE m_ParityMatrix[2][43];

public:
    void Mode1Raw(BYTE* raw, BYTE M, BYTE S, BYTE F);
};
