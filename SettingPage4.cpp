#include "stdafx.h"
#include "cdm.h"
#include "SettingPage4.h"
#include "Setting.h"


IMPLEMENT_DYNAMIC ( CSettingPage4, CDialog )
CSettingPage4::CSettingPage4 ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CSettingPage4::IDD, pParent )
{
}

CSettingPage4::~CSettingPage4()
{
}

void CSettingPage4::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
    DDX_Control ( pDX, IDC_SUBQSPEED, m_SubQSpeed );
    DDX_Control ( pDX, IDC_SUBQMETHOD, m_SubQMethod );
}


BEGIN_MESSAGE_MAP ( CSettingPage4, CDialog )
END_MESSAGE_MAP()


void CSettingPage4::Save ( void )
{
    int SpeedList[5] = {0xff, 32, 8, 4, 1};
    theSetting.m_Speed_SubQ = SpeedList[m_SubQSpeed.GetCurSel()];
    theSetting.m_SubQMethod = m_SubQMethod.GetCurSel();
}

BOOL CSettingPage4::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_SubQSpeed.AddString ( MSG ( 20 ) );
    m_SubQSpeed.AddString ( "x32" );
    m_SubQSpeed.AddString ( "x8" );
    m_SubQSpeed.AddString ( "x4" );
    m_SubQSpeed.AddString ( "x1" );
    m_SubQMethod.AddString ( MSG ( 122 ) );
    m_SubQMethod.AddString ( MSG ( 123 ) );
    m_SubQMethod.AddString ( MSG ( 124 ) );

    if ( theSetting.m_Speed_SubQ <= 1 )
        {
            m_SubQSpeed.SetCurSel ( 4 );
        }

    else
        if ( theSetting.m_Speed_SubQ <= 4 )
            {
                m_SubQSpeed.SetCurSel ( 3 );
            }

        else
            if ( theSetting.m_Speed_SubQ <= 8 )
                {
                    m_SubQSpeed.SetCurSel ( 2 );
                }

            else
                if ( theSetting.m_Speed_SubQ <= 32 )
                    {
                        m_SubQSpeed.SetCurSel ( 1 );
                    }

                else
                    {
                        m_SubQSpeed.SetCurSel ( 0 );
                    }

    m_SubQMethod.SetCurSel ( theSetting.m_SubQMethod );
    UpdateData ( FALSE );
    SetLanguage();
    return TRUE;  // return TRUE unless you set the focus to a control
}

#define STR(i) (theSetting.m_Lang.m_Str[LP_SET + i])
void CSettingPage4::SetLanguage ( void )
{
    DWORD CtrlStr[][2] =
    {
        {IDC_STATIC1        , 3},
    };
    int i;

    for ( i = 0; i < 1; i++ )
        {
            SetDlgItemText ( CtrlStr[i][0], STR ( CtrlStr[i][1] ) );
        }
}
