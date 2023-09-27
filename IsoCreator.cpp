#include "StdAfx.h"
#include "isocreator.h"

CIsoCreator::CIsoCreator(void)
{
    WORD Coefficient[15] =
    {
        0x3a16, 0x2c25, 0x3c27,
        0x3d32, 0x7d21, 0x7c36,
        0x6775, 0x6565, 0x7966,
        0x0c75, 0x1e7b, 0x3b34,
        0x3630, 0x3c3d, 0x343e
    };
    int i, j;
    WORD Counter;
    auto p = (BYTE*)Coefficient;
    BYTE* q = m_CPBuffer + 12;
    memset(m_CPBuffer, 0, 2352);
    Counter = 1;

    for (i = 0; i < 2340; i++)
    {
        for (j = 0; j < 8; j++)
        {
            if (Counter & 1)
            {
                q[i] = q[i] | (1 << j);
            }

            if ((Counter & 1) != ((Counter & 2) >> 1))
            {
                Counter = Counter | 0x8000;
            }

            Counter = Counter >> 1;
        }

        q[i] = q[i] ^ (p[i % 30] ^ 0x55);
    }

    m_FileList = nullptr;
    m_DirList = nullptr;
#if COPY_PROTECTION
    m_CopyProtectionSize = 0;
    m_WroteProtection = 0;
#endif
    m_VolumeLabel = "";
    m_hFile = INVALID_HANDLE_VALUE;
}

CIsoCreator::~CIsoCreator(void)
{
    if (m_FileList != nullptr)
    {
        delete m_FileList;
    }

    if (m_DirList != nullptr)
    {
        delete m_DirList;
    }

    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }
}

void CIsoCreator::SetParams(LPCSTR VolumeLabel, int CopyProtectionSize)
{
    m_VolumeLabel = VolumeLabel;

    if (m_VolumeLabel.GetLength() > 30)
    {
        m_VolumeLabel = m_VolumeLabel.Left(30);
    }

#if COPY_PROTECTION
    m_CopyProtectionSize = CopyProtectionSize;
    m_WroteProtection = 0;
#endif
}

