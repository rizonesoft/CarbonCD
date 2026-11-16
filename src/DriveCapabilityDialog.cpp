#include "stdafx.h"
#include "CDM.h"
#include "DriveCapabilityDialog.h"
#include "Setting.h"


IMPLEMENT_DYNAMIC(CDriveCapabilityDialog, CDialog)

CDriveCapabilityDialog::CDriveCapabilityDialog(CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
      , m_Vendor(_T(""))
      , m_Product(_T(""))
      , m_Revision(_T(""))
      , m_Sao(_T(""))
      , m_Raw96(_T(""))
      , m_Raw16(_T(""))
      , m_Raw96P(_T(""))
      , m_SaoTest(_T(""))
      , m_Raw16Test(_T(""))
      , m_Raw96Test(_T(""))
      , m_Raw96PTest(_T(""))
      , m_BufferSize(_T(""))
      , m_Writer(_T(""))
      , m_DefaultCommand(_T(""))
      , m_Address(_T(""))
{
}

CDriveCapabilityDialog::~CDriveCapabilityDialog()
{
}

void CDriveCapabilityDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_VENDOR, m_Vendor);
    DDX_Text(pDX, IDC_DRIVENAME, m_Product);
    DDX_Text(pDX, IDC_REVISION, m_Revision);
    DDX_Text(pDX, IDC_SAO, m_Sao);
    DDX_Text(pDX, IDC_RAW96, m_Raw96);
    DDX_Text(pDX, IDC_RAW16, m_Raw16);
    DDX_Text(pDX, IDC_RAW96P, m_Raw96P);
    DDX_Text(pDX, IDC_SAO_TEST, m_SaoTest);
    DDX_Text(pDX, IDC_RAW16_TEST, m_Raw16Test);
    DDX_Text(pDX, IDC_RAW96_TEST, m_Raw96Test);
    DDX_Text(pDX, IDC_RAW96P_TEST, m_Raw96PTest);
    DDX_Text(pDX, IDC_BUFFERSIZE, m_BufferSize);
    DDX_Text(pDX, IDC_WRITER, m_Writer);
    DDX_Text(pDX, IDC_DEFAULT_COMMAND, m_DefaultCommand);
    DDX_Text(pDX, IDC_ADDRESS, m_Address);
}


BEGIN_MESSAGE_MAP(CDriveCapabilityDialog, CDialog)
END_MESSAGE_MAP()


BOOL CDriveCapabilityDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    int i;
    SenseCapability();
    SetWindowText(theSetting.m_Lang.m_Str[LP_DC + 0]);

    for (i = 0; i < 12; i++)
    {
        SetDlgItemText(1114 + i, theSetting.m_Lang.m_Str[LP_DC + i + 1]);
    }

    SetDlgItemText(IDOK, theSetting.m_Lang.m_Str[1]);
    return TRUE; // return TRUE unless you set the focus to a control
}

void CDriveCapabilityDialog::SenseCapability(void)
{
    UpdateData(TRUE);
    CString cs;
    //   set drive address
    //   set drive info
    m_CD->GetAspiCtrl()->GetDeviceString(m_Vendor, m_Product, m_Revision, cs);
    m_Vendor.TrimRight();
    m_Product.TrimRight();
    m_Revision.TrimRight();
    m_Address.Format("[ %s ]", cs);
    //   set commands
    {
        m_Raw96 = MSG(56);
        m_Raw16 = MSG(56);
        m_Raw96P = MSG(56);
        m_Sao = MSG(56);
        m_Raw96Test = MSG(56);
        m_Raw16Test = MSG(56);
        m_Raw96PTest = MSG(56);
        m_SaoTest = MSG(56);

        if (m_CD->SetWritingParams(WRITEMODE_RAW_96, false, false, 10))
        {
            m_Raw96 = MSG(55);
        }

        if (m_CD->SetWritingParams(WRITEMODE_RAW_16, false, false, 10))
        {
            m_Raw16 = MSG(55);
        }

        if (m_CD->SetWritingParams(WRITEMODE_RAW_P96, false, false, 10))
        {
            m_Raw96P = MSG(55);
        }

        if (m_CD->SetWritingParams(WRITEMODE_2048, false, false, 10))
        {
            m_Sao = MSG(55);
        }

        if (m_CD->SetWritingParams(WRITEMODE_RAW_96, false, true, 10))
        {
            m_Raw96Test = MSG(55);
        }

        if (m_CD->SetWritingParams(WRITEMODE_RAW_16, false, true, 10))
        {
            m_Raw16Test = MSG(55);
        }

        if (m_CD->SetWritingParams(WRITEMODE_RAW_P96, false, true, 10))
        {
            m_Raw96PTest = MSG(55);
        }

        if (m_CD->SetWritingParams(WRITEMODE_2048, false, true, 10))
        {
            m_SaoTest = MSG(55);
        }

        m_BufferSize.Format("%dKByte", m_CD->GetBufferSize());

        if (m_CD->IsCDR())
        {
            m_Writer = MSG(67);
        }

        else
        {
            m_Writer = MSG(68);
        }

        m_DefaultCommand = MSG(69);

        if (m_Raw96 == MSG(55))
        {
            m_DefaultCommand = "RAW+96";
        }

        else if (m_Raw16 == MSG(55))
        {
            m_DefaultCommand = "RAW+16";
        }

        else if (m_Sao == MSG(55))
        {
            m_DefaultCommand = "SAO";
        }
    }
    UpdateData(FALSE);
}
