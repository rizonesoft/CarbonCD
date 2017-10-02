#include "StdAfx.h"
#include "mmcwriter.h"
#include "CDWriter.h"

#define PGB(a) ((BYTE *)(((DWORD)(a) + 0x0f) & ~0x0f))

CMMCWriter::CMMCWriter ( void )
{
    m_Aspi = NULL;
    m_LeadInLBA = 0;
    m_LeadInSize = 0;
    m_Buffer = m_Buffer1;
    m_FirstWriteFlag = true;
    m_WritingMode = 0;
    m_MaxFrames = 0;
    m_Buffer1 = m_PBuf1.CreateBuffer ( ( 2352 + 96 ) * 27 );
    m_Buffer2 = m_PBuf2.CreateBuffer ( ( 2352 + 96 ) * 27 );
}

CMMCWriter::~CMMCWriter ( void )
{
}

void CMMCWriter::Initialize ( CAspi * aspi )
{
    m_Aspi = aspi;
}

bool CMMCWriter::CheckDisc ( void )
{
    if ( m_Aspi == NULL )
        {
            return false;
        }

    CPBBuffer PBuffer;
    BYTE *Buffer;
    SRB_ExecSCSICmd cmd;
    Buffer = PBuffer.CreateBuffer ( 256 );
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 256;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_CDBLen = 10;
    cmd.SRB_Flags      = SRB_DIR_IN;
    cmd.CDBByte[ 0] = 0x51;     //   READ DISC INFORMATION
    cmd.CDBByte[ 7] = ( BYTE ) ( 255 >> 8 );
    cmd.CDBByte[ 8] = ( BYTE ) ( 255 & 0xff );
    m_Aspi->ExecuteCommand ( cmd );
    m_LeadInLBA = 0;
    m_LeadInSize = 0;

    if ( cmd.SRB_Status != SS_COMP )
        {
            m_SK = cmd.SenseArea[2] & 0x0f;
            m_ASC = cmd.SenseArea[12];
            m_ASCQ = cmd.SenseArea[13];
            return false;
        }

    if ( ( Buffer[2] & 0x03 ) != 0 )
        {
            m_SK = 0;
            m_ASC = 0;
            m_ASCQ = 0;
            return false;
        }

    m_LeadInLBA = Buffer[19] + 75 * ( Buffer[18] + 60 * ( Buffer[17] ) );
    m_LeadInSize = 450000 - ( Buffer[19] + 75 * ( Buffer[18] + 60 * ( Buffer[17] ) ) );
    return true;
}

DWORD CMMCWriter::GetLeadInLBA ( void )
{
    return m_LeadInLBA;
}

DWORD CMMCWriter::GetLeadInSize ( void )
{
    return m_LeadInSize;
}

