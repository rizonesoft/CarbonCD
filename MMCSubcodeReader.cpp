#include "stdafx.h"
#include "mmcsubcodereader.h"

CMMCSubcodeReader::CMMCSubcodeReader ( void )
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

    m_Aspi = NULL;
    m_TransBCD = false;
}

CMMCSubcodeReader::~CMMCSubcodeReader ( void )
{
}

void CMMCSubcodeReader::Initialize ( CAspi * Aspi )
{
    m_Aspi = Aspi;
}

bool CMMCSubcodeReader::ReadRaw16 ( MSFAddress Address, BYTE * Buffer )
{
    SRB_ExecSCSICmd cmd;

    if ( m_Aspi == NULL ) { return false; }

    DWORD lba;
    lba = Address.Frame + 75 * ( Address.Second + 60 * ( Address.Minute ) ) - 150;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 2352 + 96;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags      = SRB_DIR_IN;
    cmd.CDBByte[ 0] = 0xBE;     //   READ CD LBA
    cmd.CDBByte[ 2] = ( BYTE ) ( lba >> 24 );
    cmd.CDBByte[ 3] = ( BYTE ) ( lba >> 16 );
    cmd.CDBByte[ 4] = ( BYTE ) ( lba >> 8 );
    cmd.CDBByte[ 5] = ( BYTE ) ( lba );
    cmd.CDBByte[ 6] = 0;
    cmd.CDBByte[ 7] = 0;
    cmd.CDBByte[ 8] = 1;
    cmd.CDBByte[ 9] = 0xF8;     //   SYNC/AllHeaders/UserData/EDC&ECC
    cmd.CDBByte[10] = 0x02;     //   Sub 16
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status == SS_COMP )
        {
            BYTE Tmp[16], *Sub;
            Sub = Buffer + 2352;
            memcpy ( Tmp, Sub, 16 );

            if ( ( Tmp[0] & 0x0f ) == 1 && m_TransBCD )
                {
                    Tmp[ 1] = ( Tmp[ 1] % 10 ) + ( ( Tmp[ 1] / 10 ) * 16 );
                    Tmp[ 3] = ( Tmp[ 3] % 10 ) + ( ( Tmp[ 3] / 10 ) * 16 );
                    Tmp[ 4] = ( Tmp[ 4] % 10 ) + ( ( Tmp[ 4] / 10 ) * 16 );
                    Tmp[ 5] = ( Tmp[ 5] % 10 ) + ( ( Tmp[ 5] / 10 ) * 16 );
                    Tmp[ 7] = ( Tmp[ 7] % 10 ) + ( ( Tmp[ 7] / 10 ) * 16 );
                    Tmp[ 8] = ( Tmp[ 8] % 10 ) + ( ( Tmp[ 8] / 10 ) * 16 );
                    Tmp[ 9] = ( Tmp[ 9] % 10 ) + ( ( Tmp[ 9] / 10 ) * 16 );
                    CalcCRC ( Tmp );
                }

            memset ( Sub, 0, 96 );
            memcpy ( Sub + 12, Tmp, 12 );

            if ( Tmp[15] == 0x80 )
                {
                    memset ( Sub, 0xff, 12 );
                }

            else
                if ( Tmp[15] != 0 && Tmp[2] == 0 )
                    {
                        memset ( Sub, 0xff, 12 );
                    }

            return true;
        }

    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCSubcodeReader::ReadCD16 ( MSFAddress Address, BYTE * Buffer )
{
    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * ( Address.Second + 60 * ( Address.Minute ) ) - 150;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 2352 + 16;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags      = SRB_DIR_IN;
    cmd.CDBByte[ 0] = 0xB9;     //   READ CD MSF
    cmd.CDBByte[ 3] = Address.Minute;
    cmd.CDBByte[ 4] = Address.Second;
    cmd.CDBByte[ 5] = Address.Frame;
    Address = Address.GetByLBA() + 1;
    cmd.CDBByte[ 6] = Address.Minute;
    cmd.CDBByte[ 7] = Address.Second;
    cmd.CDBByte[ 8] = Address.Frame;
    cmd.CDBByte[ 9] = 0xF8;     //   SYNC/AllHeaders/UserData/EDC&ECC
    cmd.CDBByte[10] = 0x02;     //   Sub 16
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status == SS_COMP )
        {
            BYTE Tmp[16], *Sub;
            Sub = Buffer + 2352;
            memcpy ( Tmp, Sub, 16 );

            if ( ( Tmp[0] & 0x0f ) == 1 && m_TransBCD )
                {
                    Tmp[ 1] = ( Tmp[ 1] % 10 ) + ( ( Tmp[ 1] / 10 ) * 16 );
                    Tmp[ 3] = ( Tmp[ 3] % 10 ) + ( ( Tmp[ 3] / 10 ) * 16 );
                    Tmp[ 4] = ( Tmp[ 4] % 10 ) + ( ( Tmp[ 4] / 10 ) * 16 );
                    Tmp[ 5] = ( Tmp[ 5] % 10 ) + ( ( Tmp[ 5] / 10 ) * 16 );
                    Tmp[ 7] = ( Tmp[ 7] % 10 ) + ( ( Tmp[ 7] / 10 ) * 16 );
                    Tmp[ 8] = ( Tmp[ 8] % 10 ) + ( ( Tmp[ 8] / 10 ) * 16 );
                    Tmp[ 9] = ( Tmp[ 9] % 10 ) + ( ( Tmp[ 9] / 10 ) * 16 );
                    CalcCRC ( Tmp );
                }

            memset ( Sub, 0, 96 );
            memcpy ( Sub + 12, Tmp, 12 );

            if ( Tmp[15] == 0x80 )
                {
                    memset ( Sub, 0xff, 12 );
                }

            else
                if ( Tmp[15] != 0 && Tmp[2] == 0 )
                    {
                        memset ( Sub, 0xff, 12 );
                    }

            return true;
        }

    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCSubcodeReader::ReadCDDA16 ( MSFAddress Address, BYTE * Buffer )
{
    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * ( Address.Second + 60 * ( Address.Minute ) ) - 150;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 2352 + 16;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags      = SRB_DIR_IN;
    cmd.CDBByte[ 1] = 0x04;     //   CDDA Sector
    cmd.CDBByte[ 0] = 0xBE;     //   READ CD LBA
    cmd.CDBByte[ 2] = ( BYTE ) ( lba >> 24 );
    cmd.CDBByte[ 3] = ( BYTE ) ( lba >> 16 );
    cmd.CDBByte[ 4] = ( BYTE ) ( lba >> 8 );
    cmd.CDBByte[ 5] = ( BYTE ) ( lba );
    cmd.CDBByte[ 6] = 0;
    cmd.CDBByte[ 7] = 0;
    cmd.CDBByte[ 8] = 1;
    cmd.CDBByte[ 9] = 0x10;     //   UserData
    cmd.CDBByte[10] = 0x02;     //   Sub 16
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status == SS_COMP )
        {
            BYTE Tmp[16], *Sub;
            Sub = Buffer + 2352;
            memcpy ( Tmp, Sub, 16 );

            if ( ( Tmp[0] & 0x0f ) == 1 && m_TransBCD )
                {
                    Tmp[ 1] = ( Tmp[ 1] % 10 ) + ( ( Tmp[ 1] / 10 ) * 16 );
                    Tmp[ 3] = ( Tmp[ 3] % 10 ) + ( ( Tmp[ 3] / 10 ) * 16 );
                    Tmp[ 4] = ( Tmp[ 4] % 10 ) + ( ( Tmp[ 4] / 10 ) * 16 );
                    Tmp[ 5] = ( Tmp[ 5] % 10 ) + ( ( Tmp[ 5] / 10 ) * 16 );
                    Tmp[ 7] = ( Tmp[ 7] % 10 ) + ( ( Tmp[ 7] / 10 ) * 16 );
                    Tmp[ 8] = ( Tmp[ 8] % 10 ) + ( ( Tmp[ 8] / 10 ) * 16 );
                    Tmp[ 9] = ( Tmp[ 9] % 10 ) + ( ( Tmp[ 9] / 10 ) * 16 );
                    CalcCRC ( Tmp );
                }

            memset ( Sub, 0, 96 );
            memcpy ( Sub + 12, Tmp, 12 );

            if ( Tmp[15] == 0x80 )
                {
                    memset ( Sub, 0xff, 12 );
                }

            else
                if ( Tmp[15] != 0 && Tmp[2] == 0 )
                    {
                        memset ( Sub, 0xff, 12 );
                    }

            return true;
        }

    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCSubcodeReader::ReadRaw96 ( MSFAddress Address, BYTE * Buffer )
{
    SRB_ExecSCSICmd cmd;

    if ( m_Aspi == NULL ) { return false; }

    DWORD lba;
    lba = Address.GetByLBA() - 150;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 2352 + 96;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags      = SRB_DIR_IN;
    cmd.CDBByte[ 0] = 0xBE;     //   READ CD LBA
    cmd.CDBByte[ 1] = 0x00;     //   All Sector types
    cmd.CDBByte[ 2] = ( BYTE ) ( lba >> 24 );
    cmd.CDBByte[ 3] = ( BYTE ) ( lba >> 16 );
    cmd.CDBByte[ 4] = ( BYTE ) ( lba >> 8 );
    cmd.CDBByte[ 5] = ( BYTE ) ( lba );
    cmd.CDBByte[ 6] = 0;
    cmd.CDBByte[ 7] = 0;
    cmd.CDBByte[ 8] = 1;
    cmd.CDBByte[ 9] = 0xF8;     //   SYNC/AllHeaders/UserData/EDC&ECC
    cmd.CDBByte[10] = 0x01;     //   Sub 96
    //  cmd.CDBByte[10] = 0x04;     //   Sub 96
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status == SS_COMP )
        {
            //*
            BYTE Tmp[96], *Sub, *p, *q;
            Sub = Buffer + 2352;
            int i, j, k;
            memcpy ( Tmp, Sub, 96 );
            memset ( Sub, 0, 96 );

            for ( k = 0; k < 12; k++ )
                {
                    q = Tmp + k * 8;

                    for ( j = 7; j >= 0; j-- )
                        {
                            p = Sub + k + j * 12;
                            *p |= ( q[7] & 1 );
                            *p |= ( q[6] & 1 ) << 1;
                            *p |= ( q[5] & 1 ) << 2;
                            *p |= ( q[4] & 1 ) << 3;
                            *p |= ( q[3] & 1 ) << 4;
                            *p |= ( q[2] & 1 ) << 5;
                            *p |= ( q[1] & 1 ) << 6;
                            *p |= ( q[0] & 1 ) << 7;

                            for ( i = 0; i < 8; i++ )
                                {
                                    q[i] = q[i] >> 1;
                                }
                        }
                }//*/

            return true;
        }

    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCSubcodeReader::ReadCD96 ( MSFAddress Address, BYTE * Buffer )
{
    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * ( Address.Second + 60 * ( Address.Minute ) ) - 150;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 2352 + 96;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags      = SRB_DIR_IN;
    cmd.CDBByte[ 0] = 0xB9;     //   READ CD MSF
    cmd.CDBByte[ 1] = 0x00;     //   All Sector types
    cmd.CDBByte[ 3] = Address.Minute;
    cmd.CDBByte[ 4] = Address.Second;
    cmd.CDBByte[ 5] = Address.Frame;
    Address = Address.GetByLBA() + 1;
    cmd.CDBByte[ 6] = Address.Minute;
    cmd.CDBByte[ 7] = Address.Second;
    cmd.CDBByte[ 8] = Address.Frame;
    cmd.CDBByte[ 9] = 0xF8;     //   SYNC/AllHeaders/UserData/EDC&ECC
    cmd.CDBByte[10] = 0x01;     //   Sub 96
    //  cmd.CDBByte[10] = 0x04;     //   Sub 96
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status == SS_COMP )
        {
            //*
            BYTE Tmp[96], *Sub, *p, *q;
            Sub = Buffer + 2352;
            int i, j, k;
            memcpy ( Tmp, Sub, 96 );
            memset ( Sub, 0, 96 );

            for ( k = 0; k < 12; k++ )
                {
                    q = Tmp + k * 8;

                    for ( j = 7; j >= 0; j-- )
                        {
                            p = Sub + k + j * 12;
                            *p |= ( q[7] & 1 );
                            *p |= ( q[6] & 1 ) << 1;
                            *p |= ( q[5] & 1 ) << 2;
                            *p |= ( q[4] & 1 ) << 3;
                            *p |= ( q[3] & 1 ) << 4;
                            *p |= ( q[2] & 1 ) << 5;
                            *p |= ( q[1] & 1 ) << 6;
                            *p |= ( q[0] & 1 ) << 7;

                            for ( i = 0; i < 8; i++ )
                                {
                                    q[i] = q[i] >> 1;
                                }
                        }
                }//*/

            return true;
        }

    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCSubcodeReader::ReadCDDA96 ( MSFAddress Address, BYTE * Buffer )
{
    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * ( Address.Second + 60 * ( Address.Minute ) ) - 150;
    memset ( &cmd, 0, sizeof ( cmd ) );
    cmd.SRB_BufLen = 2352 + 96;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags      = SRB_DIR_IN;
    cmd.CDBByte[ 0] = 0xBE;     //   READ CD LBA
    cmd.CDBByte[ 1] = 0x04;     //   CDDA Sector
    cmd.CDBByte[ 2] = ( BYTE ) ( lba >> 24 );
    cmd.CDBByte[ 3] = ( BYTE ) ( lba >> 16 );
    cmd.CDBByte[ 4] = ( BYTE ) ( lba >> 8 );
    cmd.CDBByte[ 5] = ( BYTE ) ( lba );
    cmd.CDBByte[ 6] = 0;
    cmd.CDBByte[ 7] = 0;
    cmd.CDBByte[ 8] = 1;
    cmd.CDBByte[ 9] = 0x10;     //   UserData
    cmd.CDBByte[10] = 0x01;     //   Sub 96
    //  cmd.CDBByte[10] = 0x04;     //   Sub 96
    m_Aspi->ExecuteCommand ( cmd );

    if ( cmd.SRB_Status == SS_COMP )
        {
            //*
            BYTE Tmp[96], *Sub, *p, *q;
            Sub = Buffer + 2352;
            int i, j, k;
            memcpy ( Tmp, Sub, 96 );
            memset ( Sub, 0, 96 );

            for ( k = 0; k < 12; k++ )
                {
                    q = Tmp + k * 8;

                    for ( j = 7; j >= 0; j-- )
                        {
                            p = Sub + k + j * 12;
                            *p |= ( q[7] & 1 );
                            *p |= ( q[6] & 1 ) << 1;
                            *p |= ( q[5] & 1 ) << 2;
                            *p |= ( q[4] & 1 ) << 3;
                            *p |= ( q[3] & 1 ) << 4;
                            *p |= ( q[2] & 1 ) << 5;
                            *p |= ( q[1] & 1 ) << 6;
                            *p |= ( q[0] & 1 ) << 7;

                            for ( i = 0; i < 8; i++ )
                                {
                                    q[i] = q[i] >> 1;
                                }
                        }
                }//*/

            return true;
        }

    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

void CMMCSubcodeReader::SetBCDMode ( bool TransBCD )
{
    m_TransBCD = TransBCD;
}

void CMMCSubcodeReader::CalcCRC ( BYTE * SubQ )
{
    WORD crc;
    register BYTE index;
    int i;
    //   fast crc calcuration
    crc = 0;

    for ( i = 0; i < 10; i++ )
        {
            index = ( BYTE ) ( SubQ[i] ^ ( crc >> 8 ) );
            crc = m_SubcodeCRCTable[index] ^ ( crc << 8 );
        }

    SubQ[10] = ~ ( BYTE ) ( crc >> 8 );
    SubQ[11] = ~ ( BYTE ) ( crc );
}
