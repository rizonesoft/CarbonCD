#include "StdAfx.h"
#include "cuesheetparser.h"
#include "Setting.h"

CCueSheetParser::CCueSheetParser ( void )
{
    m_DataMode = 1;
}

CCueSheetParser::~CCueSheetParser ( void )
{
}

bool CCueSheetParser::Parse ( LPCSTR CueSheet, LPCSTR ImageDir, DWORD ImageSize )
{
    int Line;
    int i;
    m_CueSheet = CueSheet;
    m_CuePoint = CueSheet;
    m_ImageDir = ImageDir;
    m_ImageFile = "";
    m_ImageSize = ImageSize;

    if ( m_ImageDir.GetLength() > 0 && m_ImageDir[m_ImageDir.GetLength() - 1] != '\\' )
        {
            m_ImageDir += "\\";
        }

    for ( i = 0; i < 99; i++ )
        {
            m_Cue[i].m_Frames = 0x0;
            m_Cue[i].m_PregapLBA = 0xffffffff;
            m_Cue[i].m_StartLBA = 0xffffffff;
            m_Cue[i].m_PregapFrames = 0x0;
            m_Cue[i].m_TrackType = TRACK_AUDIO;
        }

    Line = 1;
    m_TrackCount = 0;

    while ( AbstractCueLine() )
        {
            if ( StringCompare ( m_CueLine, "TRACK", 5 ) )
                {
                    LPCSTR p;
                    int TrackNo;
                    int TrackType;
                    p = ( ( LPCSTR ) m_CueLine ) + 5;

                    while ( *p == ' ' || *p == '\t' ) { p++; }

                    TrackNo = 0;

                    while ( *p >= '0' && *p <= '9' )
                        {
                            TrackNo *= 10;
                            TrackNo += *p - '0';
                            p++;
                        }

                    if ( TrackNo != m_TrackCount + 1 )
                        {
                            m_ErrorMessage.Format ( MSG ( 7 ), m_CueLine );
                            return false;
                        }

                    while ( *p == ' ' || *p == '\t' ) { p++; }

                    if ( StringCompare ( p, "AUDIO", 5 ) )
                        {
                            TrackType = TRACK_AUDIO;
                        }

                    else
                        if ( StringCompare ( p, "MODE1/2352", 10 ) )
                            {
                                TrackType = TRACK_MODE1_RAW;
                                m_DataMode = 1;
                            }

                        else
                            if ( StringCompare ( p, "MODE2/2352", 10 ) )
                                {
                                    TrackType = TRACK_MODE1_RAW;
                                    m_DataMode = 2;
                                }

                            else
                                {
                                    m_ErrorMessage.Format ( MSG ( 8 ), m_CueLine );
                                    return false;
                                }

                    m_TrackCount = TrackNo;
                    m_Cue[m_TrackCount - 1].m_TrackType = TrackType;
                }

            else
                if ( StringCompare ( m_CueLine, "INDEX", 5 ) )
                    {
                        LPCSTR p;
                        int IndexNo, m, s, f;
                        p = ( ( LPCSTR ) m_CueLine ) + 5;

                        while ( *p == ' ' || *p == '\t' ) { p++; }

                        IndexNo = 0;

                        while ( *p >= '0' && *p <= '9' )
                            {
                                IndexNo *= 10;
                                IndexNo += *p - '0';
                                p++;
                            }

                        while ( *p == ' ' || *p == '\t' ) { p++; }

                        m = ( p[0] - '0' ) * 10 + p[1] - '0';
                        s = ( p[3] - '0' ) * 10 + p[4] - '0';
                        f = ( p[6] - '0' ) * 10 + p[7] - '0';

                        if ( m_TrackCount <= 0 )
                            {
                                m_ErrorMessage.Format ( MSG ( 9 ), m_CueLine );
                                return false;
                            }

                        if ( IndexNo == 0 )
                            {
                                m_Cue[m_TrackCount - 1].m_PregapLBA = f + 75 * ( s + 60 * ( m ) );
                            }

                        else
                            if ( IndexNo == 1 )
                                {
                                    m_Cue[m_TrackCount - 1].m_StartLBA = f + 75 * ( s + 60 * ( m ) );
                                }

                            else
                                {
                                    m_ErrorMessage.Format ( MSG ( 10 ), m_CueLine );
                                    return false;
                                }
                    }

                else
                    if ( StringCompare ( m_CueLine, "FILE", 4 ) )
                        {
                            LPCSTR p;
                            CString ImageFile;
                            p = ( ( LPCSTR ) m_CueLine ) + 4;

                            while ( *p == ' ' || *p == '\t' ) { p++; }

                            if ( *p != '\"' )
                                {
                                    m_ErrorMessage.Format ( MSG ( 11 ), m_CueLine );
                                    return false;
                                }

                            p++;
                            ImageFile = "";

                            while ( *p != '\"' )
                                {
                                    if ( *p == '\0' )
                                        {
                                            m_ErrorMessage.Format ( MSG ( 12 ), m_CueLine );
                                            return false;
                                        }

                                    ImageFile += *p;
                                    p++;
                                }

                            p++;

                            while ( *p == ' ' || *p == '\t' ) { p++; }

                            if ( !StringCompare ( p, "BINARY", 6 ) )
                                {
                                    m_ErrorMessage.Format ( MSG ( 13 ), m_CueLine );
                                    return false;
                                }

                            if ( ImageFile.Find ( '\\', 0 ) == -1 )
                                {
                                    //   local path
                                    m_ImageFile = m_ImageDir + ImageFile;
                                }

                            else
                                {
                                    //   full path
                                    m_ImageFile = ImageFile;
                                }

                            if ( m_ImageSize == 0 )
                                {
                                    HANDLE hFile;
                                    hFile = CreateFile ( m_ImageFile, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

                                    if ( hFile == INVALID_HANDLE_VALUE )
                                        {
                                            m_ErrorMessage.Format ( MSG ( 14 ), m_ImageFile );
                                            return false;
                                        }

                                    m_ImageSize = GetFileSize ( hFile, NULL );
                                    CloseHandle ( hFile );
                                }
                        }

                    else
                        {
                            m_ErrorMessage.Format ( MSG ( 15 ), m_CueLine );
                            return false;
                        }

            Line++;
        }

    if ( !CheckCue() )
        {
            return false;
        }

    return true;
}

bool CCueSheetParser::AbstractCueLine ( void )
{
    m_CueLine = "";

    while ( *m_CuePoint == ' ' || *m_CuePoint == '\n' || *m_CuePoint == '\r' || *m_CuePoint == '\t' )
        {
            m_CuePoint++;
        }

    if ( *m_CuePoint == '\0' )
        {
            return false;
        }

    while ( *m_CuePoint != '\n' && *m_CuePoint != '\r' && *m_CuePoint != '\0' )
        {
            m_CueLine += *m_CuePoint;
            m_CuePoint++;
        }

    return true;
}

bool CCueSheetParser::StringCompare ( LPCSTR String1, LPCSTR String2, int Length )
{
    while ( Length > 0 )
        {
            if ( *String1 != *String2 && ( *String1 - 0x20 ) != *String2 && *String1 != ( *String2 - 0x20 ) )
                {
                    return false;
                }

            Length--;
            String1++;
            String2++;
        }

    return true;
}

LPCSTR CCueSheetParser::GetErrorMessage ( void )
{
    return m_ErrorMessage;
}

bool CCueSheetParser::CheckCue ( void )
{
    int i;
    m_Cue[m_TrackCount].m_StartLBA = m_ImageSize / 2352;

    for ( i = 0; i <= m_TrackCount; i++ )
        {
            if ( m_Cue[i].m_StartLBA == 0xffffffff )
                {
                    m_ErrorMessage.Format ( MSG ( 16 ), i + 1 );
                    return false;
                }

            if ( i > 0 )
                {
                    if ( m_Cue[i].m_PregapLBA == 0xffffffff )
                        {
                            m_Cue[i - 1].m_Frames = m_Cue[i].m_StartLBA - m_Cue[i - 1].m_StartLBA;
                        }

                    else
                        {
                            m_Cue[i - 1].m_Frames = m_Cue[i].m_PregapLBA - m_Cue[i - 1].m_StartLBA;
                        }
                }

            if ( m_Cue[i].m_PregapLBA != 0xffffffff )
                {
                    m_Cue[i].m_PregapFrames = m_Cue[i].m_StartLBA - m_Cue[i].m_PregapLBA;
                }

            else
                {
                    m_Cue[i].m_PregapFrames = 0;
                }
        }

    return true;
}

int CCueSheetParser::CreateMMCCueSheet ( DWORD LeadInLBA )
{
    int Entry, i, lba;
    lba = ( ( int ) LeadInLBA );
    //   Lead-In cue
    Entry = 0;
    m_MMCCue[Entry + 0] = 0x01;     //   CTL/ADR
    m_MMCCue[Entry + 1] = 0x00;     //   Track NO
    m_MMCCue[Entry + 2] = 0x00;     //   INDEX
    m_MMCCue[Entry + 3] = 0x01;     //   Data Form
    m_MMCCue[Entry + 4] = 0x00;     //   Serial Copy Management System
    m_MMCCue[Entry + 5] = ToHex ( ( BYTE ) ( LeadInLBA / ( 60 * 75 ) ) ); //   Absolute Minute
    m_MMCCue[Entry + 6] = ToHex ( ( BYTE ) ( ( LeadInLBA / 75 ) % 60 ) ); //   Absolute Second
    m_MMCCue[Entry + 7] = ToHex ( ( BYTE ) ( LeadInLBA % 75 ) );    //   Absolute Frame

    if ( m_Cue[0].m_TrackType == TRACK_MODE1_RAW )
        {
            m_MMCCue[Entry + 0] = 0x41;     //   CTL/ADR
            m_MMCCue[Entry + 3] = 0x14;     //   Data Form
        }

    //   Pregap for first track cue
    Entry = 8;
    m_MMCCue[Entry + 0] = 0x01;     //   CTL/ADR
    m_MMCCue[Entry + 1] = 0x01;     //   Track NO
    m_MMCCue[Entry + 2] = 0x00;     //   INDEX
    m_MMCCue[Entry + 3] = 0x00;     //   Data Form
    m_MMCCue[Entry + 4] = 0x00;     //   Serial Copy Management System
    m_MMCCue[Entry + 5] = 0x00;     //   Absolute Minute
    m_MMCCue[Entry + 6] = 0x00;     //   Absolute Second
    m_MMCCue[Entry + 7] = 0x00;     //   Absolute Frame

    if ( m_Cue[0].m_TrackType == TRACK_MODE1_RAW )
        {
            m_MMCCue[Entry + 0] = 0x41;     //   CTL/ADR
            m_MMCCue[Entry + 3] = 0x10;     //   Data Form
        }

    //   cues for each track
    Entry = 16;

    for ( i = 0; i < m_TrackCount; i++ )
        {
            if ( m_Cue[i].m_PregapLBA != 0xffffffff )
                {
                    m_MMCCue[Entry + 0] = 0x01;     //   CTL/ADR
                    m_MMCCue[Entry + 1] = ToHex ( i + 1 );  //   Track NO
                    m_MMCCue[Entry + 2] = 0x00;     //   INDEX
                    m_MMCCue[Entry + 3] = 0x00;     //   Data Form
                    m_MMCCue[Entry + 4] = 0x00;     //   Serial Copy Management System
                    m_MMCCue[Entry + 5] = ToHex ( ( BYTE ) ( ( m_Cue[i].m_PregapLBA + 150 ) / ( 60 * 75 ) ) ); //   Absolute Minute
                    m_MMCCue[Entry + 6] = ToHex ( ( BYTE ) ( ( ( m_Cue[i].m_PregapLBA + 150 ) / 75 ) % 60 ) ); //   Absolute Second
                    m_MMCCue[Entry + 7] = ToHex ( ( BYTE ) ( ( m_Cue[i].m_PregapLBA + 150 ) % 75 ) ); //   Absolute Frame

                    if ( m_Cue[i].m_TrackType == TRACK_MODE1_RAW )
                        {
                            m_MMCCue[Entry + 0] = 0x41;     //   CTL/ADR
                            m_MMCCue[Entry + 3] = 0x10;     //   Data Form
                        }

                    Entry += 8;
                }

            m_MMCCue[Entry + 0] = 0x01;     //   CTL/ADR
            m_MMCCue[Entry + 1] = ToHex ( i + 1 );  //   Track NO
            m_MMCCue[Entry + 2] = 0x01;     //   INDEX
            m_MMCCue[Entry + 3] = 0x00;     //   Data Form
            m_MMCCue[Entry + 4] = 0x00;     //   Serial Copy Management System
            m_MMCCue[Entry + 5] = ToHex ( ( BYTE ) ( ( m_Cue[i].m_StartLBA + 150 ) / ( 60 * 75 ) ) ); //   Absolute Minute
            m_MMCCue[Entry + 6] = ToHex ( ( BYTE ) ( ( ( m_Cue[i].m_StartLBA + 150 ) / 75 ) % 60 ) ); //   Absolute Second
            m_MMCCue[Entry + 7] = ToHex ( ( BYTE ) ( ( m_Cue[i].m_StartLBA + 150 ) % 75 ) ); //   Absolute Frame

            if ( m_Cue[i].m_TrackType == TRACK_MODE1_RAW )
                {
                    m_MMCCue[Entry + 0] = 0x41;     //   CTL/ADR
                    m_MMCCue[Entry + 3] = 0x10;     //   Data Form
                }

            Entry += 8;
        }

    //   Lead-Out cue
    m_MMCCue[Entry + 0] = 0x01;     //   CTL/ADR
    m_MMCCue[Entry + 1] = 0xaa;     //   Track NO
    m_MMCCue[Entry + 2] = 0x01;     //   INDEX
    m_MMCCue[Entry + 3] = 0x01;     //   Data Form
    m_MMCCue[Entry + 4] = 0x00;     //   Serial Copy Management System
    m_MMCCue[Entry + 5] = ToHex ( ( BYTE ) ( ( m_Cue[m_TrackCount].m_StartLBA + 150 ) / ( 60 * 75 ) ) ); //   Absolute Minute
    m_MMCCue[Entry + 6] = ToHex ( ( BYTE ) ( ( ( m_Cue[m_TrackCount].m_StartLBA + 150 ) / 75 ) % 60 ) ); //   Absolute Second
    m_MMCCue[Entry + 7] = ToHex ( ( BYTE ) ( ( m_Cue[m_TrackCount].m_StartLBA + 150 ) % 75 ) ); //   Absolute Frame
    Entry += 8;
    m_MMCCueEntry = Entry / 8;
    return m_MMCCueEntry;
}

BYTE * CCueSheetParser::GetMMCCueSheet ( void )
{
    return m_MMCCue;
}

int CCueSheetParser::GetEntryCount ( void )
{
    return m_MMCCueEntry;
}

DWORD CCueSheetParser::GetTotalFrames ( void )
{
    return m_ImageSize / 2352;
}

BYTE CCueSheetParser::ToHex ( BYTE Data )
{
    return ( Data / 10 ) * 0x10 + ( Data % 10 );
}

LPCSTR CCueSheetParser::GetImageFileName ( void )
{
    return ( LPCSTR ) m_ImageFile;
}

int CCueSheetParser::GetDataMode ( void )
{
    return m_DataMode;
}
