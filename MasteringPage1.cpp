#include "stdafx.h"
#include "CDM.h"
#include "MasteringPage1.h"
#include "MasteringFileDialog.h"
#include "Setting.h"


#define STR(i)  (theSetting.m_Lang.m_Str[LP_MASTERING + i])

IMPLEMENT_DYNAMIC(CMasteringPage1, CDialog)

CMasteringPage1::CMasteringPage1(CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
{
}

CMasteringPage1::~CMasteringPage1()
{
}

void CMasteringPage1::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TRACKLIST, m_TrackList);
}


BEGIN_MESSAGE_MAP(CMasteringPage1, CDialog)
    ON_NOTIFY(LVN_KEYDOWN, IDC_TRACKLIST, OnLvnKeydownTracklist)
    ON_NOTIFY(NM_RCLICK, IDC_TRACKLIST, OnNMRclickTracklist)
    //  ON_WM_PAINT()
    //  ON_NOTIFY(NM_CUSTOMDRAW, IDC_TRACKLIST, OnNMCustomdrawTracklist)
    ON_WM_DROPFILES()
    ON_BN_CLICKED(IDC_UP_ORDER, OnBnClickedUpOrder)
    ON_BN_CLICKED(IDC_DOWN_ORDER, OnBnClickedDownOrder)
END_MESSAGE_MAP()


BOOL CMasteringPage1::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_TrackList.InsertColumn(0, STR(9), LVCFMT_LEFT, 40, 0);
    m_TrackList.InsertColumn(1, STR(10), LVCFMT_CENTER, 90, 1);
    m_TrackList.InsertColumn(2, STR(11), LVCFMT_LEFT, 350, 2);
    m_TrackList.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    m_TrackMenu.CreatePopupMenu();
    m_TrackMenu.AppendMenu(MF_STRING, ID_EDIT_ADDDATA, theSetting.m_Lang.m_Str[LP_MASTERINGPOPUP + 4]);
    m_TrackMenu.AppendMenu(MF_STRING, ID_TRACK_ISO, theSetting.m_Lang.m_Str[LP_MASTERINGMENU + 15]);
    m_TrackMenu.AppendMenu(MF_STRING, ID_EDIT_ADDAUDIO, theSetting.m_Lang.m_Str[LP_MASTERINGPOPUP + 5]);
    m_TrackMenu.AppendMenu(MF_SEPARATOR, MF_SEPARATOR, "");
    m_TrackMenu.AppendMenu(MF_STRING, ID_EDIT_DELETETRACK, theSetting.m_Lang.m_Str[LP_MASTERINGPOPUP + 6]);
    SetLanguage();
    UpdateData(FALSE);
    return TRUE; // return TRUE unless you set the focus to a control
}

void CMasteringPage1::OnLvnKeydownTracklist(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
    *pResult = 0;

    if (pLVKeyDow->wVKey == VK_DELETE)
    {
        static_cast<CMasteringFileDialog*>(m_MainDialog)->OnEditDeletetrack();
    }
}

void CMasteringPage1::OnNMRclickTracklist(NMHDR* pNMHDR, LRESULT* pResult)
{
    POINT pos;
    GetCursorPos(&pos);
    m_TrackMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pos.x, pos.y, m_MainDialog, nullptr);
    *pResult = 0;
}

void CMasteringPage1::InsertIsoTrack(LPCSTR FileName)
{
    int id;
    int IsoType;
    IsoType = CheckIsoType(FileName);

    if (IsoType == -1)
    {
        return;
    }

    if (m_TrackList.GetItemCount() == 0)
    {
        m_TrackList.InsertItem(0, "1");
    }

    else if (m_TrackList.GetItemData(0) != 0)
    {
        m_TrackList.InsertItem(0, "1");
    }

    if (IsoType == 0)
    {
        m_TrackList.SetItemText(0, 1, "MODE1/2048");
    }

    else if (IsoType == 1)
    {
        m_TrackList.SetItemText(0, 1, "MODE1/2352");
    }

    else
    {
        m_TrackList.SetItemText(0, 1, "MODE2/2352");
    }

    m_TrackList.SetItemText(0, 2, FileName);
    m_TrackList.SetItemData(0, 0);

    for (id = 1; id < m_TrackList.GetItemCount(); id++)
    {
        CString cs;
        cs.Format("%d", id + 1);
        m_TrackList.SetItemText(id, 0, cs);
    }
}