bool CMMCWriter::SetWriteParam ( int WriteMode, bool BurnProof, bool TestMode, int BufferingFrames )
{
    if ( m_Aspi == NULL )
        {
            return false;
        }

    BYTE *Buffer, B[256 + 15];
    DWORD DataPoint;
    BYTE *mp;
    BYTE PageLen;
    Buffer = PGB ( B );
    DataPoint = m_Aspi->ModeSense ( Buffer, 256, 1, 5 ); //   get default setting

    if ( DataPoint == 0 )
        {
            return false;
        }

    mp = Buffer + DataPoint;

    if ( ( mp[2] & 0x40 ) == 0 )
        {
            BurnProof = false;
        }

    DataPoint = m_Aspi->ModeSense ( Buffer, 256, 2, 5 ); //   get default setting

    if ( DataPoint == 0 )
        {
            return false;
        }

    mp = Buffer + DataPoint;
    PageLen = mp[1] + 2;
    memset ( mp + 2, 0, 0x36 );

    if ( TestMode )
        {
            mp[2] |= 0x10;  //   set test mode
        }

    else
        {
            mp[2] &= ~0x10; //   set write mode
        }

    if ( BurnProof )
        {
            mp[2] |= 0x40;
        }

    else
        {
            mp[2] &= ~0x40;
        }

    if ( WriteMode == WRITEMODE_RAW_96 )
        {
            mp[2] = ( mp[2] & 0xf0 ) | 0x03; //   Write Type : RAW
            mp[4] = ( mp[4] & 0xf0 ) | 0x03; //   Data Block Type : 2352 bytes raw data + raw PW Sub channel
            m_FrameSize = 2352 + 96;
        }

    else
        if ( WriteMode == WRITEMODE_RAW_P96 )
            {
                mp[2] = ( mp[2] & 0xf0 ) | 0x03; //   Write Type : RAW
                mp[4] = ( mp[4] & 0xf0 ) | 0x02; //   Data Block Type : 2352 bytes raw data + packed PW Sub channel
                m_FrameSize = 2352 + 96;
            }

        else
            if ( WriteMode == WRITEMODE_RAW_16 )
                {
                    mp[2] = ( mp[2] & 0xf0 ) | 0x03; //   Write Type : RAW
                    mp[4] = ( mp[4] & 0xf0 ) | 0x01; //   Data Block Type : 2352 bytes raw data + PQ Sub channel
                    m_FrameSize = 2352 + 16;
                }

            else
                if ( WriteMode == WRITEMODE_2048 )
                    {
                        mp[2] = ( mp[2] & 0xf0 ) | 0x02; //   Write Type : Session at once
                        mp[4] = ( mp[4] & 0xf0 ) | 0x08; //   Data Block Type : 2048 bytes data
                        m_FrameSize = 2352;
                    }

                else
                    {
                        return false;
                    }

    if ( !m_Aspi->ModeSelect ( Buffer, PageLen + DataPoint ) )
        {
            return false;
        }

    m_BufferingFrames = 0;
    m_BufferLBA = 0;
    m_FirstWriteFlag = true;
    m_WritingMode = WriteMode;
    m_MaxFrames = BufferingFrames;

    if ( m_MaxFrames > 27 )
        {
            m_MaxFrames = 27;
        }

    m_Buffer = m_Buffer1;
    return true;
}

void CMMCWriter::ResetParams ( void )
{
    if ( m_Aspi == NULL )
        {
            return;
        }

    BYTE *Buffer, B[256 + 15];
    DWORD DataPoint;
    BYTE PageLen;
    Buffer = PGB ( B );
    DataPoint = m_Aspi->ModeSense ( Buffer, 256, 2, 5 ); //   get default setting

    if ( DataPoint == 0 )
        {
            return;
        }

    PageLen = Buffer[DataPoint + 1] + 2;
    m_Aspi->ModeSelect ( Buffer, PageLen + DataPoint );
    return;
}

bool CMMCWriter::WriteBuffering ( BYTE * Buffer, DWORD lba )
{
    bool RetValue;

    if ( m_BufferingFrames == 0 )
        {
            m_BufferPoint = 0;
            m_BufferLBA = lba;
        }

    memcpy ( m_Buffer + m_BufferPoint , Buffer , m_FrameSize );
    m_BufferingFrames ++;
    m_BufferPoint += m_FrameSize;
    RetValue = true;

    if ( m_BufferingFrames >= m_MaxFrames )
        {
            RetValue = Write ( m_Buffer, m_BufferLBA, m_BufferingFrames );
            m_BufferingFrames = 0;
            m_BufferPoint = 0;

            if ( m_Buffer == m_Buffer1 )
                {
                    m_Buffer = m_Buffer2;
                }

            else
                {
                    m_Buffer = m_Buffer1;
                }
        }

    return RetValue;
}

bool CMMCWriter::WriteBufferingEx ( BYTE * Buffer, DWORD lba, DWORD Size )
{
    bool RetValue;

    if ( m_BufferingFrames == 0 )
        {
            m_BufferPoint = 0;
            m_BufferLBA = lba;
        }

    memcpy ( m_Buffer + m_BufferPoint , Buffer , Size );
    m_BufferingFrames ++;
    m_BufferPoint += Size;
    RetValue = true;

    if ( m_BufferingFrames >= m_MaxFrames )
        {
            RetValue = Write ( m_Buffer, m_BufferLBA, m_BufferingFrames );
            m_BufferingFrames = 0;
            m_BufferPoint = 0;

            if ( m_Buffer == m_Buffer1 )
                {
                    m_Buffer = m_Buffer2;
                }

            else
                {
                    m_Buffer = m_Buffer1;
                }
        }

    return RetValue;
}

