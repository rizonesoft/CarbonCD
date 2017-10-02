#include "stdafx.h"
#include "CDM.h"
#include "MsfDialog.h"
#include "CDType.h"

#include "Setting.h"


IMPLEMENT_DYNAMIC ( CMsfDialog, CDialog )
CMsfDialog::CMsfDialog ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CMsfDialog::IDD, pParent )
    , m_MSF_M ( _T ( "" ) )
    , m_MSF_S ( _T ( "" ) )
    , m_MSF_F ( _T ( "" ) )
    , m_MSF150_M ( _T ( "" ) )
    , m_MSF150_S ( _T ( "" ) )
    , m_MSF150_F ( _T ( "" ) )
    , m_LBA ( _T ( "" ) )
    , m_LBAHex ( _T ( "" ) )
{
}

CMsfDialog::~CMsfDialog()
{
}

void CMsfDialog::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
    DDX_Text ( pDX, IDC_MSF_M, m_MSF_M );
    DDX_Text ( pDX, IDC_MSF_S, m_MSF_S );
    DDX_Text ( pDX, IDC_MSF_F, m_MSF_F );
    DDX_Text ( pDX, IDC_MSF150_M, m_MSF150_M );
    DDX_Text ( pDX, IDC_MSF150_S, m_MSF150_S );
    DDX_Text ( pDX, IDC_MSF150_F, m_MSF150_F );
    DDX_Text ( pDX, IDC_LBA, m_LBA );
    DDX_Text ( pDX, IDC_LBAHEX, m_LBAHex );
}


BEGIN_MESSAGE_MAP ( CMsfDialog, CDialog )
    ON_BN_CLICKED ( IDOK, OnBnClickedOk )
    ON_BN_CLICKED ( IDC_CALC_MSF, OnBnClickedCalcMsf )
    ON_BN_CLICKED ( IDC_CALC_MSF150, OnBnClickedCalcMsf150 )
    ON_BN_CLICKED ( IDC_CALC_LBA, OnBnClickedCalcLba )
    ON_BN_CLICKED ( IDC_CALC_LBAHEX, OnBnClickedCalcLbahex )
    ON_BN_CLICKED ( IDC_ZERO, OnBnClickedZero )
END_MESSAGE_MAP()

BOOL CMsfDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    CalcMSF ( 0 );
    SetWindowText ( theSetting.m_Lang.m_Str[LP_MSF + 0] );
    SetDlgItemText ( IDC_CALC_MSF, theSetting.m_Lang.m_Str[LP_MSF + 1] );
    SetDlgItemText ( IDC_CALC_MSF150, theSetting.m_Lang.m_Str[LP_MSF + 1] );
    SetDlgItemText ( IDC_CALC_LBA, theSetting.m_Lang.m_Str[LP_MSF + 1] );
    SetDlgItemText ( IDC_CALC_LBAHEX, theSetting.m_Lang.m_Str[LP_MSF + 1] );
    SetDlgItemText ( IDCANCEL, theSetting.m_Lang.m_Str[LP_MSF + 2] );
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CMsfDialog::CalcMSF ( DWORD LBA )
{
    MSFAddress msf;
    msf = LBA;
    m_LBA.Format ( "%ld", LBA );
    m_LBAHex.Format ( "%X", LBA );
    m_MSF_M.Format ( "%d", msf.Minute );
    m_MSF_S.Format ( "%d", msf.Second );
    m_MSF_F.Format ( "%d", msf.Frame );
    msf = msf.GetByLBA() + 150;
    m_MSF150_M.Format ( "%d", msf.Minute );
    m_MSF150_S.Format ( "%d", msf.Second );
    m_MSF150_F.Format ( "%d", msf.Frame );
    UpdateData ( FALSE );
}

void CMsfDialog::OnBnClickedOk()
{
    //  OnOK();
}

