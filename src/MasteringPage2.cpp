#include "stdafx.h"
#include "CDM.h"
#include "MasteringPage1.h"
#include "MasteringPage2.h"
#include "Setting.h"
#include "MasteringFileDialog.h"

#define STR(i)  (theSetting.m_Lang.m_Str[LP_MASTERING + i])

IMPLEMENT_DYNAMIC(CMasteringPage2, CDialog)

CMasteringPage2::CMasteringPage2(CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
      , m_VolumeLabel(_T(""))
{
}

CMasteringPage2::~CMasteringPage2()
{
}

void CMasteringPage2::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST, m_List);
    DDX_Control(pDX, IDC_TREE, m_Tree);
    DDX_Text(pDX, IDC_VOLUME_LABEL, m_VolumeLabel);
}


BEGIN_MESSAGE_MAP(CMasteringPage2, CDialog)
    ON_NOTIFY(LVN_KEYDOWN, IDC_LIST, OnLvnKeydownList)
    ON_NOTIFY(NM_RCLICK, IDC_LIST, OnNMRclickList)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST, OnNMDblclkList)
    ON_NOTIFY(TVN_SELCHANGED, IDC_TREE, OnTvnSelchangedTree)
    ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST, OnLvnEndlabeleditList)
    ON_WM_DROPFILES()
END_MESSAGE_MAP()


void CMasteringPage2::SetLanguage(void)
{
    int i;
    DWORD CtrlString[][2] =
    {
        {IDC_STATIC_TXT1, 1},
    };

    for (i = 0; i < 1; i++)
    {
        this->SetDlgItemText(CtrlString[i][0], STR(CtrlString[i][1]));
    }
}

BOOL CMasteringPage2::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_Menu.CreatePopupMenu();
    m_Menu.AppendMenu(MF_STRING, ID_EDIT_ADDFOLDER, theSetting.m_Lang.m_Str[LP_MASTERINGPOPUP + 0]);
    m_Menu.AppendMenu(MF_STRING, ID_EDIT_INSERTFOLDER, theSetting.m_Lang.m_Str[LP_MASTERINGMENU + 14]);
    m_Menu.AppendMenu(MF_STRING, ID_EDIT_ADDFILE, theSetting.m_Lang.m_Str[LP_MASTERINGPOPUP + 1]);
    m_Menu.AppendMenu(MF_STRING, ID_EDIT_DELETEFOLDER, theSetting.m_Lang.m_Str[LP_MASTERINGPOPUP + 2]);
    m_Menu.AppendMenu(MF_SEPARATOR, MF_SEPARATOR, "");
    m_Menu.AppendMenu(MF_STRING, ID_EDIT_LABEL, theSetting.m_Lang.m_Str[LP_MASTERINGPOPUP + 3]);
    m_TreeImageNormal.Create(IDB_FILE_TREE, 12, 2, 0xffffff);
    m_Tree.SetImageList(&m_TreeImageNormal, TVSIL_NORMAL);
    HTREEITEM ht;
    ht = m_Tree.InsertItem("ISO_Image:\\", 0, 1, TVI_ROOT, TVI_LAST);
    m_Tree.SetItemData(ht, (DWORD)(m_Dir));
    m_Tree.Select(ht, TVGN_CARET);
    m_ListImage.Create(IDB_FILE_LIST, 12, 2, 0xffffff);
    m_List.SetImageList(&m_ListImage, LVSIL_SMALL);
    m_List.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    m_List.InsertColumn(0, STR(13), LVCFMT_LEFT, 250, 0);
    m_List.InsertColumn(1, STR(14), LVCFMT_LEFT, 50, 1);
    m_List.InsertColumn(2, STR(15), LVCFMT_RIGHT, 80, 2);
    m_List.InsertColumn(3, STR(16), LVCFMT_LEFT, 130, 3);
    m_List.InsertColumn(4, STR(17), LVCFMT_LEFT, 400, 4);
    m_VolumeLabel = STR(12);
    SetLanguage();
    return TRUE; // return TRUE unless you set the focus to a control
}