bool CMMCWriter::Write ( BYTE * Buffer, DWORD lba, DWORD WriteLen )
{
    SRB_ExecSCSICmd cmd;

    if ( m_Aspi == NULL )
        {
            return false;
        }

    if ( WriteLen == 0 )
        {
            return true;
        }

    DWORD BufferLength = m_BufferPoint;

    if ( m_FirstWriteFlag )
        {
            m_FirstWriteFlag = false;
            m_Aspi->InitialAsync();
        }

    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = BufferLength;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_CDBLen = 10;
    cmd.SRB_Flags      = SRB_DIR_OUT;
    cmd.CDBByte[ 0] = 0x2A;     //   WRITE 10
    cmd.CDBByte[ 1] = 0;
    cmd.CDBByte[ 2] = ( BYTE ) ( lba >> 24 );
    cmd.CDBByte[ 3] = ( BYTE ) ( lba >> 16 );
    cmd.CDBByte[ 4] = ( BYTE ) ( lba >> 8 );
    cmd.CDBByte[ 5] = ( BYTE ) ( lba >> 0 );
    cmd.CDBByte[ 6] = 0;
    cmd.CDBByte[ 7] = ( BYTE ) ( WriteLen >> 8 );
    cmd.CDBByte[ 8] = ( BYTE ) ( WriteLen >> 0 );
    m_Aspi->ExecuteCommandAsync ( cmd );

    if ( cmd.SRB_Status != SS_COMP )
        {
            m_SK = cmd.SenseArea[2] & 0x0f;
            m_ASC = cmd.SenseArea[12];
            m_ASCQ = cmd.SenseArea[13];
            return false;       //   bad status
        }

    return true;
}

int CMMCWriter::TestUnitReady ( void )
{
    if ( m_Aspi == NULL )
        {
            return false;
        }

    SRB_ExecSCSICmd cmd;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 0;
    cmd.SRB_BufPointer = NULL;
    cmd.SRB_CDBLen = 6;
    //  cmd.SRB_Flags      = SRB_DIR_IN;
    cmd.CDBByte[ 0] = 0x00;     //   TEST UNIT READY
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status != SS_COMP )
        {
            m_SK = cmd.SenseArea[2] & 0x0f;
            m_ASC = cmd.SenseArea[12];
            m_ASCQ = cmd.SenseArea[13];
            return 1;       //   bad status
        }

    if ( ( cmd.SenseArea[2] & 0x0f ) == 2 )
        {
            return 2;       //   not ready
        }

    if ( ( cmd.SenseArea[2] & 0x0f ) == 6 )
        {
            return 2;       //   illegal request
        }

    return 0;
}

bool CMMCWriter::FlushBuffer ( void )
{
    if ( m_Aspi == NULL )
        {
            return false;
        }

    //   write buffer
    if ( m_BufferingFrames != 0 )
        {
            Write ( m_Buffer, m_BufferLBA, m_BufferingFrames );
            m_BufferingFrames = 0;
        }

    m_Aspi->FinalizeAsync();
    SRB_ExecSCSICmd cmd;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 0;
    cmd.SRB_BufPointer = NULL;
    cmd.SRB_CDBLen = 10;
    cmd.SRB_Flags      = SRB_DIR_OUT;
    cmd.CDBByte[ 0] = 0x35;     //   Synchronize cache
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status != SS_COMP )
        {
            m_SK = cmd.SenseArea[2] & 0x0f;
            m_ASC = cmd.SenseArea[12];
            m_ASCQ = cmd.SenseArea[13];
            return false;       //   bad status
        }

    return true;
}