void CIsoCreator::CreateJolietHeader(CDirStructure* Dir)
{
    int i;
    BYTE Buffer[2048];
    DWORD PathTableSectors;
    m_Dir = Dir;
    m_BufferSize = 0;
    m_DirRecordSize[0] = 0;
    m_DirRecordSize[1] = 0;
    //   write 16 blank sector
    memset(Buffer, 0, 2048);

    for (i = 0; i < 0x10; i++)
    {
        m_Writer.WriteHeader(Buffer);
    }

    //   calc PATH TABLE SIZE
    m_PTSizeShort = 10; //   PATH TABLE for Primary
    m_PTSizeLong = 10; //   PATH TABLE for UNICODE

    if (m_Dir->GetDirectory() != nullptr)
    {
        m_PTSizeShort += m_Dir->GetDirectory()->CalcPathTableSize(0);
        m_PTSizeLong += m_Dir->GetDirectory()->CalcPathTableSize(1);
    }

    if (m_PTSizeShort % 2048)
    {
        m_PTSizeShort = m_PTSizeShort / 2048 + 1;
    }

    else
    {
        m_PTSizeShort = m_PTSizeShort / 2048;
    }

    if (m_PTSizeLong % 2048)
    {
        m_PTSizeLong = m_PTSizeLong / 2048 + 1;
    }

    else
    {
        m_PTSizeLong = m_PTSizeLong / 2048;
    }

    PathTableSectors = (m_PTSizeShort + m_PTSizeLong) * 2;
    //   list directories
    {
        CFileList *p, *q;
        int count;

        if (m_DirList != nullptr)
        {
            delete m_DirList;
            m_DirList = nullptr;
        }

        m_DirList = new CFileList;
        m_DirList->m_Dir = m_Dir;
        m_DirList->m_No = 1;
        m_DirList->m_ParentNo = 1;
        m_Dir->m_No = 1;
        q = m_DirList;
        p = ListDirectories(m_Dir, m_DirList, 0);
        count = 1;

        while (p != q)
        {
            q = p;
            p = ListDirectories(m_Dir, p, count);
            count++;
        }
    }
    //   list files
    {
        if (m_FileList != nullptr)
        {
            delete m_FileList;
            m_FileList = nullptr;
        }

        ListFiles(m_Dir, m_FileList);
    }
    //   Emulate Directory Record
    {
        CFileList *root, *p;
        p = m_DirList;
        m_DirRecordSize[0] = 0;

        while (p != nullptr)
        {
            root = SortFiles(p->m_Dir, 0);
            p->m_RecordSize = EmulateDirRecord(root, 0, p->m_No, p->m_ParentNo);
            delete root;
            m_DirRecordSize[0] += p->m_RecordSize;
            p = p->GetNext();
        }

        p = m_DirList;
        m_DirRecordSize[1] = 0;

        while (p != nullptr)
        {
            root = SortFiles(p->m_Dir, 1);
            p->m_RecordSize = EmulateDirRecord(root, 1, p->m_No, p->m_ParentNo);
            delete root;
            m_DirRecordSize[1] += p->m_RecordSize;
            p = p->GetNext();
        }
    }
    //   calc file LBA
    {
        CFileList* p;
        DWORD LBA;
        DWORD LBASize;
        LBA = 0x13 + PathTableSectors + m_DirRecordSize[0] + m_DirRecordSize[1];
#if COPY_PROTECTION
        LBA += m_CopyProtectionSize;
#endif
        p = m_FileList;

        while (p != nullptr)
        {
            p->m_LBA = LBA;
            p->m_Dir->m_LBA = LBA;
            LBASize = p->m_Dir->m_FileSize / 2048;

            if (p->m_Dir->m_FileSize % 2048)
            {
                LBASize++;
            }

            if (LBASize == 0)
            {
                LBASize = 1;
            }

            LBA += LBASize;
            p = p->GetNext();
        }

        m_ImageSize = LBA;
    }
    //   create Volume Descriptor
    {
        Buffer[2048];
        CreatePVD(Buffer);
        m_Writer.WriteHeader(Buffer);
        CreateSVD(Buffer);
        m_Writer.WriteHeader(Buffer);
        CreateEVD(Buffer);
        m_Writer.WriteHeader(Buffer);
    }
    //   create Path Table
    {
        CreatePathTable(0);
        CreatePathTable(1);
    }
    //   create Directory Record
    {
        CFileList *root, *p;
        p = m_DirList;

        while (p != nullptr)
        {
            root = SortFiles(p->m_Dir, 0);
            CreateDirRecord(root, 0, p->m_No, p->m_ParentNo);
            delete root;
            p = p->GetNext();
        }

        p = m_DirList;

        while (p != nullptr)
        {
            root = SortFiles(p->m_Dir, 1);
            CreateDirRecord(root, 1, p->m_No, p->m_ParentNo);
            delete root;
            p = p->GetNext();
        }
    }
}