void CMasteringPage1::InsertMode1MasteringTrack(void)
{
    int flag;
    flag = 0;

    if (m_TrackList.GetItemCount() == 0)
    {
        flag = 1;
    }

    else
    {
        DWORD Data;
        Data = m_TrackList.GetItemData(0);

        if (Data != 0)
        {
            flag = 1;
        }
    }

    if (flag == 1)
    {
        int id;
        id = m_TrackList.InsertItem(0, "1");

        for (id = 1; id < m_TrackList.GetItemCount(); id++)
        {
            CString cs;
            cs.Format("%d", id + 1);
            m_TrackList.SetItemText(id, 0, cs);
        }
    }

    m_TrackList.SetItemText(0, 1, "Mastering");
    m_TrackList.SetItemText(0, 2, MSG(88));
    m_TrackList.SetItemData(0, 0);
}

void CMasteringPage1::DeleteSelectedTracks(void)
{
    POSITION pos;
    int id;
    UpdateData(TRUE);

    while (true)
    {
        pos = m_TrackList.GetFirstSelectedItemPosition();

        if (pos == nullptr)
        {
            break;
        }

        id = m_TrackList.GetNextSelectedItem(pos);
        m_TrackList.DeleteItem(id);
    }

    for (id = 0; id < m_TrackList.GetItemCount(); id++)
    {
        CString cs;
        cs.Format("%d", id + 1);
        m_TrackList.SetItemText(id, 0, cs);
    }

    UpdateData(FALSE);
    static_cast<CMasteringFileDialog*>(m_MainDialog)->CalcSize();
}

void CMasteringPage1::InsertWaveAudioTrack(void)
{
    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_ALLOWMULTISELECT,
                    MSG(89));
    char buf[2048];
    char* p;
    int id;
    UpdateData(TRUE);
    buf[0] = '\0';
    p = dlg.m_ofn.lpstrFile;
    dlg.m_ofn.lpstrFile = buf;
    dlg.m_ofn.nMaxFile = 2048;

    if (dlg.DoModal() == IDOK)
    {
        POSITION pos;
        pos = dlg.GetStartPosition();

        while (pos)
        {
            LPCSTR PathName;
            CString cs;
            cs.Format("%d", m_TrackList.GetItemCount() + 1);
            PathName = static_cast<LPCTSTR>(dlg.GetNextPathName(pos));
            id = m_TrackList.InsertItem(m_TrackList.GetItemCount(), cs);
            m_TrackList.SetItemText(id, 1, "Audio");
            m_TrackList.SetItemText(id, 2, PathName);
            m_TrackList.SetItemData(id, 1);
        }
    }

    dlg.m_ofn.lpstrFile = p;
    UpdateData(FALSE);
    static_cast<CMasteringFileDialog*>(m_MainDialog)->CalcSize();
}

void CMasteringPage1::OnDropFiles(HDROP hDropInfo)
{
    char buf[2048];
    CString cs, lw;
    int i, max;
    UpdateData(TRUE);
    max = DragQueryFile(hDropInfo, 0xffffffff, nullptr, 0);

    for (i = 0; i < max; i++)
    {
        DragQueryFile(hDropInfo, i, buf, 2048);
        cs = buf;
        lstrcpy(buf, cs.MakeLower());

        if (lstrcmpi(buf + lstrlen(buf) - 4, ".wav") == 0)
        {
            int id;
            CString cs;
            cs.Format("%d", m_TrackList.GetItemCount() + 1);
            id = m_TrackList.InsertItem(m_TrackList.GetItemCount(), cs);
            m_TrackList.SetItemText(id, 1, "Audio");
            m_TrackList.SetItemText(id, 2, buf);
            m_TrackList.SetItemData(id, 1);
        }

        else if (lstrcmpi(buf + lstrlen(buf) - 4, ".iso") == 0)
        {
            InsertIsoTrack(buf);
        }

        else if (lstrcmpi(buf + lstrlen(buf) - 4, ".bin") == 0)
        {
            InsertIsoTrack(buf);
        }

        else if (lstrcmpi(buf + lstrlen(buf) - 4, ".img") == 0)
        {
            InsertIsoTrack(buf);
        }
    }

    CDialog::OnDropFiles(hDropInfo);
    UpdateData(FALSE);
    static_cast<CMasteringFileDialog*>(m_MainDialog)->CalcSize();
}

void CMasteringPage1::OnBnClickedUpOrder()
{
    int i, start;

    if (m_TrackList.GetItemCount() == 0)
    {
        return;
    }

    start = 0;

    if (m_TrackList.GetItemData(0) == 0)
    {
        m_TrackList.SetItemState(0, 0, LVIS_SELECTED);
        start = 1;
    }

    //   search first point
    for (i = start; i < m_TrackList.GetItemCount(); i++)
    {
        if (m_TrackList.GetItemState(i, LVIS_SELECTED))
        {
            start++;
        }

        else
        {
            break;
        }
    }

    //   swap tracks
    for (i = start; i < m_TrackList.GetItemCount(); i++)
    {
        if (m_TrackList.GetItemState(i, LVIS_SELECTED))
        {
            SwapItem(i, i - 1);
        }
    }
}

