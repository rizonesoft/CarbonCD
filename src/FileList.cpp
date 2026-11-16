#include "StdAfx.h"
#include "filelist.h"

CFileList::CFileList(void)
{
    m_Dir = nullptr;
    m_Next = nullptr;
}

CFileList::~CFileList(void)
{
    if (m_Next != nullptr)
    {
        delete m_Next;
    }
}

CFileList* CFileList::CreateNext(void)
{
    if (m_Next == nullptr)
    {
        m_Next = new CFileList;
    }

    return m_Next;
}

CFileList* CFileList::GetNext(void)
{
    return m_Next;
}

void CFileList::SetNext(CFileList* Node)
{
    m_Next = Node;
}
