#pragma once

#include "DirStructure.h"

class CFileList {
    public:
        CFileList ( void );
        ~CFileList ( void );
        CDirStructure *m_Dir;
        DWORD m_LBA;
        DWORD m_No;
        DWORD m_ParentNo;
        int m_RecordSize;
    protected:
        CFileList *m_Next;
    public:
        CFileList * CreateNext ( void );
        CFileList * GetNext ( void );
        void SetNext ( CFileList * Node );
};
