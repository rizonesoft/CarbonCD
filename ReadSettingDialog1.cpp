#include "stdafx.h"
#include "cdm.h"
#include "ReadSettingDialog1.h"
#include "Setting.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_READS + i])

IMPLEMENT_DYNAMIC ( CReadSettingDialog1, CDialog )
CReadSettingDialog1::CReadSettingDialog1 ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CReadSettingDialog1::IDD, pParent )
    , m_TestRead ( FALSE )
    , m_ExtMenu ( FALSE )
    , m_AnalyzeSubQ ( FALSE )
    , m_IgnoreReaderror ( FALSE )
    , m_FastErrorskip ( FALSE )
    , m_BurstErrorScan ( FALSE )
    , m_SwapChannel ( FALSE )
    , m_AutoDetectReadMethod ( FALSE )
{
}

CReadSettingDialog1::~CReadSettingDialog1()
{
}

void CReadSettingDialog1::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
    DDX_Check ( pDX, IDC_WRITENULL, m_TestRead );
    DDX_Check ( pDX, IDC_EXTMENU, m_ExtMenu );
    DDX_Check ( pDX, IDC_ANALYZE_SUBQ, m_AnalyzeSubQ );
    DDX_Check ( pDX, IDC_IGNORE_READERROR, m_IgnoreReaderror );
    DDX_Check ( pDX, IDC_FAST_ERRORSKIP, m_FastErrorskip );
    DDX_Check ( pDX, IDC_BURST_ERROR_SCAN, m_BurstErrorScan );
    DDX_Check ( pDX, IDC_SWAPCHANNEL, m_SwapChannel );
    DDX_Check ( pDX, IDC_AUTODETECT_COMMAND, m_AutoDetectReadMethod );
    DDX_Control ( pDX, IDC_DATASPEED2, m_DataSpeed );
    DDX_Control ( pDX, IDC_AUDIOSPEED, m_AudioSpeed );
    DDX_Control ( pDX, IDC_AUDIOMETHOD, m_AudioMethod );
}


BEGIN_MESSAGE_MAP ( CReadSettingDialog1, CDialog )
    ON_BN_CLICKED ( IDC_EXTMENU, OnBnClickedExtmenu )
    ON_BN_CLICKED ( IDC_AUTODETECT_COMMAND, OnBnClickedAutodetectCommand )
END_MESSAGE_MAP()


void CReadSettingDialog1::Load ( void )
{
    UpdateData ( TRUE );

    if ( m_SettingAtCopy )
        {
            m_TestRead = false;
        }

    else
        {
            m_TestRead = theSetting.m_TestReadMode;
        }

    m_FastErrorskip = theSetting.m_FastErrorSkip;
    m_AnalyzeSubQ = theSetting.m_AnalyzeSubQ;
    m_BurstErrorScan = theSetting.m_BurstErrorScan;
    m_ExtMenu = theSetting.m_ExtSetting_Read;

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
    UpdateData ( FALSE );
    OnBnClickedExtmenu();
}

void CReadSettingDialog1::Save ( void )
{
    UpdateData ( TRUE );
    theSetting.m_TestReadMode = ( m_TestRead == TRUE ) ? true : false;
    theSetting.m_FastErrorSkip = ( m_FastErrorskip == TRUE ) ? true : false;
    theSetting.m_AnalyzeSubQ = ( m_AnalyzeSubQ == TRUE ) ? true : false;
    theSetting.m_BurstErrorScan = m_BurstErrorScan;
    //   from setting.cpp
    int SpeedList[5] = {0xff, 32, 8, 4, 1};
    theSetting.m_Speed_Audio = SpeedList[m_AudioSpeed.GetCurSel()];
    theSetting.m_Speed_Data = SpeedList[m_DataSpeed.GetCurSel()];
    theSetting.m_ReadAudioMethod = m_AudioMethod.GetCurSel();
    theSetting.m_IgnoreError = ( m_IgnoreReaderror == TRUE ) ? true : false;
    theSetting.m_SwapChannel = ( m_SwapChannel == TRUE ) ? true : false;
    theSetting.m_AutoDetectMethod = ( m_AutoDetectReadMethod == TRUE ) ? true : false;
    theSetting.m_ExtSetting_Read = m_ExtMenu;
    UpdateData ( FALSE );
}

BOOL CReadSettingDialog1::OnInitDialog()
{
    CDialog::OnInitDialog();

    if ( m_SettingAtCopy )
        {
            GetDlgItem ( IDC_WRITENULL )->ShowWindow ( SW_HIDE );
        }

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
    UpdateData ( FALSE );
    SetLanguage();
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CReadSettingDialog1::OnBnClickedExtmenu()
{
    UpdateData ( TRUE );

    if ( m_ExtMenu )
        {
            GetDlgItem ( IDC_SWAPCHANNEL       )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_AUTODETECT_COMMAND )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_ANALYZE_SUBQ      )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_IGNORE_READERROR  )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_FAST_ERRORSKIP    )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_BURST_ERROR_SCAN  )->ShowWindow ( SW_SHOW );
            UpdateData ( FALSE );
            OnBnClickedAutodetectCommand();
        }

    else
        {
            GetDlgItem ( IDC_SWAPCHANNEL       )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_AUTODETECT_COMMAND )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_AUDIOMETHOD       )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_ANALYZE_SUBQ      )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_IGNORE_READERROR  )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_FAST_ERRORSKIP    )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_BURST_ERROR_SCAN  )->ShowWindow ( SW_HIDE );
            UpdateData ( FALSE );
        }
}

void CReadSettingDialog1::OnBnClickedAutodetectCommand()
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

void CReadSettingDialog1::SetLanguage ( void )
{
    DWORD CtrlStr[][2] =
    {
        {IDC_WRITENULL          , 4},
        {IDC_EXTMENU            , 9},
        {IDC_STATIC3            , 7},
        {IDC_STATIC4            , 8},
        {IDC_ANALYZE_SUBQ       , 10},
        {IDC_IGNORE_READERROR   , 11},
        {IDC_FAST_ERRORSKIP     , 12},
        {IDC_BURST_ERROR_SCAN   , 13},
        {IDC_SWAPCHANNEL        , 14},
        {IDC_AUTODETECT_COMMAND , 15},
        {IDC_STATIC1            , 5},
    };
    int i;

    for ( i = 0; i < 11; i++ )
        {
            this->SetDlgItemText ( CtrlStr[i][0], STR ( CtrlStr[i][1] ) );
        }
}
