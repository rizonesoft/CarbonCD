#include "stdafx.h"
#include "CDM.h"
#include "ReadTrackDialog.h"

#include "Setting.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_READS + i])
#define STRT(i) (theSetting.m_Lang.m_Str[LP_READT + i])


IMPLEMENT_DYNAMIC ( CReadTrackDialog, CDialog )
CReadTrackDialog::CReadTrackDialog ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CReadTrackDialog::IDD, pParent )
    , m_ImageName ( _T ( "" ) )
    , m_Toc ( NULL )
    , m_DriveName ( _T ( "" ) )
    , m_IgnoreReaderror ( FALSE )
    , m_SwapChannel ( FALSE )
    , m_AutoDetectReadMethod ( FALSE )
    , m_BurstErrorScan ( FALSE )
    , m_ExtMenu ( FALSE )
{
}

CReadTrackDialog::~CReadTrackDialog()
{
}

void CReadTrackDialog::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
    DDX_Text ( pDX, IDC_IMAGENAME, m_ImageName );
    DDX_Control ( pDX, IDC_FILELIST, m_FileList );
    DDX_Text ( pDX, IDC_DRIVENAME, m_DriveName );
    DDX_Check ( pDX, IDC_IGNORE_READERROR, m_IgnoreReaderror );
    DDX_Check ( pDX, IDC_SWAPCHANNEL, m_SwapChannel );
    DDX_Check ( pDX, IDC_AUTODETECT_COMMAND, m_AutoDetectReadMethod );
    DDX_Control ( pDX, IDC_AUDIOMETHOD, m_AudioMethod );
    DDX_Control ( pDX, IDC_AUDIOSPEED, m_AudioSpeed );
    DDX_Control ( pDX, IDC_DATASPEED2, m_DataSpeed );
    DDX_Check ( pDX, IDC_BURST_ERROR_SCAN, m_BurstErrorScan );
    DDX_Check ( pDX, IDC_EXTMENU, m_ExtMenu );
}


BEGIN_MESSAGE_MAP ( CReadTrackDialog, CDialog )
    ON_BN_CLICKED ( IDC_BROWS, OnBnClickedBrows )
    ON_BN_CLICKED ( IDOK, OnBnClickedOk )
    ON_BN_CLICKED ( IDCANCEL, OnBnClickedCancel )
    ON_BN_CLICKED ( IDC_AUTODETECT_COMMAND, OnBnClickedAutodetectCommand )
    ON_BN_CLICKED ( IDC_EXTMENU, OnBnClickedExtmenu )
END_MESSAGE_MAP()


void CReadTrackDialog::OnBnClickedBrows()
{
    CFileDialog dlg ( FALSE, NULL, NULL, 0, MSG ( 118 ) );
    UpdateData ( TRUE );

    if ( dlg.DoModal() == IDOK )
        {
            char Buffer[512];
            LPSTR p;
            CString cs;
            int i;

            while ( m_FileList.DeleteString ( 0 ) != LB_ERR );

            lstrcpy ( Buffer, dlg.GetPathName() );
            p = Buffer + lstrlen ( Buffer );

            while ( p > Buffer && *p != '.' )  { p--; }

            if ( *p == '.' )   { *p = '\0'; }

            m_ImageName.Format ( "%s_", Buffer );

            for ( i = 0; i < m_Toc->m_LastTrack; i++ )
                {
                    if ( m_Toc->m_Track[i].m_SelectFlag )
                        {
                            if ( m_Toc->m_Track[i].m_TrackType == TRACKTYPE_AUDIO )
                                {
                                    cs.Format ( "%s%02d.wav", m_ImageName, i + 1 );
                                }

                            else
                                {
                                    cs.Format ( "%s%02d.iso", m_ImageName, i + 1 );
                                }

                            m_FileList.InsertString ( m_FileList.GetCount(), cs );
                        }
                }
        }

    UpdateData ( FALSE );
}