bool CMMCWriter::SetCueSheet ( BYTE * Buffer, int EntryNumber )
{
    if ( m_Aspi == NULL )
        {
            return false;
        }

    SRB_ExecSCSICmd cmd;
    DWORD BufferLength = EntryNumber * 8;
    CPBBuffer PBuffer;
    BYTE *CueBuffer;
    CueBuffer = PBuffer.CreateBuffer ( 2000 );
    //   patch -> modify cue-sheet
    {
        int i;
        BYTE *p;
        //   regenerate lead-in pregap
        memcpy ( CueBuffer, Buffer, 8 * EntryNumber );
        memset ( CueBuffer + 4, 0, 4 );

        for ( i = 1; i < EntryNumber; i++ )
            {
                p = CueBuffer + i * 8;

                if ( p[1] != 0xaa )
                    {
                        p[1] = ( ( p[1] >> 4 ) * 10 ) + ( p[1] & 0x0f );
                    }

                p[5] = ( ( p[5] >> 4 ) * 10 ) + ( p[5] & 0x0f );
                p[6] = ( ( p[6] >> 4 ) * 10 ) + ( p[6] & 0x0f );
                p[7] = ( ( p[7] >> 4 ) * 10 ) + ( p[7] & 0x0f );
            }
    }
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = BufferLength;
    cmd.SRB_BufPointer = CueBuffer;
    cmd.SRB_CDBLen = 10;
    cmd.SRB_Flags      = SRB_DIR_OUT;
    cmd.CDBByte[ 0] = 0x5D;     //   Send cue sheet(MMC format)
    cmd.CDBByte[ 6] = ( BYTE ) ( BufferLength >> 16 );
    cmd.CDBByte[ 7] = ( BYTE ) ( BufferLength >> 8 );
    cmd.CDBByte[ 8] = ( BYTE ) ( BufferLength >> 0 );
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status != SS_COMP )
        {
            m_SK = cmd.SenseArea[2] & 0x0f;
            m_ASC = cmd.SenseArea[12];
            m_ASCQ = cmd.SenseArea[13];
            return false;       //   bad status
        }

    return true;
}

bool CMMCWriter::PerformPowerCalibration ( void )
{
    if ( m_Aspi == NULL )
        {
            return false;
        }

    SRB_ExecSCSICmd cmd;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 0;
    cmd.SRB_BufPointer = NULL;
    cmd.SRB_CDBLen = 10;
    //  cmd.SRB_Flags      = SRB_DIR_OUT;
    cmd.CDBByte[ 0] = 0x54;     //   Send OPC information
    cmd.CDBByte[ 1] = 0x01;
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status != SS_COMP )
        {
            m_SK = cmd.SenseArea[2] & 0x0f;
            m_ASC = cmd.SenseArea[12];
            m_ASCQ = cmd.SenseArea[13];
            return false;       //   bad status
        }

    return true;
}

bool CMMCWriter::PreventMediaRemoval ( bool BlockFlag )
{
    if ( m_Aspi == NULL )
        {
            return false;
        }

    SRB_ExecSCSICmd cmd;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 0;
    cmd.SRB_BufPointer = NULL;
    cmd.SRB_CDBLen = 6;
    //  cmd.SRB_Flags      = SRB_DIR_OUT;
    cmd.CDBByte[ 0] = 0x1E;     //   Prevent Allow media removal

    if ( BlockFlag )
        {
            cmd.CDBByte[ 4] = 0x01;
        }

    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status != SS_COMP )
        {
            m_SK = cmd.SenseArea[2] & 0x0f;
            m_ASC = cmd.SenseArea[12];
            m_ASCQ = cmd.SenseArea[13];
            return false;       //   bad status
        }

    return true;
}

bool CMMCWriter::ReWind ( void )
{
    if ( m_Aspi == NULL )
        {
            return false;
        }

    SRB_ExecSCSICmd cmd;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 0;
    cmd.SRB_BufPointer = NULL;
    cmd.SRB_CDBLen = 6;
    //  cmd.SRB_Flags      = SRB_DIR_OUT;
    cmd.CDBByte[ 0] = 0x01;     //   REWIND
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status != SS_COMP )
        {
            m_SK = cmd.SenseArea[2] & 0x0f;
            m_ASC = cmd.SenseArea[12];
            m_ASCQ = cmd.SenseArea[13];
            return false;       //   bad status
        }

    return true;
}