void CIsoCreator::CreatePVD(BYTE* pvd)
{
    DWORD Size;
    SYSTEMTIME st;
    CTime tm = CTime::GetCurrentTime();
    tm.GetAsSystemTime(st);
    memset(pvd, 0, 2048);
    pvd[0x000] = 1;
    memcpy(pvd + 1, "CD001", 5);
    pvd[0x006] = 1;
    pvd[0x007] = 0;
    memset(pvd + 0x008, 0x20, 0x20);
    sprintf((char*)(pvd + 0x28), "%-30s", static_cast<LPCSTR>(m_VolumeLabel));
    *((DWORD*)(pvd + 0x50)) = m_ImageSize;
    *((DWORD*)(pvd + 0x54)) = LeToBe(m_ImageSize);
    *((WORD*)(pvd + 0x78)) = 1;
    *((WORD*)(pvd + 0x7a)) = LeToBeShort(1);
    *((WORD*)(pvd + 0x7c)) = 1;
    *((WORD*)(pvd + 0x7e)) = LeToBeShort(1);
    *((WORD*)(pvd + 0x80)) = 2048;
    *((WORD*)(pvd + 0x82)) = LeToBeShort(2048);
    *((DWORD*)(pvd + 0x84)) = m_PTSizeShort * 2048;
    *((DWORD*)(pvd + 0x88)) = LeToBe(m_PTSizeShort * 2048);
    *((DWORD*)(pvd + 0x8c)) = 0x13;
    *((DWORD*)(pvd + 0x94)) = LeToBe(0x13 + m_PTSizeShort);
    pvd[0x09c] = 0x22;
    Size = 0x13 + (m_PTSizeShort + m_PTSizeLong) * 2;
    *((DWORD*)(pvd + 0x9e)) = Size;
    *((DWORD*)(pvd + 0xa2)) = LeToBe(Size);
    *((DWORD*)(pvd + 0xa6)) = m_DirList->m_RecordSize;
    *((DWORD*)(pvd + 0xaa)) = LeToBe(Size);
    SetDate(pvd + 0xae, m_Dir->m_TimeStamp);
    pvd[0x0b5] = 2;
    *((WORD*)(pvd + 0xb8)) = 1;
    *((WORD*)(pvd + 0xba)) = LeToBeShort(1);
    pvd[0x0bb] = 1;
    pvd[0x0bc] = 1;
    pvd[0x0bd] = 0;
    memset(pvd + 0x0be, 0x20, 0x26f);
    SetDate_L(pvd + 0x32d, st);
    SetDate_L(pvd + 0x33e, st);
    memset(pvd + 0x34f, 0x30, 17 * 2);
    pvd[0x34f + 16] = 0;
    pvd[0x34f + 16 + 17] = 0;
}

void CIsoCreator::CreateSVD(BYTE* pvd)
{
    DWORD Size;
    SYSTEMTIME st;
    CTime tm = CTime::GetCurrentTime();
    tm.GetAsSystemTime(st);
    memset(pvd, 0, 2048);
    pvd[0x000] = 2;
    memcpy(pvd + 1, "CD001", 5);
    pvd[0x006] = 1;
    pvd[0x007] = 02;
    memset(pvd + 0x008, 0x20, 0x20);
    {
        CString cs;
        CStringW ws;
        cs.Format("%-32s", m_VolumeLabel);
        ws = cs;
        ByteSwapCopy(pvd + 0x28, ws, 32);
    }
    *((DWORD*)(pvd + 0x50)) = m_ImageSize;
    *((DWORD*)(pvd + 0x54)) = LeToBe(m_ImageSize);
    memset(pvd + 0x58, 0x20, 32);
    pvd[0x58] = 0x25;
    pvd[0x59] = 0x2F;
    pvd[0x5a] = 0x45;
    *((WORD*)(pvd + 0x78)) = 1;
    *((WORD*)(pvd + 0x7a)) = LeToBeShort(1);
    *((WORD*)(pvd + 0x7c)) = 1;
    *((WORD*)(pvd + 0x7e)) = LeToBeShort(1);
    *((WORD*)(pvd + 0x80)) = 2048;
    *((WORD*)(pvd + 0x82)) = LeToBeShort(2048);
    *((DWORD*)(pvd + 0x84)) = m_PTSizeLong * 2048;
    *((DWORD*)(pvd + 0x88)) = LeToBe(m_PTSizeLong * 2048);
    *((DWORD*)(pvd + 0x8c)) = 0x13 + m_PTSizeShort * 2;
    *((DWORD*)(pvd + 0x94)) = LeToBe(0x13 + m_PTSizeShort * 2 + m_PTSizeLong);
    pvd[0x09c] = 0x22;
    Size = 0x13 + (m_PTSizeShort + m_PTSizeLong) * 2 + m_DirRecordSize[0];
    *((DWORD*)(pvd + 0x9e)) = Size;
    *((DWORD*)(pvd + 0xa2)) = LeToBe(Size);
    *((DWORD*)(pvd + 0xa6)) = m_DirList->m_RecordSize;
    *((DWORD*)(pvd + 0xaa)) = LeToBe(Size);
    SetDate(pvd + 0xae, m_Dir->m_TimeStamp);
    pvd[0x0b5] = 2;
    *((WORD*)(pvd + 0xb8)) = 1;
    *((WORD*)(pvd + 0xba)) = LeToBeShort(1);
    pvd[0x0bb] = 1;
    pvd[0x0bc] = 1;
    pvd[0x0bd] = 0;
    memset(pvd + 0x0be, 0x20, 0x26f);
    SetDate_L(pvd + 0x32d, st);
    SetDate_L(pvd + 0x33e, st);
    memset(pvd + 0x34f, 0x30, 17 * 2);
    pvd[0x34f + 16] = 0;
    pvd[0x34f + 16 + 17] = 0;
}

