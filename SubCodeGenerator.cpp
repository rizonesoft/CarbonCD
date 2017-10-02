#include "StdAfx.h"
#include "subcodegenerator.h"

CSubCodeGenerator::CSubCodeGenerator ( void )
{
    int i, c, crc;

    //   generate crc table
    for ( i = 0; i < 256; i++ )
        {
            crc = i << 8;

            for ( c = 0; c < 8; c++ )
                {
                    if ( crc & 0x8000 )
                        { crc = ( crc << 1 ) ^ 0x1021; }

                    else
                        { crc = ( crc << 1 ); }
                }

            m_SubcodeCRCTable[i] = crc & 0xffff;
        }
}

CSubCodeGenerator::~CSubCodeGenerator ( void )
{
}

void CSubCodeGenerator::SetCueSheet ( const BYTE * MMCCueSheet, int EntryCount )
{
    m_CueSheet = MMCCueSheet;
    m_CueEntryCount = EntryCount;
    m_CurrentLBA = ToBin ( m_CueSheet[7] ) + 75 * ( ToBin ( m_CueSheet[6] ) + 60 * ToBin ( m_CueSheet[5] ) ) - 450000;
    m_RelativeLBA = 0;
    CreateToc();
    m_CurrentEntry = 0;
    m_TocCounter = 0;
    m_3Counter = 0;
    m_P_Count = 0;
}

DWORD CSubCodeGenerator::GenerateSub16 ( BYTE * Buffer )
{
    BYTE m, s, f;
    const BYTE *Cue;
    DWORD lba;
    //   check lead-out
    Cue = m_CueSheet + m_CurrentEntry * 8;

    if ( Cue[1] == 0xaa )
        {
            //   lead-out section
        }
    else
        {
            const BYTE *CueNext;
            //   check increment entry
            CueNext = m_CueSheet + m_CurrentEntry * 8 + 8;
            Cue = m_CueSheet + m_CurrentEntry * 8 + 8;
            lba = ToBin ( Cue[7] ) + 75 * ( ToBin ( Cue[6] ) + 60 * ToBin ( Cue[5] ) );

            if ( lba <= m_CurrentLBA && m_CurrentLBA < 0x80000000 )
                {
                    //   increment entry
                    m_CurrentEntry++;
                    Cue = m_CueSheet + m_CurrentEntry * 8;
                    CueNext = m_CueSheet + m_CurrentEntry * 8 + 8;

                    if ( Cue[1] != 0 && Cue[2] == 0 )
                        {
                            m_RelativeLBA = ( ToBin ( CueNext[7] ) + 75 * ( ToBin ( CueNext[6] ) + 60 * ToBin ( CueNext[5] ) ) )
                                            - ( ToBin ( Cue[7] ) + 75 * ( ToBin ( Cue[6] ) + 60 * ToBin ( Cue[5] ) ) );
                        }

                    else
                        {
                            m_RelativeLBA = 0;
                        }
                }

            if ( m_CurrentEntry >= m_CueEntryCount )
                {
                    m_CurrentEntry = m_CueEntryCount - 1;
                }
        }

    Cue = m_CueSheet + m_CurrentEntry * 8;
    memset ( Buffer, 0, 16 );

    if ( Cue[1] == 0 )
        {
            BYTE *Toc;
            //   Lead-In sub code (TOC)
            Toc = m_RawToc + m_TocCounter * 16;
            memset ( Buffer, 0, 16 );
            Buffer[0] = ( ( Toc[1] >> 4 ) | ( Toc[1] << 4 ) );
            Buffer[2] = Toc[3];
            LBAtoMSF ( m, s, f, m_CurrentLBA );
            Buffer[3] = ToHex ( m );
            Buffer[4] = ToHex ( s );
            Buffer[5] = ToHex ( f );
            Buffer[7] = Toc[8];
            Buffer[8] = Toc[9];
            Buffer[9] = Toc[10];
            m_CurrentLBA++;
            m_RelativeLBA++;
            m_3Counter++;

            if ( m_3Counter >= 3 )
                {
                    m_3Counter = 0;
                    m_TocCounter = ( m_TocCounter + 1 ) % m_TocCount;
                }
        }

    else
        {
            //   set CTR/ADR & TRACK & INDEX
            Buffer[0] = Cue[0];     //   CTR/ADR
            Buffer[1] = Cue[1];     //   TRACK
            Buffer[2] = Cue[2];     //   INDEX
            //   calc relative address
            LBAtoMSF ( m, s, f, m_RelativeLBA );
            Buffer[3] = ToHex ( m );
            Buffer[4] = ToHex ( s );
            Buffer[5] = ToHex ( f );
            //   calc absolute address
            LBAtoMSF ( m, s, f, m_CurrentLBA );
            Buffer[7] = ToHex ( m );
            Buffer[8] = ToHex ( s );
            Buffer[9] = ToHex ( f );

            //   set p channel
            if ( m_P_Count > 0 )
                {
                    Buffer[15] = 0x80;
                    m_P_Count--;
                }

            if ( Cue[2] == 0 )
                {
                    m_P_Count  = 1;
                }

            //   increment address
            if ( Cue[1] != 0xaa && Cue[2] == 0 )
                {
                    m_CurrentLBA++;
                    m_RelativeLBA--;
                }

            else
                {
                    m_CurrentLBA++;
                    m_RelativeLBA++;
                }
        }

    CalcCRC ( Buffer );
    return m_CurrentLBA - 1;
}

