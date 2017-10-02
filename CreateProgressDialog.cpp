#include "stdafx.h"
#include "CDM.h"
#include "CreateProgressDialog.h"
#include "Setting.h"
#include "Mmsystem.h"
#include "MessageDialog.h"


IMPLEMENT_DYNAMIC ( CCreateProgressDialog, CDialog )
CCreateProgressDialog::CCreateProgressDialog ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CCreateProgressDialog::IDD, pParent )
    , m_Message ( _T ( "" ) )
    , m_Percent ( _T ( "" ) )
{
}

CCreateProgressDialog::~CCreateProgressDialog()
{
}

void CCreateProgressDialog::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
    DDX_Text ( pDX, IDC_MESSAGE, m_Message );
    DDX_Control ( pDX, IDCANCEL, m_CancelButton );
    DDX_Control ( pDX, IDC_PROGRESS, m_Progress );
    DDX_Text ( pDX, PERCENT, m_Percent );
}


BEGIN_MESSAGE_MAP ( CCreateProgressDialog, CDialog )
    ON_BN_CLICKED ( IDOK, OnBnClickedOk )
    ON_BN_CLICKED ( IDCANCEL, OnBnClickedCancel )
    ON_COMMAND ( ID_WINDOW_CLOSE, OnWindowClose )
    ON_BN_CLICKED ( IDC_LOG, OnBnClickedLog )
    ON_COMMAND ( ID_UPDATE_DIALOG, OnUpdateDialog )
END_MESSAGE_MAP()


void CCreateProgressDialog::OnBnClickedOk()
{
    //  OnOK();
}

void CCreateProgressDialog::CreateIso ( LPCSTR ImgFileName, LPCSTR VolumeLabel, CListCtrl * List, CDirStructure * Dir, CLogWindow *LogWnd )
{
    m_Thread.m_FileName = ImgFileName;
    m_Thread.m_TrackList = List;
    m_Thread.m_Dir = Dir;
    m_Thread.m_ParentWnd = this;
    m_Thread.m_VolumeLabel = VolumeLabel;
    m_Thread.m_LogWnd = LogWnd;
    DoModal();
}

BOOL CCreateProgressDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_Thread.StartThread();
    m_Progress.SetRange ( 0, 100 );
    SetWindowText ( theSetting.m_Lang.m_Str[LP_CREATEP + 0] );
    this->SetDlgItemText ( IDC_LOG, theSetting.m_Lang.m_Str[LP_CREATEP + 1] );
    this->SetDlgItemText ( IDCANCEL, theSetting.m_Lang.m_Str[LP_CREATEP + 2] );
    return TRUE;  // return TRUE unless you set the focus to a control
}

void CCreateProgressDialog::OnBnClickedCancel()
{
    //  m_Thread.StopThread();
    //  OnCancel();
    m_Thread.m_StopFlag = true;
}

void CCreateProgressDialog::OnWindowClose()
{
    CMessageDialog dlg;

    if ( !m_Thread.m_StopFlag )
        {
            if ( m_Thread.m_Success )
                {
                    if ( theSetting.m_WavOnSuccess != "" )
                        {
                            PlaySound ( theSetting.m_WavOnSuccess, NULL, SND_ASYNC );
                        }

                    dlg.MessageBox ( CONF_MSG, MSG ( 60 ) );
                }

            else
                {
                    if ( theSetting.m_WavOnFail != "" )
                        {
                            PlaySound ( theSetting.m_WavOnFail, NULL, SND_ASYNC );
                        }

                    dlg.MessageBox ( CONF_MSG, MSG ( 61 ) );
                }
        }

    m_Thread.StopThread();
    OnCancel();
}

void CCreateProgressDialog::OnBnClickedLog()
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

void CCreateProgressDialog::OnUpdateDialog()
{
    UpdateData ( FALSE );
}