int CReadTrackDialog::DoModalOriginal ( TableOfContents * Toc )
{
    m_Toc = Toc;
    return ( int ) DoModal();
}

BOOL CReadTrackDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_BurstErrorScan = theSetting.m_BurstErrorScan;
    //   from setting.cpp
    m_AudioSpeed.AddString ( MSG ( 20 ) );
    m_AudioSpeed.AddString ( "x32" );
    m_AudioSpeed.AddString ( "x8" );
    m_AudioSpeed.AddString ( "x4" );
    m_AudioSpeed.AddString ( "x1" );
    m_DataSpeed.AddString ( MSG ( 20 ) );
    m_DataSpeed.AddString ( "x32" );
    m_DataSpeed.AddString ( "x8" );
    m_DataSpeed.AddString ( "x4" );
    m_DataSpeed.AddString ( "x1" );
    m_AudioMethod.AddString ( "READ D8" );
    m_AudioMethod.AddString ( "MMC READ CDDA LBA" );
    m_AudioMethod.AddString ( "MMC READ CDDA MSF" );
    m_AudioMethod.AddString ( "MMC LBA" );
    m_AudioMethod.AddString ( "MMC MSF" );
    m_AudioMethod.AddString ( "MMC LBA(RAW)" );
    m_AudioMethod.AddString ( "MMC MSF(RAW)" );
    m_AudioMethod.AddString ( "READ(10)" );
    m_AudioMethod.AddString ( "READ D4(10)" );
    m_AudioMethod.AddString ( "READ D4(12)" );
    m_AudioMethod.AddString ( "READ D5" );

    if ( theSetting.m_Speed_Audio <= 1 )
        {
            m_AudioSpeed.SetCurSel ( 4 );
        }

    else
        if ( theSetting.m_Speed_Audio <= 4 )
            {
                m_AudioSpeed.SetCurSel ( 3 );
            }

        else
            if ( theSetting.m_Speed_Audio <= 8 )
                {
                    m_AudioSpeed.SetCurSel ( 2 );
                }

            else
                if ( theSetting.m_Speed_Audio <= 32 )
                    {
                        m_AudioSpeed.SetCurSel ( 1 );
                    }

                else
                    {
                        m_AudioSpeed.SetCurSel ( 0 );
                    }

    if ( theSetting.m_Speed_Data <= 1 )
        {
            m_DataSpeed.SetCurSel ( 4 );
        }

    else
        if ( theSetting.m_Speed_Data <= 4 )
            {
                m_DataSpeed.SetCurSel ( 3 );
            }

        else
            if ( theSetting.m_Speed_Data <= 8 )
                {
                    m_DataSpeed.SetCurSel ( 2 );
                }

            else
                if ( theSetting.m_Speed_Data <= 32 )
                    {
                        m_DataSpeed.SetCurSel ( 1 );
                    }

                else
                    {
                        m_DataSpeed.SetCurSel ( 0 );
                    }

    m_AudioMethod.SetCurSel ( theSetting.m_ReadAudioMethod );
    m_IgnoreReaderror = theSetting.m_IgnoreError;
    m_SwapChannel = theSetting.m_SwapChannel;
    m_AutoDetectReadMethod = theSetting.m_AutoDetectMethod;
    m_ExtMenu = theSetting.m_ExtSetting_Read;
    UpdateData ( FALSE );
    SetLanguage();
    OnBnClickedExtmenu();
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CReadTrackDialog::OnBnClickedOk()
{
    UpdateData ( TRUE );

    if ( m_ImageName == "" )
        {
            MessageBox ( MSG ( 119 ), CONF_MSG );
            return;
        }

    //   from setting.cpp
    int SpeedList[5] = {0xff, 32, 8, 4, 1};
    theSetting.m_Speed_Audio = SpeedList[m_AudioSpeed.GetCurSel()];
    theSetting.m_Speed_Data = SpeedList[m_DataSpeed.GetCurSel()];
    theSetting.m_ReadAudioMethod = m_AudioMethod.GetCurSel();
    theSetting.m_BurstErrorScan = m_BurstErrorScan;
    theSetting.m_IgnoreError = ( m_IgnoreReaderror == TRUE ) ? true : false;
    theSetting.m_SwapChannel = ( m_SwapChannel == TRUE ) ? true : false;
    theSetting.m_AutoDetectMethod = ( m_AutoDetectReadMethod == TRUE ) ? true : false;
    theSetting.m_ExtSetting_Read = m_ExtMenu;
    OnOK();
}