void CIsoCreator::CreateEVD(BYTE* pvd)
{
    memset(pvd, 0, 2048);
    pvd[0x000] = 0xff;
    memcpy(pvd + 1, "CD001", 5);
    pvd[0x006] = 1;
}

CFileList* CIsoCreator::ListFiles(CDirStructure* Dir, CFileList* List)
{
    CDirStructure* p;
    p = Dir->GetFile();

    while (p != nullptr)
    {
        //   create new node
        if (m_FileList == nullptr)
        {
            m_FileList = new CFileList;
            List = m_FileList;
        }

        else
        {
            List = List->CreateNext();
        }

        List->m_Dir = p;
        p = p->GetNext();
    }

    p = Dir->GetDirectory();

    while (p != nullptr)
    {
        List = ListFiles(p, List);
        p = p->GetNext();
    }

    return List;
}

CFileList* CIsoCreator::ListDirectories(CDirStructure* Dir, CFileList* List, int Level)
{
    CDirStructure* p;
    CFileList* l;
    DWORD No;
    l = List;
    p = Dir->GetDirectory();

    if (Level == 0)
    {
        while (p != nullptr)
        {
            No = l->m_No + 1;
            l = l->CreateNext();
            l->m_Dir = p;
            l->m_ParentNo = Dir->m_No;
            l->m_No = No;
            p->m_No = No;
            p->m_ParentNo = Dir->m_No;
            p = p->GetNext();
        }
    }

    else
    {
        while (p != nullptr)
        {
            l = ListDirectories(p, l, Level - 1);
            p = p->GetNext();
        }
    }

    return l;
}

DWORD CIsoCreator::LeToBe(DWORD Le)
{
    BYTE LeIm[4], BeIm[4];
    *((DWORD*)(LeIm)) = Le;
    BeIm[0] = LeIm[3];
    BeIm[1] = LeIm[2];
    BeIm[2] = LeIm[1];
    BeIm[3] = LeIm[0];
    return *((DWORD*)(BeIm));
}

WORD CIsoCreator::LeToBeShort(WORD Le)
{
    BYTE LeIm[2], BeIm[2];
    *((WORD*)(LeIm)) = Le;
    BeIm[0] = LeIm[1];
    BeIm[1] = LeIm[0];
    return *((WORD*)(BeIm));
}

void CIsoCreator::SetDirRecord(BYTE* Buffer, CDirStructure* Dir)
{
}

void CIsoCreator::SetDate(BYTE* Buffer, SYSTEMTIME& Time)
{
    Buffer[0] = static_cast<BYTE>(Time.wYear - 1900);
    Buffer[1] = static_cast<BYTE>(Time.wMonth);
    Buffer[2] = static_cast<BYTE>(Time.wDay);
    Buffer[3] = static_cast<BYTE>(Time.wHour);
    Buffer[4] = static_cast<BYTE>(Time.wMinute);
    Buffer[5] = static_cast<BYTE>(Time.wSecond);
    Buffer[6] = 0;
}

void CIsoCreator::SetDate_L(BYTE* Buffer, SYSTEMTIME& Time)
{
    sprintf((char*)Buffer, "%04d%02d%02d%02d%02d%02d%02d",
            Time.wYear, Time.wMonth, Time.wDay,
            Time.wHour, Time.wMinute, Time.wSecond,
            Time.wMilliseconds / 10);
    Buffer[16] = 0;
}


