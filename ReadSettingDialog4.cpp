#include "stdafx.h"
#include "cdm.h"
#include "ReadSettingDialog4.h"
#include "setting.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_READS + i])

IMPLEMENT_DYNAMIC ( CReadSettingDialog4, CDialog )
CReadSettingDialog4::CReadSettingDialog4 ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CReadSettingDialog4::IDD, pParent )
{
}

CReadSettingDialog4::~CReadSettingDialog4()
{
}

void CReadSettingDialog4::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
    DDX_Control ( pDX, IDC_AUDIOSPEED, m_Speed );
}


BEGIN_MESSAGE_MAP ( CReadSettingDialog4, CDialog )
END_MESSAGE_MAP()


void CReadSettingDialog4::Load ( void )
{
    UpdateData ( TRUE );

    if ( theSetting.m_Speed_Data <= 1 )
        {
            m_Speed.SetCurSel ( 4 );
        }

    else
        if ( theSetting.m_Speed_Data <= 4 )
            {
                m_Speed.SetCurSel ( 3 );
            }

        else
            if ( theSetting.m_Speed_Data <= 8 )
                {
                    m_Speed.SetCurSel ( 2 );
                }

            else
                if ( theSetting.m_Speed_Data <= 32 )
                    {
                        m_Speed.SetCurSel ( 1 );
                    }

                else
                    {
                        m_Speed.SetCurSel ( 0 );
                    }

    UpdateData ( FALSE );
}

void CReadSettingDialog4::Save ( void )
{
    UpdateData ( TRUE );
    int SpeedList[5] = {0xff, 32, 8, 4, 1};
    theSetting.m_Speed_Data = SpeedList[m_Speed.GetCurSel()];
    UpdateData ( FALSE );
}

BOOL CReadSettingDialog4::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_Speed.AddString ( MSG ( 20 ) );
    m_Speed.AddString ( "x32" );
    m_Speed.AddString ( "x8" );
    m_Speed.AddString ( "x4" );
    m_Speed.AddString ( "x1" );
    UpdateData ( FALSE );
    SetLanguage();
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CReadSettingDialog4::SetLanguage ( void )
{
    DWORD CtrlStr[][2] =
    {
        {IDC_STATIC1            , 18},
        {IDC_STATIC3            , 16},
    };
    int i;

    for ( i = 0; i < 2; i++ )
        {
            this->SetDlgItemText ( CtrlStr[i][0], STR ( CtrlStr[i][1] ) );
        }
}