void CSubCodeGenerator::LBAtoMSF ( BYTE & m, BYTE & s, BYTE & f, DWORD lba )
{
    if ( lba > 0x80000000 )
        {
            lba += 450000;
        }

    m = ( BYTE ) ( lba / ( 75 * 60 ) );
    s = ( BYTE ) ( ( lba / 75 ) % 60 );
    f = ( BYTE ) ( lba % 75 );
}

void CSubCodeGenerator::CreateToc ( void )
{
    int i;
    const BYTE *Cue;
    BYTE *Toc;
    BYTE LastTrack, LastTNO;
    BYTE FirstADR, LastADR;
    BYTE M, S, F;

    for ( i = 0; i < m_CueEntryCount; i++ )
        {
            Cue = m_CueSheet + i * 8;

            if ( Cue[1] == 1 && Cue[2] == 1 )
                {
                    FirstADR = Cue[0];
                }

            if ( Cue[2] == 1 && Cue[1] != 0xaa )
                {
                    LastTrack = ToBin ( Cue[1] );
                    LastTNO = Cue[1];
                    LastADR = Cue[0];
                }

            if ( Cue[1] == 0xaa )
                {
                    M = Cue[5];
                    S = Cue[6];
                    F = Cue[7];
                }
        }

    //   set first track info
    Toc = m_RawToc + 16 * LastTrack;
    memset ( Toc, 0, 16 );
    Toc[0] = 1;     //   session no
    Toc[1] = ( 0x10 | ( FirstADR >> 4 ) ); //   ADR/CTL
    Toc[3] = 0xa0;  //   TRACK NO -> First Track
    Toc[8] = 1;
    //   set last track info
    Toc = m_RawToc + 16 * ( LastTrack + 1 );
    memset ( Toc, 0, 16 );
    Toc[0] = 1;     //   session no
    //  Toc[1] = (0x10 | (LastADR >> 4));   //   ADR/CTL
    Toc[1] = ( 0x10 | ( FirstADR >> 4 ) ); //   ADR/CTL
    Toc[3] = 0xa1;  //   TRACK NO -> Last Track
    Toc[8] = LastTNO;
    //   set Lead-Out position
    Toc = m_RawToc + 16 * ( LastTrack + 2 );
    memset ( Toc, 0, 16 );
    Toc[0] = 1;     //   session no
    Toc[1] = ( 0x10 | ( FirstADR >> 4 ) ); //   ADR/CTL
    Toc[3] = 0xA2;  //   TRACK NO -> Lead-Out
    Toc[ 8] = M;
    Toc[ 9] = S;
    Toc[10] = F;

    for ( i = 0; i < m_CueEntryCount; i++ )
        {
            Cue = m_CueSheet + i * 8;

            if ( Cue[2] == 1 && Cue[1] != 0xaa )
                {
                    BYTE tno;
                    tno = ToBin ( Cue[1] );
                    Toc = m_RawToc + 16 * ( tno - 1 );
                    memset ( Toc, 0, 16 );
                    Toc[ 0] = 0x01;     //   session no
                    Toc[ 1] = ( 0x10 | ( Cue[0] >> 4 ) ); //   ADR/CTL
                    Toc[ 2] = 0x00;     //   TNO
                    Toc[ 3] = Cue[1];   //   POINT
                    Toc[ 4] = 0x00;     //   Min
                    Toc[ 5] = 0x00;     //   Sec
                    Toc[ 6] = 0x00;     //   Frame
                    Toc[ 7] = 0x00;     //   Zero
                    Toc[ 8] = Cue[5];       //   Pmin
                    Toc[ 9] = Cue[6];       //   Psec
                    Toc[10] = Cue[7];       //   Pframe
                }
        }

    m_TocCount = LastTrack + 3;
}

BYTE CSubCodeGenerator::ToHex ( BYTE Bin )
{
    return ( Bin / 10 ) * 0x10 + ( Bin % 10 );
}

BYTE CSubCodeGenerator::ToBin ( BYTE Hex )
{
    return ( Hex / 0x10 ) * 10 + ( Hex % 0x10 );
}

void CSubCodeGenerator::CalcCRC ( BYTE * Buffer )
{
    WORD crc;
    register BYTE index;
    int i;
    //   fast crc calcuration
    crc = 0;

    for ( i = 0; i < 10; i++ )
        {
            index = ( BYTE ) ( Buffer[i] ^ ( crc >> 8 ) );
            crc = m_SubcodeCRCTable[index] ^ ( crc << 8 );
        }

    Buffer[10] = ~ ( BYTE ) ( crc >> 8 );
    Buffer[11] = ~ ( BYTE ) ( crc );
}
