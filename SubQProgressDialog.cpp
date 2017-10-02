#include "stdafx.h"
#include "CDM.h"
#include "SubQProgressDialog.h"
#include "Setting.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_SUBQ + i])


IMPLEMENT_DYNAMIC ( CSubQProgressDialog, CDialog )
CSubQProgressDialog::CSubQProgressDialog ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CSubQProgressDialog::IDD, pParent )
    , m_Message ( _T ( "" ) )
    , m_Sector ( _T ( "" ) )
{
}

CSubQProgressDialog::~CSubQProgressDialog()
{
}

void CSubQProgressDialog::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
    DDX_Text ( pDX, IDC_MESSAGE, m_Message );
    DDX_Control ( pDX, IDCANCEL, m_CancelButton );
    DDX_Text ( pDX, IDC_SECTOR, m_Sector );
}


BEGIN_MESSAGE_MAP ( CSubQProgressDialog, CDialog )
    ON_BN_CLICKED ( IDOK, OnBnClickedOk )
    ON_BN_CLICKED ( IDCANCEL, OnBnClickedCancel )
    ON_BN_CLICKED ( IDC_LOG, OnBnClickedLog )
    ON_COMMAND ( ID_WINDOW_CLOSE, OnWindowClose )
    ON_COMMAND ( ID_UPDATE_DIALOG, OnUpdateDialog )
END_MESSAGE_MAP()


void CSubQProgressDialog::OnBnClickedOk()
{
    //  OnOK();
}

bool CSubQProgressDialog::AnalyzeSubQ ( CCDController * cd, CLogWindow * logWnd )
{
    CString DriveName, Message;
    cd->GetDriveName ( DriveName );
    Message.Format ( MSG ( 127 ), DriveName );
    logWnd->AddMessage ( LOG_NORMAL, Message );
    m_Thread.m_CD = cd;
    m_Thread.m_LogWnd = logWnd;
    m_Thread.m_ParentWnd = this;
    DoModal();
    cd->SetSpeed ( 0xff, 0xff );
    return m_Thread.m_Success;
}

BOOL CSubQProgressDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetWindowText ( STR ( 0 ) );
    SetDlgItemText ( IDC_LOG, STR ( 1 ) );
    SetDlgItemText ( IDCANCEL, STR ( 2 ) );
    m_Thread.StartThread();
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CSubQProgressDialog::OnBnClickedCancel()
{
    m_Thread.m_StopFlag = true;
}

void CSubQProgressDialog::OnBnClickedLog()
{
    if ( m_Thread.m_LogWnd->GetStyle() & 0x10000000 )
        {
            m_Thread.m_LogWnd->ShowWindow ( SW_HIDE );
        }

    else
        {
            m_Thread.m_LogWnd->ShowWindow ( SW_SHOW );
        }
}

void CSubQProgressDialog::OnWindowClose()
{
    m_Thread.StopThread();
    OnCancel();
}

void CSubQProgressDialog::OnUpdateDialog()
{
    UpdateData ( FALSE );
}