void CMasteringPage2::OnLvnKeydownList(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
    *pResult = 0;

    if (pLVKeyDow->wVKey == VK_DELETE)
    {
        static_cast<CMasteringFileDialog*>(m_MainDialog)->OnEditDeletefolder();
    }

    else if (pLVKeyDow->wVKey == VK_F2)
    {
        static_cast<CMasteringFileDialog*>(m_MainDialog)->OnEditLabel();
    }
}

void CMasteringPage2::OnNMRclickList(NMHDR* pNMHDR, LRESULT* pResult)
{
    POINT pos;
    GetCursorPos(&pos);
    m_Menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pos.x, pos.y, m_MainDialog, nullptr);
    *pResult = 0;
}

void CMasteringPage2::OnNMDblclkList(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    int id;
    POSITION pos;
    CDirStructure *NowNode, *MoveNode;
    HTREEITEM ht;
    CString MoveNodeName;
    ht = m_Tree.GetSelectedItem();
    NowNode = (CDirStructure*)m_Tree.GetItemData(ht);
    pos = m_List.GetFirstSelectedItemPosition();
    id = m_List.GetNextSelectedItem(pos);
    MoveNodeName = m_List.GetItemText(id, 0);
    MoveNode = NowNode->SearchName(MoveNodeName);

    if (MoveNode != nullptr)
    {
        m_Tree.Select(MoveNode->m_hTreeItem, TVGN_CARET);
    }
}

void CMasteringPage2::OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
    *pResult = 0;
    m_List.DeleteAllItems();
    {
        CDirStructure *NowNode, *p;
        HTREEITEM ht;
        int id;
        CString cs;
        ht = m_Tree.GetSelectedItem();
        NowNode = (CDirStructure*)m_Tree.GetItemData(ht);
        p = NowNode->GetDirectory();

        while (p != nullptr)
        {
            id = m_List.InsertItem(m_List.GetItemCount(), p->m_ImageFileName, 0);
            m_List.SetItemText(id, 1, MSG(85));
            cs.Format("%04d/%02d/%02d %02d:%02d:%02d",
                      p->m_TimeStamp.wYear, p->m_TimeStamp.wMonth, p->m_TimeStamp.wDay,
                      p->m_TimeStamp.wHour, p->m_TimeStamp.wMinute, p->m_TimeStamp.wSecond);
            m_List.SetItemText(id, 3, cs);
            m_List.SetItemText(id, 4, "Folder");
            p = p->GetNext();
        }

        p = NowNode->GetFile();

        while (p != nullptr)
        {
            id = m_List.InsertItem(m_List.GetItemCount(), p->m_ImageFileName, 1);
            m_List.SetItemText(id, 1, MSG(86));
            cs.Format("%ld", p->m_FileSize);
            m_List.SetItemText(id, 2, cs);
            cs.Format("%04d/%02d/%02d %02d:%02d:%02d",
                      p->m_TimeStamp.wYear, p->m_TimeStamp.wMonth, p->m_TimeStamp.wDay,
                      p->m_TimeStamp.wHour, p->m_TimeStamp.wMinute, p->m_TimeStamp.wSecond);
            m_List.SetItemText(id, 3, cs);
            m_List.SetItemText(id, 4, p->m_RealFileName);
            p = p->GetNext();
        }
    }
}

void CMasteringPage2::OnLvnEndlabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    *pResult = 0;
    int id;
    CString OldString;
    POSITION pos;
    LPCSTR NewString;
    CDirStructure* NowNode;
    CDirStructure* p;
    HTREEITEM ht;
    ht = m_Tree.GetSelectedItem();
    p = (CDirStructure*)m_Tree.GetItemData(ht);
    pos = m_List.GetFirstSelectedItemPosition();
    id = m_List.GetNextSelectedItem(pos);
    OldString = m_List.GetItemText(id, 0);
    NewString = pDispInfo->item.pszText;

    if (NewString == nullptr || *NewString == '\0')
    {
        return;
    }

    NowNode = p->SearchName(OldString);

    if (NowNode != nullptr)
    {
        NowNode->m_ImageFileName = NewString;
        p->CalcShortName(m_MainDialog);

        if (NowNode->m_IsDirectory)
        {
            m_Tree.SetItemText(NowNode->m_hTreeItem, NowNode->m_ImageFileName);
        }

        m_List.SetItemText(id, 0, NowNode->m_ImageFileName);
    }

    else
    {
        MessageBox(MSG(87), WARNING_MSG);
    }
}