void CMsfDialog::OnBnClickedCalcMsf()
{
    int LBA = 0;
    LPCSTR p;
    MSFAddress msf;
    UpdateData ( TRUE );
    LBA = 0;
    p = m_MSF_M;

    while ( *p != '\0' )
        {
            if ( *p >= '0' && *p <= '9' )
                {
                    LBA = LBA * 10;
                    LBA += *p - '0';
                }

            else
                {
                    break;
                }

            p++;
        }

    msf.Minute = LBA;
    LBA = 0;
    p = m_MSF_S;

    while ( *p != '\0' )
        {
            if ( *p >= '0' && *p <= '9' )
                {
                    LBA = LBA * 10;
                    LBA += *p - '0';
                }

            else
                {
                    break;
                }

            p++;
        }

    msf.Second = LBA;
    LBA = 0;
    p = m_MSF_F;

    while ( *p != '\0' )
        {
            if ( *p >= '0' && *p <= '9' )
                {
                    LBA = LBA * 10;
                    LBA += *p - '0';
                }

            else
                {
                    break;
                }

            p++;
        }

    msf.Frame = LBA;
    LBA = msf.GetByLBA();
    CalcMSF ( ( DWORD ) LBA );
}

void CMsfDialog::OnBnClickedCalcMsf150()
{
    int LBA = 0;
    LPCSTR p;
    MSFAddress msf;
    UpdateData ( TRUE );
    LBA = 0;
    p = m_MSF150_M;

    while ( *p != '\0' )
        {
            if ( *p >= '0' && *p <= '9' )
                {
                    LBA = LBA * 10;
                    LBA += *p - '0';
                }

            else
                {
                    break;
                }

            p++;
        }

    msf.Minute = LBA;
    LBA = 0;
    p = m_MSF150_S;

    while ( *p != '\0' )
        {
            if ( *p >= '0' && *p <= '9' )
                {
                    LBA = LBA * 10;
                    LBA += *p - '0';
                }

            else
                {
                    break;
                }

            p++;
        }

    msf.Second = LBA;
    LBA = 0;
    p = m_MSF150_F;

    while ( *p != '\0' )
        {
            if ( *p >= '0' && *p <= '9' )
                {
                    LBA = LBA * 10;
                    LBA += *p - '0';
                }

            else
                {
                    break;
                }

            p++;
        }

    msf.Frame = LBA;
    LBA = msf.GetByLBA();

    if ( LBA < 150 )
        {
            MessageBox ( MSG ( 95 ), CONF_MSG );
            return;
        }

    CalcMSF ( ( DWORD ) ( LBA - 150 ) );
}

void CMsfDialog::OnBnClickedCalcLba()
{
    int LBA = 0;
    LPCSTR p;
    UpdateData ( TRUE );
    p = m_LBA;

    while ( *p != '\0' )
        {
            if ( *p >= '0' && *p <= '9' )
                {
                    LBA = LBA * 10;
                    LBA += *p - '0';
                }

            else
                {
                    break;
                }

            p++;
        }

    CalcMSF ( ( DWORD ) LBA );
}

void CMsfDialog::OnBnClickedCalcLbahex()
{
    int LBA = 0;
    LPCSTR p;
    UpdateData ( TRUE );
    p = m_LBAHex;

    while ( *p != '\0' )
        {
            if ( *p >= '0' && *p <= '9' )
                {
                    LBA = LBA * 16;
                    LBA += *p - '0';
                }

            else
                if ( *p >= 'A' && *p <= 'F' )
                    {
                        LBA = LBA * 16;
                        LBA += *p - 'A' + 10;
                    }

                else
                    if ( *p >= 'a' && *p <= 'f' )
                        {
                            LBA = LBA * 16;
                            LBA += *p - 'a' + 10;
                        }

                    else
                        {
                            break;
                        }

            p++;
        }

    CalcMSF ( ( DWORD ) LBA );
}

void CMsfDialog::OnBnClickedZero()
{
    CalcMSF ( 0 );
}