void CIsoCreator::WriteBuffer(BYTE* Buffer, int Size)
{
    if (Buffer == nullptr || Size == 0)
    {
        if (m_BufferSize == 0)
        {
            return;
        }

        memset(m_Buffer + m_BufferSize, 0, 2048 - m_BufferSize);
        m_Writer.WriteHeader(m_Buffer);
        m_BufferSize = 0;
    }

    else
    {
        if (m_BufferSize + Size > 2048)
        {
            memset(m_Buffer + m_BufferSize, 0, Size);
            m_Writer.WriteHeader(m_Buffer);
            m_BufferSize = 0;
        }

        memcpy(m_Buffer + m_BufferSize, Buffer, Size);
        m_BufferSize += Size;
    }
}


void CIsoCreator::WriteBufferNB(BYTE* Buffer, int Size)
{
    if (Buffer == nullptr || Size == 0)
    {
        if (m_BufferSize == 0)
        {
            return;
        }

        memset(m_Buffer + m_BufferSize, 0, 2048 - m_BufferSize);
        m_Writer.WriteHeader(m_Buffer);
        m_BufferSize = 0;
    }

    else
    {
        if (m_BufferSize + Size > 2048)
        {
            memcpy(m_Buffer + m_BufferSize, Buffer, 2048 - m_BufferSize);
            Size -= 2048 - m_BufferSize;
            Buffer += 2048 - m_BufferSize;
            m_Writer.WriteHeader(m_Buffer);
            m_BufferSize = 0;
        }

        memcpy(m_Buffer + m_BufferSize, Buffer, Size);
        m_BufferSize += Size;
    }
}

void CIsoCreator::CreatePathTable(int Type)
{
    DWORD LBA;
    int LeBe;
    BYTE length;
    BYTE Buffer[2048];
    CFileList* p;

    for (LeBe = 0; LeBe < 2; LeBe++)
    {
        if (Type == 0)
        {
            LBA = 0x13 + (m_PTSizeShort + m_PTSizeLong) * 2;
        }

        else
        {
            LBA = 0x13 + (m_PTSizeShort + m_PTSizeLong) * 2 + m_DirRecordSize[0];
        }

        p = m_DirList;
        //   create root directory
        memset(Buffer, 0, 10);
        Buffer[0] = 0x01;

        if (LeBe)
        {
            *((DWORD*)(Buffer + 2)) = LeToBe(LBA);
        }

        else
        {
            *((DWORD*)(Buffer + 2)) = LBA;
        }

        if (LeBe)
        {
            *((WORD*)(Buffer + 6)) = LeToBeShort(0);
        }

        else
        {
            *((WORD*)(Buffer + 6)) = 0;
        }

        WriteBuffer(Buffer, 10);
        //   move next
        LBA += p->m_RecordSize;
        p = p->GetNext();

        //   create directories
        while (p != nullptr)
        {
            if (Type == 0)
            {
                length = p->m_Dir->m_ImageFileNameShort.GetLength();
            }

            else
            {
                CStringW cw;
                cw = p->m_Dir->m_ImageFileName;
                length = cw.GetLength() * 2;
            }

            memset(Buffer, 0, 9 + length);
            Buffer[0] = length;

            if (LeBe)
            {
                *((DWORD*)(Buffer + 2)) = LeToBe(LBA);
            }

            else
            {
                *((DWORD*)(Buffer + 2)) = LBA;
            }

            if (LeBe)
            {
                *((WORD*)(Buffer + 6)) = LeToBeShort(static_cast<WORD>(p->m_ParentNo));
            }

            else
            {
                *((WORD*)(Buffer + 6)) = static_cast<WORD>(p->m_ParentNo);
            }

            if (Type == 0)
            {
                memcpy(Buffer + 8, p->m_Dir->m_ImageFileNameShort, p->m_Dir->m_ImageFileNameShort.GetLength());
            }

            else
            {
                CStringW cw;
                cw = p->m_Dir->m_ImageFileName;
                ByteSwapCopy(Buffer + 8, cw, cw.GetLength() * 2);
            }

            if (length % 2) { length++; }

            WriteBufferNB(Buffer, 8 + length);
            //   move next
            LBA += p->m_RecordSize;
            p = p->GetNext();
        }

        //   flush buffer
        WriteBuffer(nullptr, 0);
    }
}