bool CMasteringPage2::AddFile(LPCSTR FileName)
{
    CDirStructure* p;
    HTREEITEM ht;
    LPCSTR sp, bp;
    ht = m_Tree.GetSelectedItem();
    p = (CDirStructure*)m_Tree.GetItemData(ht);
    sp = FileName;
    bp = FileName;

    while (*bp != '\0')
    {
        if (*(BYTE*)(bp) >= 0x80)
        {
            bp++;
        }

        else if (*bp == '\\')
        {
            sp = bp + 1;
        }

        bp++;
    }

    if (p->SearchName(sp) == nullptr && p->SearchFile(FileName) == nullptr)
    {
        CDirStructure* NewFile;
        HANDLE hFile;
        DWORD high;
        FILETIME fc, fr, fw;
        int id;
        CString cs;
        hFile = CreateFile(FileName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        NewFile = p->CreateFile();
        NewFile->m_ImageFileName = sp;
        NewFile->m_ImageFileNameShort = sp;
        NewFile->m_RealFileName = FileName;
        NewFile->m_FileSize = GetFileSize(hFile, &high);
        GetFileTime(hFile, &fc, &fr, &fw);
        FileTimeToSystemTime(&fw, &(NewFile->m_TimeStamp));
        CloseHandle(hFile);
        p->CalcShortName(m_MainDialog);
        id = m_List.InsertItem(m_List.GetItemCount(), NewFile->m_ImageFileName, 1);
        m_List.SetItemText(id, 1, "ƒtƒ@ƒCƒ‹");
        cs.Format("%ld", NewFile->m_FileSize);
        m_List.SetItemText(id, 2, cs);
        cs.Format("%04d/%02d/%02d %02d:%02d:%02d",
                  NewFile->m_TimeStamp.wYear, NewFile->m_TimeStamp.wMonth, NewFile->m_TimeStamp.wDay,
                  NewFile->m_TimeStamp.wHour, NewFile->m_TimeStamp.wMinute, NewFile->m_TimeStamp.wSecond);
        m_List.SetItemText(id, 3, cs);
        m_List.SetItemText(id, 4, NewFile->m_RealFileName);
        static_cast<CMasteringPage1*>(m_Page1)->InsertMode1MasteringTrack();
        return true;
    }

    return false;
}

CDirStructure* CMasteringPage2::AddFolder(LPCSTR FolderName)
{
    CDirStructure* p;
    HTREEITEM ht;
    ht = m_Tree.GetSelectedItem();
    p = (CDirStructure*)m_Tree.GetItemData(ht);

    if (p->SearchName(FolderName) == nullptr)
    {
        HTREEITEM NewNode;
        CDirStructure* NewFolder;
        int id;
        CString cs;
        struct tm *tmp, tmp2;
        CTime NowTime = CTime::GetCurrentTime();
        tmp = NowTime.GetGmtTm(&tmp2);
        NewFolder = p->CreateDirectory();
        NewNode = m_Tree.InsertItem(FolderName, 0, 1, ht, TVI_LAST);
        NewFolder->m_ImageFileName = FolderName;
        NewFolder->m_ImageFileNameShort = FolderName;
        NewFolder->m_RealFileName = "";
        NewFolder->m_hTreeItem = NewNode;
        NewFolder->m_FileSize = 0;
        NewFolder->m_IsDirectory = true;
        NewFolder->m_TimeStamp.wYear = tmp->tm_year + 1900;
        NewFolder->m_TimeStamp.wMonth = tmp->tm_mon + 1;
        NewFolder->m_TimeStamp.wDay = tmp->tm_mday;
        NewFolder->m_TimeStamp.wDayOfWeek = tmp->tm_wday - 1;
        NewFolder->m_TimeStamp.wHour = tmp->tm_hour;
        NewFolder->m_TimeStamp.wMinute = tmp->tm_min;
        NewFolder->m_TimeStamp.wSecond = tmp->tm_sec;
        NewFolder->m_TimeStamp.wMilliseconds = 0;
        p->CalcShortName(m_MainDialog);
        m_Tree.SetItemData(NewNode, (DWORD)NewFolder);
        id = m_List.InsertItem(m_List.GetItemCount(), FolderName, 0);
        m_List.SetItemText(id, 1, "ƒtƒHƒ‹ƒ_");
        cs.Format("%04d/%02d/%02d %02d:%02d:%02d",
                  NewFolder->m_TimeStamp.wYear, NewFolder->m_TimeStamp.wMonth, NewFolder->m_TimeStamp.wDay,
                  NewFolder->m_TimeStamp.wHour, NewFolder->m_TimeStamp.wMinute, NewFolder->m_TimeStamp.wSecond);
        m_List.SetItemText(id, 3, cs);
        m_List.SetItemText(id, 4, "Folder");
        static_cast<CMasteringPage1*>(m_Page1)->InsertMode1MasteringTrack();
        return NewFolder;
    }

    return nullptr;
}

void CMasteringPage2::AddFileRec(LPCSTR PathName)
{
    DWORD attribute;
    HTREEITEM ht;
    ht = m_Tree.GetSelectedItem();
    attribute = GetFileAttributes(PathName);

    if (attribute == 0xffffffff) { return; }

    if (attribute & FILE_ATTRIBUTE_DIRECTORY)
    {
        HANDLE hFind;
        BOOL flag;
        WIN32_FIND_DATA data;
        char olddir[1024];
        CDirStructure* p;
        GetCurrentDirectory(1024, olddir);
        SetCurrentDirectory(PathName);
        hFind = FindFirstFile("*.*", &data);

        if (hFind == INVALID_HANDLE_VALUE) { return; }

        {
            LPCSTR sp, bp;
            sp = PathName;
            bp = PathName;

            while (*bp != '\0')
            {
                if (*(BYTE*)(bp) >= 0x80)
                {
                    bp++;
                }

                else if (*bp == '\\')
                {
                    sp = bp + 1;
                }

                bp++;
            }

            p = AddFolder(sp);

            if (p == nullptr)
            {
                return;
            }

            m_Tree.Select(p->m_hTreeItem, TVGN_CARET);
        }

        flag = TRUE;

        while (flag)
        {
            if (data.cFileName[0] != '.')
            {
                char fullname[1024];
                GetCurrentDirectory(1024, fullname);
                lstrcat(fullname, "\\");
                lstrcat(fullname, data.cFileName);
                AddFileRec(fullname);
            }

            flag = FindNextFile(hFind, &data);
        }

        FindClose(hFind);
        SetCurrentDirectory(olddir);
        m_Tree.Select(ht, TVGN_CARET);
    }

    else
    {
        AddFile(PathName);
    }
}

void CMasteringPage2::OnDropFiles(HDROP hDropInfo)
{
    char buf[2048];
    int i, max;
    UpdateData(TRUE);
    max = DragQueryFile(hDropInfo, 0xffffffff, nullptr, 0);

    for (i = 0; i < max; i++)
    {
        DragQueryFile(hDropInfo, i, buf, 2048);
        AddFileRec(buf);
    }

    CDialog::OnDropFiles(hDropInfo);
    UpdateData(FALSE);
    static_cast<CMasteringFileDialog*>(m_MainDialog)->CalcSize();
}

void CMasteringPage2::DeleteSelectedItems(void)
{
    int id;
    POSITION pos;
    CDirStructure *NowNode, *DelNode;
    HTREEITEM ht;
    CString DelNodeName;
    UpdateData(TRUE);
    ht = m_Tree.GetSelectedItem();
    NowNode = (CDirStructure*)m_Tree.GetItemData(ht);

    while (true)
    {
        pos = m_List.GetFirstSelectedItemPosition();
        id = m_List.GetNextSelectedItem(pos);
        DelNodeName = m_List.GetItemText(id, 0);
        DelNode = NowNode->SearchName(DelNodeName);

        if (DelNode != nullptr)
        {
            if (DelNode->m_IsDirectory)
            {
                m_Tree.DeleteItem(DelNode->m_hTreeItem);
            }

            m_List.DeleteItem(id);
            NowNode->DeleteNode(DelNode);
        }

        else
        {
            break;
        }
    }

    UpdateData(FALSE);
    static_cast<CMasteringFileDialog*>(m_MainDialog)->CalcSize();
}