void CReadTrackDialog::OnBnClickedCancel()
{
    UpdateData ( TRUE );
    //   from setting.cpp
    int SpeedList[5] = {0xff, 32, 8, 4, 1};
    theSetting.m_Speed_Audio = SpeedList[m_AudioSpeed.GetCurSel()];
    theSetting.m_Speed_Data = SpeedList[m_DataSpeed.GetCurSel()];
    theSetting.m_ReadAudioMethod = m_AudioMethod.GetCurSel();
    theSetting.m_BurstErrorScan = m_BurstErrorScan;
    theSetting.m_IgnoreError = ( m_IgnoreReaderror == TRUE ) ? true : false;
    theSetting.m_SwapChannel = ( m_SwapChannel == TRUE ) ? true : false;
    theSetting.m_AutoDetectMethod = ( m_AutoDetectReadMethod == TRUE ) ? true : false;
    theSetting.m_ExtSetting_Read = m_ExtMenu;
    OnCancel();
}

void CReadTrackDialog::OnBnClickedAutodetectCommand()
{
    UpdateData ( TRUE );

    if ( m_AutoDetectReadMethod )
        {
            GetDlgItem ( IDC_AUDIOMETHOD )->ShowWindow ( SW_HIDE );
        }

    else
        {
            GetDlgItem ( IDC_AUDIOMETHOD )->ShowWindow ( SW_SHOW );
        }

    UpdateData ( FALSE );
}

void CReadTrackDialog::OnBnClickedExtmenu()
{
    UpdateData ( TRUE );

    if ( m_ExtMenu )
        {
            GetDlgItem ( IDC_IGNORE_READERROR  )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_BURST_ERROR_SCAN  )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_SWAPCHANNEL       )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_AUTODETECT_COMMAND )->ShowWindow ( SW_SHOW );
            UpdateData ( FALSE );
            OnBnClickedAutodetectCommand();
        }

    else
        {
            GetDlgItem ( IDC_IGNORE_READERROR  )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_BURST_ERROR_SCAN  )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_SWAPCHANNEL       )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_AUTODETECT_COMMAND )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_AUDIOMETHOD       )->ShowWindow ( SW_HIDE );
            UpdateData ( FALSE );
        }
}

void CReadTrackDialog::SetLanguage ( void )
{
    DWORD CtrlStr[][2] =
    {
        {IDC_STATIC1            , 1},
        {IDC_BROWS              , 3},
        {IDC_STATIC3            , 7},
        {IDC_STATIC4            , 8},
        {IDC_EXTMENU            , 9},
        {IDC_IGNORE_READERROR   , 11},
        {IDC_BURST_ERROR_SCAN   , 13},
        {IDC_SWAPCHANNEL        , 14},
        {IDC_AUTODETECT_COMMAND , 15},
    };
    int i;
    SetWindowText ( STRT ( 0 ) );
    SetDlgItemText ( IDOK, theSetting.m_Lang.m_Str[1] );
    SetDlgItemText ( IDCANCEL, theSetting.m_Lang.m_Str[2] );

    for ( i = 0; i < 9; i++ )
        {
            this->SetDlgItemText ( CtrlStr[i][0], STR ( CtrlStr[i][1] ) );
        }

    this->SetDlgItemText ( IDC_STATIC2, STRT ( 1 ) );
}
