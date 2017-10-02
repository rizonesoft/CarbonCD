#include "stdafx.h"
#include "CDM.h"
#include "EraseDialog.h"
#include "Setting.h"


static DWORD WINAPI EraseThread ( LPVOID Thread )
{
    if ( ( ( CEraseDialog* ) Thread )->Erase() )
        {
            ( ( CEraseDialog* ) Thread )->Complete();
        }

    else
        {
            ( ( CEraseDialog* ) Thread )->Incomplete();
        }

    ExitThread ( 1 );
    return 1;
}

IMPLEMENT_DYNAMIC ( CEraseDialog, CDialog )
CEraseDialog::CEraseDialog ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CEraseDialog::IDD, pParent )
    , m_Message ( _T ( "" ) )
{
    m_hThread = INVALID_HANDLE_VALUE;
}

CEraseDialog::~CEraseDialog()
{
    if ( m_hThread )
        {
            DWORD retcode;
            GetExitCodeThread ( m_hThread, &retcode );

            if ( retcode == STILL_ACTIVE )
                {
                    TerminateThread ( m_hThread, 1 );
                }

            CloseHandle ( m_hThread );
        }
}

void CEraseDialog::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
    DDX_Control ( pDX, IDOK, m_OKButton );
    DDX_Text ( pDX, IDC_MESSAGE, m_Message );
}


BEGIN_MESSAGE_MAP ( CEraseDialog, CDialog )
    ON_BN_CLICKED ( IDCANCEL, OnBnClickedCancel )
    ON_BN_CLICKED ( IDOK, OnBnClickedOk )
    ON_COMMAND ( ID_UPDATE_DIALOG, OnUpdateDialog )
END_MESSAGE_MAP()


void CEraseDialog::OnBnClickedCancel()
{
    OnCancel();
}

void CEraseDialog::OnBnClickedOk()
{
    OnOK();
}

BOOL CEraseDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetDlgItemText ( IDOK, theSetting.m_Lang.m_Str[1] );
    m_OKButton.ShowWindow ( SW_HIDE );
    SetWindowText ( theSetting.m_Lang.m_Str[LP_ERASE + 0] );

    if ( m_FastErase )
        {
            m_Message = theSetting.m_Lang.m_Str[LP_ERASE + 1];
        }

    else
        {
            m_Message = theSetting.m_Lang.m_Str[LP_ERASE + 2];
        }

    UpdateData ( FALSE );
    m_ThreadID = 0;
    m_hThread = CreateThread ( NULL, 0, EraseThread, this, 0, &m_ThreadID );

    if ( m_hThread == INVALID_HANDLE_VALUE )
        {
            m_OKButton.ShowWindow ( SW_SHOW );
            m_Message = MSG ( 70 );
            UpdateData ( FALSE );
            return TRUE;
        }

    SetThreadPriority ( m_hThread, THREAD_PRIORITY_HIGHEST );
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CEraseDialog::EraseFast ( CCDController *cd )
{
    m_FastErase = true;
    m_CD = cd;
    DoModal();
}

void CEraseDialog::EraseCompletely ( CCDController *cd )
{
    m_FastErase = false;
    m_CD = cd;
    DoModal();
}

bool CEraseDialog::Erase ( void )
{
    bool RetValue;
    m_CD->SetWritingParams ( WRITEMODE_RAW_96, false, false, 10 );
    m_CD->OPC();
    RetValue = m_CD->EraseMedia ( m_FastErase );
    m_CD->LoadTray ( false );
    return RetValue;
}

void CEraseDialog::Complete ( void )
{
    m_OKButton.ShowWindow ( SW_SHOW );
    m_Message = theSetting.m_Lang.m_Str[LP_ERASE + 3];
    ::PostMessage ( m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0 );
}

void CEraseDialog::Incomplete ( void )
{
    m_OKButton.ShowWindow ( SW_SHOW );
    m_Message = theSetting.m_Lang.m_Str[LP_ERASE + 4];
    ::PostMessage ( m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0 );
}

void CEraseDialog::OnUpdateDialog()
{
    UpdateData ( FALSE );
}
