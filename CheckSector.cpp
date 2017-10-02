// CheckSector.cpp: CCheckSector クラスのインプリメンテーション
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CheckSector.h"
typedef struct tMode1Frame
{
    BYTE Sync[12];
    BYTE Min;
    BYTE Sec;
    BYTE Frame;
    BYTE Mode;
    BYTE Data[2048];
    DWORD Edc;
    BYTE Zero[8];
    BYTE ECC_P[172];
    BYTE ECC_Q[104];
} Mode1Frame;

CCheckSector::CCheckSector()
{
    //   create 32bit crc table for EDC
    {
        DWORD i, j, r;

        for ( i = 0; i < 256; i++ )
            {
                r = i;

                for ( j = 0; j < 8; j++ )
                    {
                        if ( r & 1 )
                            {
                                r = ( r >> 1 ) ^ 0xD8018001;
                            }

                        else
                            {
                                r = r >> 1;
                            }
                    }

                m_EdcCRCTable[i] = r;
            }
    }
    //   create crc table for ECC
    {
        WORD crc;
        int i;
        memset ( m_ParityCRC_Table, 0, 256 );
        memset ( m_ParityCRC_RevTable, 0, 256 );
        crc = 2;

        for ( i = 1; i < 255; i++ )
            {
                m_ParityCRC_Table[ ( BYTE ) i] = ( BYTE ) crc;
                m_ParityCRC_RevTable[ ( BYTE ) crc] = ( BYTE ) i;

                if ( crc & 0x80 )
                    {
                        crc = ( crc << 1 ) ^ 0x11D;
                    }

                else
                    {
                        crc = crc << 1;
                    }
            }

        m_ParityCRC_Table[0] = 1;
    }
    //   create intermediate transition table for ECC
    {
        int j;
        BYTE TmpMatrix[2][45];

        for ( j = 0; j < 45; j++ )
            {
                TmpMatrix[0][j] = m_ParityCRC_Table[j];

                if ( m_ParityCRC_Table[j] != 0 )
                    {
                        TmpMatrix[1][j] = m_ParityCRC_Table[j] ^ 1;
                    }

                else
                    {
                        TmpMatrix[1][j] = 1;
                    }
            }

        for ( j = 44; j >= 1; j-- )
            {
                if ( TmpMatrix[1][j] != 0 )
                    {
                        TmpMatrix[1][j] = m_ParityCRC_Table[ ( m_ParityCRC_RevTable[TmpMatrix[1][j]] + 230 ) % 255];
                    }

                else
                    {
                        TmpMatrix[1][j] = 0;
                    }

                if ( j < 0 || j >= 2 )
                    {
                        m_ParityMatrix[0][44 - j] = m_ParityCRC_RevTable[TmpMatrix[1][j]];
                    }

                if ( TmpMatrix[1][j] != 0 )
                    {
                        TmpMatrix[0][j] = TmpMatrix[0][j] ^ m_ParityCRC_Table[ ( m_ParityCRC_RevTable[TmpMatrix[1][j]] + 1 ) % 255];
                    }
            }

        for ( j = 44; j >= 0; j-- )
            {
                if ( j >= ( 2 ) )
                    {
                        m_ParityMatrix[1][44 - j] = m_ParityCRC_RevTable[TmpMatrix[0][j]];
                    }

                else
                    {
                        break;
                    }
            }
    }
}

CCheckSector::~CCheckSector()
{
}

void CCheckSector::Mode1Raw ( BYTE *raw, BYTE M, BYTE S, BYTE F )
{
    BYTE rM, rS, rF;
    Mode1Frame *frame;
    frame = ( Mode1Frame* ) raw;
    memset ( frame->Sync, 0xff, 12 );
    frame->Sync[0] = 0;
    frame->Sync[11] = 0;
    rM = ( ( M / 10 ) * 0x10 ) + ( M % 10 );
    rS = ( ( S / 10 ) * 0x10 ) + ( S % 10 );
    rF = ( ( F / 10 ) * 0x10 ) + ( F % 10 );
    frame->Min = rM;
    frame->Sec = rS;
    frame->Frame = rF;
    frame->Mode = 1;
    frame->Edc = CalcEDC ( raw );
    memset ( frame->Zero, 0, 8 );
    memset ( m_EccP, 0, 172 );
    CalcEccP ( ( BYTE* ) raw + 12 );
    memcpy ( frame->ECC_P, m_EccP, 172 );
    memset ( m_EccQ, 0, 104 );
    CalcEccQ ( ( BYTE* ) raw + 12 );
    memcpy ( frame->ECC_Q, m_EccQ, 104 );
}

DWORD CCheckSector::CalcEDC ( BYTE *Buffer )
{
    DWORD result = 0;
    int i;

    for ( i = 0; i < 2064; i++ )
        {
            result = m_EdcCRCTable[ ( result ^ Buffer[i] ) & 0xff] ^ ( result >> 8 );
        }

    return result;
}

void CCheckSector::CalcEccP ( BYTE *buffer )
{
    int i, j, k;

    for ( j = 0; j < 43; j++ )
        {
            for ( i = 0; i < 24; i++ )
                {
                    for ( k = 0; k < 2; k++ )
                        {
                            if ( buffer[i * 86 + k + j * 2] )
                                {
                                    m_EccP[j * 2 +  0 + k] ^= m_ParityCRC_Table[ ( m_ParityCRC_RevTable[buffer[i * 86 + k + j * 2]] + m_ParityMatrix[0][i + 19] ) % 255];
                                    m_EccP[j * 2 + 86 + k] ^= m_ParityCRC_Table[ ( m_ParityCRC_RevTable[buffer[i * 86 + k + j * 2]] + m_ParityMatrix[1][i + 19] ) % 255];
                                }
                        }
                }
        }

    return;
}

void CCheckSector::CalcEccQ ( BYTE *buffer )
{
    int i, j, k;

    for ( j = 0; j < 26; j++ )
        {
            for ( i = 0; i < 43; i++ )
                {
                    for ( k = 0; k < 2; k++ )
                        {
                            if ( buffer[ ( j * 86 + i * 88 + k ) % 2236] )
                                {
                                    m_EccQ[j * 2 +  0 + k] ^= m_ParityCRC_Table[ ( m_ParityCRC_RevTable[buffer[ ( j * 86 + i * 88 + k ) % 2236]] + m_ParityMatrix[0][i] ) % 255];
                                    m_EccQ[j * 2 + 52 + k] ^= m_ParityCRC_Table[ ( m_ParityCRC_RevTable[buffer[ ( j * 86 + i * 88 + k ) % 2236]] + m_ParityMatrix[1][i] ) % 255];
                                }
                        }
                }
        }

    return;
}
