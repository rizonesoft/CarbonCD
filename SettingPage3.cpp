#include "stdafx.h"
#include "cdm.h"
#include "SettingPage3.h"
#include "Setting.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_SET + i])

IMPLEMENT_DYNAMIC ( CSettingPage3, CDialog )
CSettingPage3::CSettingPage3 ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CSettingPage3::IDD, pParent )
    , m_BufferSizeText ( _T ( "" ) )
    , m_UseSPTI ( FALSE )
    , m_SetAspi ( FALSE )
    , m_AspiDLL ( _T ( "" ) )
{
}

CSettingPage3::~CSettingPage3()
{
}

void CSettingPage3::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
    DDX_Control ( pDX, IDC_BUFFERSIZE, m_BufferSize );
    DDX_Text ( pDX, IDC_BUFFERSIZE_TXT, m_BufferSizeText );
    DDX_Check ( pDX, IDC_USE_SPTI, m_UseSPTI );
    DDX_Check ( pDX, IDC_SET_ASPI, m_SetAspi );
    DDX_Text ( pDX, IDC_ASPI_DLL, m_AspiDLL );
}


BEGIN_MESSAGE_MAP ( CSettingPage3, CDialog )
    ON_NOTIFY ( NM_CUSTOMDRAW, IDC_BUFFERSIZE, OnNMCustomdrawBuffersize )
    ON_BN_CLICKED ( IDC_BROWS_ASPI, OnBnClickedBrowsAspi )
    ON_BN_CLICKED ( IDC_SET_ASPI, OnBnClickedSetAspi )
    ON_BN_CLICKED ( IDC_USE_SPTI, OnBnClickedUseSpti )
END_MESSAGE_MAP()


void CSettingPage3::Save ( void )
{
    theSetting.m_Write_Buffer = m_BufferSize.GetPos();

    if ( m_SetAspi == FALSE )
        {
            m_AspiDLL = "";
        }

    m_ChangedASPI = false;

    if ( m_AspiDLL != theSetting.m_AspiDLL )
        {
            theSetting.m_AspiDLL = m_AspiDLL;
            m_ChangedASPI = true;
        }

    if ( m_UseSPTI != theSetting.m_UseSPTI )
        {
            theSetting.m_UseSPTI = m_UseSPTI;
            m_ChangedASPI = true;
        }
}

BOOL CSettingPage3::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_BufferSize.SetRange ( 5, 27, TRUE );
    m_BufferSize.SetTicFreq ( 1 );
    m_BufferSize.SetPos ( theSetting.m_Write_Buffer );
    m_AspiDLL = theSetting.m_AspiDLL;

    if ( m_AspiDLL == "" )
        {
            m_SetAspi = FALSE;
            GetDlgItem ( IDC_ASPI_DLL )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_BROWS_ASPI )->ShowWindow ( SW_HIDE );
        }

    else
        {
            m_SetAspi = TRUE;
        }

    m_UseSPTI = theSetting.m_UseSPTI;
    {
        OSVERSIONINFO osver;
        osver.dwOSVersionInfoSize = sizeof ( osver );

        if ( GetVersionEx ( &osver ) )
            {
                if ( osver.dwPlatformId != VER_PLATFORM_WIN32_NT )
                    {
                        GetDlgItem ( IDC_USE_SPTI )->ShowWindow ( SW_HIDE );
                        m_UseSPTI = 0;
                    }
            }
    }
    UpdateData ( FALSE );
    SetLanguage();
    OnBnClickedUseSpti();
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CSettingPage3::SetLanguage ( void )
{
    DWORD CtrlStr[][2] =
    {
        {IDC_SET_ASPI       , 1},
        {IDC_STATIC2        , 4},
        {IDC_BROWS_ASPI     , 7},
        {IDC_USE_SPTI       , 12},
    };
    int i;

    for ( i = 0; i < 4; i++ )
        {
            SetDlgItemText ( CtrlStr[i][0], STR ( CtrlStr[i][1] ) );
        }
}

void CSettingPage3::OnNMCustomdrawBuffersize ( NMHDR *pNMHDR, LRESULT *pResult )
{
    LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW> ( pNMHDR );
    *pResult = 0;
    UpdateData ( TRUE );
    m_BufferSizeText.Format ( "%dKByte", m_BufferSize.GetPos() * 2352 / 1024 );
    UpdateData ( FALSE );
}

void CSettingPage3::OnBnClickedBrowsAspi()
{
    CFileDialog dlg ( FALSE, ".cue", NULL, OFN_HIDEREADONLY, MSG ( 125 ) );
    UpdateData ( TRUE );

    if ( dlg.DoModal() == IDOK )
        {
            m_AspiDLL = dlg.GetPathName();
        }

    UpdateData ( FALSE );
}

void CSettingPage3::OnBnClickedSetAspi()
{
    UpdateData ( TRUE );

    if ( m_SetAspi == TRUE )
        {
            GetDlgItem ( IDC_ASPI_DLL )->ShowWindow ( SW_SHOW );
            GetDlgItem ( IDC_BROWS_ASPI )->ShowWindow ( SW_SHOW );
        }

    else
        {
            GetDlgItem ( IDC_ASPI_DLL )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_BROWS_ASPI )->ShowWindow ( SW_HIDE );
        }

    UpdateData ( FALSE );
}

void CSettingPage3::OnBnClickedUseSpti()
{
    UpdateData ( TRUE );

    if ( m_UseSPTI )
        {
            GetDlgItem ( IDC_SET_ASPI )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_ASPI_DLL )->ShowWindow ( SW_HIDE );
            GetDlgItem ( IDC_BROWS_ASPI )->ShowWindow ( SW_HIDE );
        }

    else
        {
            GetDlgItem ( IDC_SET_ASPI )->ShowWindow ( SW_SHOW );

            if ( m_SetAspi )
                {
                    GetDlgItem ( IDC_ASPI_DLL )->ShowWindow ( SW_SHOW );
                    GetDlgItem ( IDC_BROWS_ASPI )->ShowWindow ( SW_SHOW );
                }

            else
                {
                    GetDlgItem ( IDC_ASPI_DLL )->ShowWindow ( SW_HIDE );
                    GetDlgItem ( IDC_BROWS_ASPI )->ShowWindow ( SW_HIDE );
                }
        }

    UpdateData ( FALSE );
}