CFileList* CIsoCreator::SortFiles(CDirStructure* Dir, int Type)
{
    CFileList *root, *p, *tmp;
    CDirStructure* q;
    DWORD LBA;
    root = nullptr;
    q = Dir->GetDirectory();

    while (q != nullptr)
    {
        tmp = new CFileList;
        tmp->m_Dir = q;
        p = m_DirList;

        if (Type == 0)
        {
            LBA = 0x13 + (m_PTSizeShort + m_PTSizeLong) * 2;
        }

        else
        {
            LBA = 0x13 + (m_PTSizeShort + m_PTSizeLong) * 2 + m_DirRecordSize[0];
        }

        while (p != nullptr)
        {
            if (q->m_ImageFileName == p->m_Dir->m_ImageFileName)
            {
                tmp->m_LBA = LBA;
                tmp->m_No = p->m_No;
                tmp->m_ParentNo = p->m_ParentNo;
                break;
            }

            LBA += p->m_RecordSize;
            p = p->GetNext();
        }

        root = AddNode(root, tmp);
        q = q->GetNext();
    }

    q = Dir->GetFile();

    while (q != nullptr)
    {
        tmp = new CFileList;
        tmp->m_Dir = q;
        p = m_FileList;
        /*      while(p != NULL){
                    if(p->m_Dir->m_RealFileName == q->m_RealFileName){
                        tmp->m_LBA = p->m_LBA;
                        tmp->m_No = p->m_No;
                        tmp->m_ParentNo = p->m_ParentNo;
                        break;
                    }
                    p = p->GetNext();
                }*/
        tmp->m_LBA = q->m_LBA;
        tmp->m_No = q->m_No;
        tmp->m_ParentNo = q->m_ParentNo;
        root = AddNode(root, tmp);
        q = q->GetNext();
    }

    return root;
}

CFileList* CIsoCreator::AddNode(CFileList* RootNode, CFileList* NewNode)
{
    CFileList *p, *q;
    p = NewNode;

    if (RootNode == nullptr)
    {
        return p;
    }
    if (p->m_Dir->m_ImageFileName < RootNode->m_Dir->m_ImageFileName)
    {
        p->SetNext(RootNode);
        return p;
    }
    q = RootNode;

    while (q != nullptr)
    {
        if (q->GetNext() == nullptr)
        {
            q->SetNext(p);
            break;
        }
        if (p->m_Dir->m_ImageFileName < q->GetNext()->m_Dir->m_ImageFileName)
        {
            p->SetNext(q->GetNext());
            q->SetNext(p);
            break;
        }

        q = q->GetNext();
    }

    return RootNode;
}

DWORD CIsoCreator::EmulateDirRecord(CFileList* List, int Type, DWORD Current, DWORD Parent)
{
    DWORD length;
    DWORD Size, r;
    Size = 34 * 2;

    while (List != nullptr)
    {
        if (Type == 0)
        {
            length = List->m_Dir->m_ImageFileNameShort.GetLength();

            if (!List->m_Dir->m_IsDirectory)
            {
                length += 2;
            }
        }

        else
        {
            CStringW cw;
            cw = List->m_Dir->m_ImageFileName;
            length = cw.GetLength() * 2;

            if (!List->m_Dir->m_IsDirectory)
            {
                length += 4;
            }
        }

        r = length + 33;

        if (r % 2) { r++; }

        if (((Size % 2048) + r) > 2048)
        {
            Size += 2048 - (Size % 2048);
        }

        Size += r;
        List = List->GetNext();
    }

    //   flush buffer
    if ((Size % 2048) == 0)
    {
        return Size / 2048;
    }
    return (Size / 2048) + 1;
}