void CMasteringPage1::OnBnClickedDownOrder()
{
    int i, end, start;

    if (m_TrackList.GetItemCount() == 0)
    {
        return;
    }

    end = 0;

    if (m_TrackList.GetItemData(0) == 0)
    {
        m_TrackList.SetItemState(0, 0, LVIS_SELECTED);
        end = 1;
    }

    start = m_TrackList.GetItemCount() - 1;

    //   search first point
    for (i = start; i >= end; i--)
    {
        if (m_TrackList.GetItemState(i, LVIS_SELECTED))
        {
            start--;
        }

        else
        {
            break;
        }
    }

    for (i = start; i >= end; i--)
    {
        if (m_TrackList.GetItemState(i, LVIS_SELECTED))
        {
            SwapItem(i, i + 1);
        }
    }
}

void CMasteringPage1::SwapItem(int id1, int id2)
{
    CString TrackType1, TrackType2;
    CString File1, File2;
    DWORD Data1, Data2;
    UINT Flag1, Flag2;

    if (id1 > m_TrackList.GetItemCount() - 1) { return; }

    if (id2 > m_TrackList.GetItemCount() - 1) { return; }

    TrackType1 = m_TrackList.GetItemText(id1, 1);
    TrackType2 = m_TrackList.GetItemText(id2, 1);
    File1 = m_TrackList.GetItemText(id1, 2);
    File2 = m_TrackList.GetItemText(id2, 2);
    Data1 = m_TrackList.GetItemData(id1);
    Data2 = m_TrackList.GetItemData(id2);
    Flag1 = m_TrackList.GetItemState(id1, LVIS_SELECTED);
    Flag2 = m_TrackList.GetItemState(id2, LVIS_SELECTED);
    m_TrackList.SetItemText(id1, 1, TrackType2);
    m_TrackList.SetItemText(id2, 1, TrackType1);
    m_TrackList.SetItemText(id1, 2, File2);
    m_TrackList.SetItemText(id2, 2, File1);
    m_TrackList.SetItemData(id1, Data2);
    m_TrackList.SetItemData(id2, Data1);
    m_TrackList.SetItemState(id1, Flag2, LVIS_SELECTED);
    m_TrackList.SetItemState(id2, Flag1, LVIS_SELECTED);
}

void CMasteringPage1::SetLanguage(void)
{
    int i;
    DWORD CtrlString[][2] =
    {
        {IDC_UP_ORDER, 22},
        {IDC_DOWN_ORDER, 23},
    };

    for (i = 0; i < 2; i++)
    {
        this->SetDlgItemText(CtrlString[i][0], STR(CtrlString[i][1]));
    }
}

//  -1 : invalid file type
//   0 : mode1/2048
//   1 : mode1/2352
//   2 : mode2/2352
int CMasteringPage1::CheckIsoType(LPCSTR FileName)
{
    HANDLE hFile;
    BYTE SyncHeader[12] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
    DWORD read;
    BYTE Buffer[16];
    int IsoType = -1;
    hFile = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        ReadFile(hFile, Buffer, 16, &read, nullptr);

        if (memcmp(Buffer, SyncHeader, 12) == 0)
        {
            //   raw mode
            if (Buffer[0x0c] == 0x00 && Buffer[0x0d] == 0x02 && Buffer[0x0e] == 0x00 && Buffer[0x0f] == 0x01)
            {
                //   mode1/2352
                IsoType = 1;
            }

            else if (Buffer[0x0c] == 0x00 && Buffer[0x0d] == 0x02 && Buffer[0x0e] == 0x00 && Buffer[0x0f] == 0x02)
            {
                //   mode2/2352
                IsoType = 2;
            }
        }

        else
        {
            SetFilePointer(hFile, 2048 * 16, nullptr, FILE_BEGIN);
            ReadFile(hFile, Buffer, 16, &read, nullptr);

            if (Buffer[0] == 1 && Buffer[1] == 'C' && Buffer[2] == 'D' && Buffer[3] == '0' && Buffer[4] == '0' && Buffer
                [5] == '1' && Buffer[6] == 1)
            {
                //   mode1/2048
                IsoType = 0;
            }
        }

        CloseHandle(hFile);
    }

    return IsoType;
}
