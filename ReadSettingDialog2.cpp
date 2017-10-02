#include "stdafx.h"
#include "cdm.h"
#include "ReadSettingDialog2.h"
#include "Setting.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_READS + i])

IMPLEMENT_DYNAMIC ( CReadSettingDialog2, CDialog )
CReadSettingDialog2::CReadSettingDialog2 ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CReadSettingDialog2::IDD, pParent )
    , m_TestRead ( FALSE )
    , m_ExtMenu ( FALSE )
    , m_AnalyzePregap ( FALSE )
    , m_AnalyzeSubQ ( FALSE )
    , m_IgnoreReaderror ( FALSE )
    , m_FastErrorskip ( FALSE )
    , m_BurstErrorScan ( FALSE )
{
}

CReadSettingDialog2::~CReadSettingDialog2()
{
}

void CReadSettingDialog2::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
    DDX_Check ( pDX, IDC_WRITENULL, m_TestRead );
    DDX_Check ( pDX, IDC_EXTMENU, m_ExtMenu );
    DDX_Check ( pDX, IDC_ANALYZE_PREGAP, m_AnalyzePregap );
    DDX_Check ( pDX, IDC_ANALYZE_SUBQ, m_AnalyzeSubQ );
    DDX_Check ( pDX, IDC_IGNORE_READERROR, m_IgnoreReaderror );
    DDX_Check ( pDX, IDC_FAST_ERRORSKIP, m_FastErrorskip );
    DDX_Check ( pDX, IDC_BURST_ERROR_SCAN, m_BurstErrorScan );
    DDX_Control ( pDX, IDC_AUDIOSPEED, m_AudioSpeed );
}


BEGIN_MESSAGE_MAP ( CReadSettingDialog2, CDialog )
    ON_BN_CLICKED ( IDC_EXTMENU, OnBnClickedExtmenu )
END_MESSAGE_MAP()


void CReadSettingDialog2::Load ( void )
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
    m_IgnoreReaderror = theSetting.m_IgnoreError;
    m_AnalyzePregap = theSetting.m_AnalyzePregap;

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

    UpdateData ( FALSE );
    OnBnClickedExtmenu();
}

void CReadSettingDialog2::Save ( void )
{
    UpdateData ( TRUE );
    theSetting.m_TestReadMode = ( m_TestRead == TRUE ) ? true : false;
    theSetting.m_FastErrorSkip = ( m_FastErrorskip == TRUE ) ? true : false;
    theSetting.m_AnalyzeSubQ = ( m_AnalyzeSubQ == TRUE ) ? true : false;
    theSetting.m_BurstErrorScan = m_BurstErrorScan;
    int SpeedList[5] = {0xff, 32, 8, 4, 1};
    theSetting.m_Speed_Audio = SpeedList[m_AudioSpeed.GetCurSel()];
    theSetting.m_AnalyzePregap = m_AnalyzePregap;
    theSetting.m_IgnoreError = ( m_IgnoreReaderror == TRUE ) ? true : false;
    theSetting.m_ExtSetting_Read = m_ExtMenu;
    UpdateData ( FALSE );
}

BOOL CReadSettingDialog2::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_AudioSpeed.AddString ( MSG ( 20 ) );
    m_AudioSpeed.AddString ( "x32" );
    m_AudioSpeed.AddString ( "x8" );
    m_AudioSpeed.AddString ( "x4" );
    m_AudioSpeed.AddString ( "x1" );
    UpdateData ( FALSE );
    SetLanguage();
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CReadSettingDialog2::OnBnClickedExtmenu()
{
    UpdateData ( TRUE );

    if ( m_SettingAtCopy )
        {
            GetDlgItem ( IDC_WRITENULL )->ShowWindow ( SW_HIDE );
        }

    if ( m_ExtMenu )
        {
            GetDlgItem ( IDC_ANALYZE_PREGAP    )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_ANALYZE_SUBQ      )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_IGNORE_READERROR  )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_FAST_ERRORSKIP    )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_BURST_ERROR_SCAN  )->ShowWindow ( SW_SHOW );
            UpdateData ( FALSE );
        }

    else
        {
            GetDlgItem ( IDC_ANALYZE_PREGAP    )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_ANALYZE_SUBQ      )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_IGNORE_READERROR  )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_FAST_ERRORSKIP    )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_BURST_ERROR_SCAN  )->ShowWindow ( SW_HIDE );
            UpdateData ( FALSE );
        }
}

void CReadSettingDialog2::SetLanguage ( void )
{
    DWORD CtrlStr[][2] =
    {
        {IDC_WRITENULL          , 4},
        {IDC_EXTMENU            , 9},
        {IDC_STATIC3            , 16},
        {IDC_ANALYZE_PREGAP     , 17},
        {IDC_ANALYZE_SUBQ       , 10},
        {IDC_IGNORE_READERROR   , 11},
        {IDC_FAST_ERRORSKIP     , 12},
        {IDC_BURST_ERROR_SCAN   , 13},
        {IDC_STATIC1            , 6},
    };
    int i;

    for ( i = 0; i < 9; i++ )
        {
            this->SetDlgItemText ( CtrlStr[i][0], STR ( CtrlStr[i][1] ) );
        }
}