void CIsoCreator::CreateDirRecord(CFileList* List, int Type, DWORD Current, DWORD Parent)
{
    DWORD LBA, CurrentLBA, ParentLBA, CurrentSize, ParentSize, length;
    CFileList* p;
    BYTE Buffer[2048];
    SYSTEMTIME st;
    struct tm* tmp;
    CTime tme = CTime::GetCurrentTime();
    struct tm tmp2;
    tmp = tme.GetGmtTm(&tmp2);
    st.wYear = tmp->tm_year + 1900;
    st.wMonth = tmp->tm_mon + 1;
    st.wDay = tmp->tm_mday;
    st.wDayOfWeek = tmp->tm_wday - 1;
    st.wHour = tmp->tm_hour;
    st.wMinute = tmp->tm_min;
    st.wSecond = tmp->tm_sec;
    st.wMilliseconds = 0;

    if (Type == 0)
    {
        LBA = 0x13 + (m_PTSizeShort + m_PTSizeLong) * 2;
    }

    else
    {
        LBA = 0x13 + (m_PTSizeShort + m_PTSizeLong) * 2 + m_DirRecordSize[0];
    }

    CurrentSize = 0;
    ParentSize = 0;
    p = m_DirList;

    while (p != nullptr)
    {
        if (p->m_No == Current)
        {
            CurrentLBA = LBA;
        }

        if (p->m_ParentNo == Parent)
        {
            ParentLBA = LBA;
        }

        LBA += p->m_RecordSize;

        if (p->m_No == Current)
        {
            CurrentSize = p->m_RecordSize * 2048;

            if (ParentSize > 0) { break; }
        }

        if (p->m_ParentNo == Parent)
        {
            ParentSize = p->m_RecordSize * 2048;

            if (CurrentSize > 0) { break; }
        }

        p = p->GetNext();
    }

    //   create current dir record
    memset(Buffer, 0, 36);
    Buffer[0] = 34;
    *((DWORD*)(Buffer + 2)) = CurrentLBA;
    *((DWORD*)(Buffer + 6)) = LeToBe(CurrentLBA);
    *((DWORD*)(Buffer + 10)) = CurrentSize;
    *((DWORD*)(Buffer + 14)) = LeToBe(CurrentSize);
    SetDate(Buffer + 18, st);
    Buffer[25] = 0x02;
    *((WORD*)(Buffer + 28)) = 1;
    *((WORD*)(Buffer + 30)) = LeToBeShort(1);
    Buffer[32] = 1;
    WriteBuffer(Buffer, 34);
    //   create parent dir record
    Buffer[0] = 34;
    *((DWORD*)(Buffer + 2)) = ParentLBA;
    *((DWORD*)(Buffer + 6)) = LeToBe(ParentLBA);
    *((DWORD*)(Buffer + 10)) = ParentSize;
    *((DWORD*)(Buffer + 14)) = LeToBe(ParentSize);
    Buffer[33] = 1;
    WriteBuffer(Buffer, 34);

    while (List != nullptr)
    {
        if (Type == 0)
        {
            length = List->m_Dir->m_ImageFileNameShort.GetLength();

            if (!List->m_Dir->m_IsDirectory)
            {
                length += 2;
            }
        }

        else
        {
            CStringW cw;
            cw = List->m_Dir->m_ImageFileName;
            length = cw.GetLength() * 2;

            if (!List->m_Dir->m_IsDirectory)
            {
                length += 4;
            }
        }

        memset(Buffer, 0, 34 + length);
        Buffer[0] = static_cast<BYTE>(length + 33);

        if (Buffer[0] % 2) { Buffer[0]++; }

        *((DWORD*)(Buffer + 2)) = List->m_LBA;
        *((DWORD*)(Buffer + 6)) = LeToBe(List->m_LBA);

        if (List->m_Dir->m_IsDirectory)
        {
            *((DWORD*)(Buffer + 10)) = List->m_RecordSize * 2048;
            *((DWORD*)(Buffer + 14)) = LeToBe(List->m_RecordSize * 2048);
            SetDate(Buffer + 18, st);
            Buffer[25] = 0x02;
        }

        else
        {
            *((DWORD*)(Buffer + 10)) = List->m_Dir->m_FileSize;
            *((DWORD*)(Buffer + 14)) = LeToBe(List->m_Dir->m_FileSize);
            SetDate(Buffer + 18, (List->m_Dir->m_TimeStamp));
            Buffer[25] = 0x00;
        }

        *((WORD*)(Buffer + 28)) = 1;
        *((WORD*)(Buffer + 30)) = LeToBeShort(1);
        Buffer[32] = static_cast<BYTE>(length);

        if (Type == 0)
        {
            CString cs;
            cs = List->m_Dir->m_ImageFileNameShort + ";1";
            memcpy(Buffer + 33, cs, length);
        }

        else
        {
            CStringW cw;
            cw = List->m_Dir->m_ImageFileName + ";1";
            ByteSwapCopy(Buffer + 33, cw, length);
        }

        WriteBuffer(Buffer, Buffer[0]);
        List = List->GetNext();
    }

    //   flush buffer
    WriteBuffer(nullptr, 0);
}