bool CMMCWriter::LoadTray ( BYTE LoUnlo )
{
    if ( m_Aspi == NULL )
        {
            return false;
        }

    SRB_ExecSCSICmd cmd;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 0;
    cmd.SRB_BufPointer = NULL;
    cmd.SRB_CDBLen = 6;
    //  cmd.SRB_Flags      = SRB_DIR_OUT;
    cmd.CDBByte[ 0] = 0x1b;     //   LOAD/UNLOAD(SSC)
    cmd.CDBByte[ 4] = LoUnlo;
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status != SS_COMP )
        {
            m_SK = cmd.SenseArea[2] & 0x0f;
            m_ASC = cmd.SenseArea[12];
            m_ASCQ = cmd.SenseArea[13];
            return false;       //   bad status
        }

    return true;
}

bool CMMCWriter::IsCDR ( void )
{
    if ( m_Aspi == NULL )
        {
            return false;
        }

    BYTE *Buffer, B[256 + 15];
    DWORD DataPoint;
    BYTE *mp;
    Buffer = PGB ( B );
    DataPoint = m_Aspi->ModeSense ( Buffer, 256, 0, 0x2A ); //   get Drive feature

    if ( DataPoint == 0 )
        {
            return false;
        }

    mp = Buffer + DataPoint;

    if ( ( mp[3] & 0x03 ) == 0 )
        {
            return false;
        }

    return true;
}

int CMMCWriter::GetBufferSize ( void )
{
    if ( m_Aspi == NULL )
        {
            return 0;
        }

    BYTE *Buffer, B[256 + 15];
    DWORD DataPoint;
    BYTE *mp;
    Buffer = PGB ( B );
    DataPoint = m_Aspi->ModeSense ( Buffer, 256, 0, 0x2A ); //   get Drive feature

    if ( DataPoint == 0 )
        {
            return 0;
        }

    mp = Buffer + DataPoint;
    return mp[12] * 0x100 + mp[13];
    return 0;
}

bool CMMCWriter::SetWriteSpeed ( BYTE Speed )
{
    SRB_ExecSCSICmd cmd;
    DWORD PSpeed;
    PSpeed = ( Speed * 2352 * 75 ) / 1000;

    if ( Speed == 0xff )
        {
            PSpeed = 0xffff;
        }

    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 0;
    cmd.SRB_BufPointer = NULL;
    cmd.SRB_CDBLen = 12;
    //  cmd.SRB_Flags      = SRB_DIR_IN;
    cmd.CDBByte[ 0] = 0xbb;     //   Set cd speed
    cmd.CDBByte[ 1] = 0;
    //   read speed
    cmd.CDBByte[ 2] = 0xff;
    cmd.CDBByte[ 3] = 0xff;
    //   write speed
    cmd.CDBByte[ 4] = ( BYTE ) ( PSpeed >> 8 );
    cmd.CDBByte[ 5] = ( BYTE ) PSpeed;
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status == SS_COMP )
        {
            return true;
        }

    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCWriter::EraseMedia ( bool FastErase )
{
    SRB_ExecSCSICmd cmd;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 0;
    cmd.SRB_BufPointer = NULL;
    cmd.SRB_CDBLen = 12;
    //  cmd.SRB_Flags      = SRB_DIR_OUT;
    cmd.CDBByte[ 0] = 0xA1;     //   BLANK

    if ( FastErase )
        {
            cmd.CDBByte[ 1] = 1;
        }

    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status == SS_COMP )
        {
            return true;
        }

    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

void CMMCWriter::GetErrorParams ( BYTE &SK, BYTE &ASC, BYTE &ASCQ )
{
    SK = m_SK;
    ASC = m_ASC;
    ASCQ = m_ASCQ;
}

bool CMMCWriter::ReadTrackInfo ( BYTE * Buffer )
{
    SRB_ExecSCSICmd cmd;
    CPBBuffer PBuffer;
    PBuffer.CreateBuffer ( 36 );
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen  = 36;
    cmd.SRB_BufPointer = PBuffer.GetBuffer();
    cmd.SRB_CDBLen  = 10;
    cmd.SRB_Flags   = SRB_DIR_IN;
    cmd.CDBByte[ 0] = 0x52;     //   Read Track Information
    cmd.CDBByte[ 1] = 0x01;
    cmd.CDBByte[ 5] = 0xff;
    cmd.CDBByte[ 8] = 0x1c;
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status == SS_COMP )
        {
            memcpy ( Buffer, PBuffer.GetBuffer(), 36 );
            return true;
        }

    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}
