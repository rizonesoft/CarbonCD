#include "stdafx.h"
#include "CDM.h"
#include "Setting.h"
#include "SettingDialog.h"
#include "ThemeController.h"


#define STR(i) (theSetting.m_Lang.m_Str[LP_SET + i])


IMPLEMENT_DYNAMIC ( CSettingDialog, CDialog )
CSettingDialog::CSettingDialog ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CSettingDialog::IDD, pParent )
{
}

CSettingDialog::~CSettingDialog()
{
}

void CSettingDialog::DoDataExchange ( CDataExchange* pDX )
{
    DDX_Control ( pDX, IDC_STAB, m_Tab );
}

BEGIN_MESSAGE_MAP ( CSettingDialog, CDialog )
    ON_BN_CLICKED ( IDOK, OnBnClickedOk )
    ON_NOTIFY ( TCN_SELCHANGE, IDC_STAB, OnTcnSelchangeStab )
END_MESSAGE_MAP()


void CSettingDialog::OnBnClickedOk()
{
    m_Page1.UpdateData ( TRUE );
    m_Page2.UpdateData ( TRUE );
    m_Page3.UpdateData ( TRUE );
    m_Page4.UpdateData ( TRUE );
    m_Page1.Save();
    m_Page2.Save();
    m_Page3.Save();
    m_Page4.Save();
    m_ChangedASPI = m_Page3.m_ChangedASPI;
    m_ChangedSkin = m_Page2.m_ChangedSkin;
    OnOK();
}

BOOL CSettingDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_ChangedSkin = false;
    m_ChangedASPI = false;
    m_Tab.InsertItem ( 0, STR ( 13 ) );
    m_Tab.InsertItem ( 1, STR ( 14 ) );
    m_Tab.InsertItem ( 2, STR ( 15 ) );
    m_Tab.InsertItem ( 3, STR ( 16 ) );
    m_Page1.Create ( IDD_SETTING_1, this );
    m_Page2.Create ( IDD_SETTING_2, this );
    m_Page3.Create ( IDD_SETTING_3, this );
    m_Page4.Create ( IDD_SETTING_4, this );
    theTheme.EnableThemeDialogTexture ( m_Page1.m_hWnd, ETDT_ENABLETAB );
    theTheme.EnableThemeDialogTexture ( m_Page2.m_hWnd, ETDT_ENABLETAB );
    theTheme.EnableThemeDialogTexture ( m_Page3.m_hWnd, ETDT_ENABLETAB );
    theTheme.EnableThemeDialogTexture ( m_Page4.m_hWnd, ETDT_ENABLETAB );
    m_Tab.SetCurSel ( 0 );
    ChangeTab();
    UpdateData ( FALSE );
    m_Tab.SetWindowPos ( &wndBottom, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
    SetLanguage();
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CSettingDialog::SetLanguage ( void )
{
    SetWindowText ( STR ( 0 ) );
    SetDlgItemText ( IDOK, theSetting.m_Lang.m_Str[1] );
    SetDlgItemText ( IDCANCEL, theSetting.m_Lang.m_Str[2] );
}

void CSettingDialog::OnTcnSelchangeStab ( NMHDR *pNMHDR, LRESULT *pResult )
{
    *pResult = 0;
    ChangeTab();
}

void CSettingDialog::ChangeTab ( void )
{
    m_Page1.UpdateData ( TRUE );
    m_Page2.UpdateData ( TRUE );
    m_Page3.UpdateData ( TRUE );
    m_Page4.UpdateData ( TRUE );
    m_Page1.ShowWindow ( SW_HIDE );
    m_Page2.ShowWindow ( SW_HIDE );
    m_Page3.ShowWindow ( SW_HIDE );
    m_Page4.ShowWindow ( SW_HIDE );

    if ( m_Tab.GetCurSel() == 0 )
        {
            m_Page1.UpdateData ( FALSE );
            m_Page1.ShowWindow ( SW_SHOW );
        }

    else
        if ( m_Tab.GetCurSel() == 1 )
            {
                m_Page2.UpdateData ( FALSE );
                m_Page2.ShowWindow ( SW_SHOW );
            }

        else
            if ( m_Tab.GetCurSel() == 2 )
                {
                    m_Page3.UpdateData ( FALSE );
                    m_Page3.ShowWindow ( SW_SHOW );
                }

            else
                if ( m_Tab.GetCurSel() == 3 )
                    {
                        m_Page4.UpdateData ( FALSE );
                        m_Page4.ShowWindow ( SW_SHOW );
                    }
}