void CIsoCreator::InitializeReading(void)
{
    m_ReadingFile = m_FileList;

    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }
}

bool CIsoCreator::OpenReadFile(void)
{
    CloseReadFile();

    if (m_ReadingFile == nullptr)
    {
        return false;
    }

    m_hFile = CreateFile(m_ReadingFile->m_Dir->m_RealFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, nullptr);

    if (m_hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    return true;
}

void CIsoCreator::CloseReadFile(void)
{
    if (m_hFile == INVALID_HANDLE_VALUE)
    {
        return;
    }

    CloseHandle(m_hFile);
    m_hFile = INVALID_HANDLE_VALUE;
    m_ReadingFile = m_ReadingFile->GetNext();
}

bool CIsoCreator::GetHeaderFrame(BYTE* Buffer)
{
    if (m_Writer.GetHeaderFrame(Buffer))
    {
        return true;
    }

    return false;
}

#if COPY_PROTECTION
bool CIsoCreator::GetProtectionArea(BYTE* Buffer)
{
    if (m_WroteProtection < m_CopyProtectionSize)
    {
        m_Writer.GenerateData(Buffer, m_CPBuffer + 16);

        if ((m_WroteProtection % 3) != 2)
        {
            if (m_WroteProtection >= 150 && (m_CopyProtectionSize - m_WroteProtection) >= 150)
            {
                memcpy(Buffer, m_CPBuffer, 2352);
            }
        }

        m_WroteProtection++;
        return true;
    }

    return false;
}
#endif

void CIsoCreator::CreateZeroSector(BYTE* Buffer)
{
    memset(m_Buffer, 0, 2048);
    m_Writer.GenerateData(Buffer, m_Buffer);
}

bool CIsoCreator::GetFrame(BYTE* Buffer)
{
    DWORD read;

    if (m_hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    ReadFile(m_hFile, m_Buffer, 2048, &read, nullptr);

    if (read == 0)
    {
        if (OpenReadFile())
        {
            ReadFile(m_hFile, m_Buffer, 2048, &read, nullptr);
        }

        else
        {
            return false;
        }
    }

    if (read < 2048)
    {
        memset(m_Buffer + read, 0, 2048 - read);
    }

    m_Writer.GenerateData(Buffer, m_Buffer);
    return true;
}

void CIsoCreator::ByteSwapCopy(void* Buffer, const void* Source, DWORD CopyCount)
{
    const BYTE* src = (BYTE*)Source;
    auto dest = static_cast<BYTE*>(Buffer);

    while (CopyCount > 0)
    {
        dest[0] = src[1];
        dest[1] = src[0];
        CopyCount -= 2;
        dest += 2;
        src += 2;
    }
}

DWORD CIsoCreator::GetCurrentLBA(void)
{
    return m_Writer.GetLBA() - 150;
}

DWORD CIsoCreator::GetImageSize(void)
{
    return m_ImageSize;
}
