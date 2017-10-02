#include "StdAfx.h"
#include "dirstructure.h"
#include "Setting.h"

CDirStructure::CDirStructure ( void )
{
    m_DirList = NULL;
    m_FileList = NULL;
    m_DirListTail = NULL;
    m_FileListTail = NULL;
    m_Next = NULL;
    m_IsDirectory = false;
}

CDirStructure::~CDirStructure ( void )
{
    Clear();
}

void CDirStructure::SetNext ( CDirStructure *Next )
{
    m_Next = Next;
}

CDirStructure * CDirStructure::CreateDirectory ( void )
{
    CDirStructure *p;
    p = new CDirStructure;

    if ( m_DirList == NULL )
        {
            m_DirList = p;
            m_DirListTail = p;
        }

    else
        {
            m_DirListTail->SetNext ( p );
            m_DirListTail = p;
        }

    return p;
}

CDirStructure * CDirStructure::CreateFile ( void )
{
    CDirStructure *p;
    p = new CDirStructure;

    if ( m_FileList == NULL )
        {
            m_FileList = p;
            m_FileListTail = p;
        }

    else
        {
            m_FileListTail->SetNext ( p );
            m_FileListTail = p;
        }

    return p;
}

CDirStructure * CDirStructure::GetNext ( void )
{
    return m_Next;
}

CDirStructure * CDirStructure::GetDirectory ( void )
{
    return m_DirList;
}

CDirStructure * CDirStructure::GetFile ( void )
{
    return m_FileList;
}

CDirStructure * CDirStructure::SearchName ( LPCSTR Name )
{
    CDirStructure * ret = NULL;

    if ( m_DirList != NULL )
        {
            ret = m_DirList->Search ( Name );
        }

    if ( ret == NULL && m_FileList != NULL )
        {
            ret = m_FileList->Search ( Name );
        }

    return ret;
}

CDirStructure * CDirStructure::SearchFile ( LPCSTR Name )
{
    CDirStructure * ret = NULL;

    if ( ret == NULL && m_FileList != NULL )
        {
            ret = m_FileList->SearchRealFile ( Name );
        }

    return ret;
}

CDirStructure * CDirStructure::SearchNameShort ( LPCSTR Name )
{
    CDirStructure * ret = NULL;

    if ( m_DirList != NULL )
        {
            ret = m_DirList->SearchShort ( Name );
        }

    if ( ret == NULL && m_FileList != NULL )
        {
            ret = m_FileList->SearchShort ( Name );
        }

    return ret;
}

CDirStructure * CDirStructure::Search ( LPCSTR Name )
{
    if ( lstrcmp ( Name, ( LPCSTR ) m_ImageFileName ) == 0 )
        {
            return this;
        }

    if ( m_Next != NULL )
        {
            return m_Next->Search ( Name );
        }

    return NULL;
}

CDirStructure * CDirStructure::SearchShort ( LPCSTR Name )
{
    if ( lstrcmp ( Name, ( LPCSTR ) m_ImageFileNameShort ) == 0 )
        {
            return this;
        }

    if ( m_Next != NULL )
        {
            return m_Next->SearchShort ( Name );
        }

    return NULL;
}

CDirStructure * CDirStructure::SearchRealFile ( LPCSTR Name )
{
    if ( lstrcmp ( Name, ( LPCSTR ) m_RealFileName ) == 0 )
        {
            return this;
        }

    if ( m_Next != NULL )
        {
            return m_Next->SearchShort ( Name );
        }

    return NULL;
}

void CDirStructure::DeleteNode ( CDirStructure *DelNode )
{
    CDirStructure *p;
    p = m_DirList;

    if ( p == DelNode )
        {
            m_DirList = p->GetNext();

            if ( m_DirList == NULL )
                {
                    m_DirListTail = NULL;
                }

            p->SetNext ( NULL );
            delete p;
        }

    else
        {
            while ( p != NULL )
                {
                    if ( p->GetNext() == DelNode )
                        {
                            p->SetNext ( DelNode->GetNext() );
                            DelNode->SetNext ( NULL );
                            delete DelNode;

                            if ( p->GetNext() == NULL )
                                {
                                    m_DirListTail = p;
                                }

                            break;
                        }

                    p = p->GetNext();
                }
        }

    p = m_FileList;

    if ( p == DelNode )
        {
            m_FileList = p->GetNext();

            if ( m_FileList == NULL )
                {
                    m_FileListTail = NULL;
                }

            p->SetNext ( NULL );
            delete p;
        }

    else
        {
            while ( p != NULL )
                {
                    if ( p->GetNext() == DelNode )
                        {
                            p->SetNext ( DelNode->GetNext() );
                            DelNode->SetNext ( NULL );
                            delete DelNode;

                            if ( p->GetNext() == NULL )
                                {
                                    m_FileListTail = p;
                                }

                            break;
                        }

                    p = p->GetNext();
                }
        }
}

void CDirStructure::CalcShortName ( CWnd *ParentWnd )
{
    CDirStructure *p;
    CString NewName, file, ext;
    p = m_DirList;

    while ( p != NULL )
        {
            if ( p->m_ImageFileNameShort.GetLength() > 8 )
                {
                    int i;

                    for ( i = 0; i < 10; i++ )
                        {
                            NewName.Format ( "%s~%d", ( LPCSTR ) p->m_ImageFileNameShort.Left ( 6 ), i );

                            if ( SearchNameShort ( NewName ) == NULL )
                                {
                                    p->m_ImageFileNameShort = NewName;
                                    break;
                                }
                        }

                    if ( i >= 10 )
                        {
                            for ( i = 0; i < 100; i++ )
                                {
                                    NewName.Format ( "%s~%02d", ( LPCSTR ) p->m_ImageFileNameShort.Left ( 5 ), i );

                                    if ( SearchNameShort ( NewName ) == NULL )
                                        {
                                            p->m_ImageFileNameShort = NewName;
                                            break;
                                        }
                                }
                        }

                    if ( i >= 100 )
                        {
                            for ( i = 0; i < 10000; i++ )
                                {
                                    NewName.Format ( "%s~%04d", ( LPCSTR ) p->m_ImageFileNameShort.Left ( 3 ), i );

                                    if ( SearchNameShort ( NewName ) == NULL )
                                        {
                                            p->m_ImageFileNameShort = NewName;
                                            break;
                                        }
                                }
                        }
                }

            if ( p->m_ImageFileName.GetLength() > 64 )
                {
                    char Buffer[2048];
                    CString fn;
                    CString OldName, cs, a, b;
                    OldName = p->m_ImageFileName;
                    lstrcpy ( Buffer, p->m_ImageFileName );
                    Buffer[64] = '\0';
                    TruncSJIS ( Buffer );
                    fn.Format ( "%s", Buffer );

                    if ( SearchName ( fn ) == NULL )
                        {
                            p->m_ImageFileName = fn;
                        }

                    else
                        {
                            int i;
                            Buffer[64 - 3] = '\0';
                            TruncSJIS ( Buffer );

                            for ( i = 0; i < 100; i++ )
                                {
                                    fn.Format ( "%s_%02d", Buffer, i );

                                    if ( SearchName ( fn ) == NULL )
                                        {
                                            p->m_ImageFileName = fn;
                                            break;
                                        }
                                }

                            if ( i >= 100 )
                                {
                                    Buffer[64 - 5] = '\0';
                                    TruncSJIS ( Buffer );

                                    for ( i = 0; i < 10000; i++ )
                                        {
                                            fn.Format ( "%s_%04d", Buffer, i );

                                            if ( SearchName ( fn ) == NULL )
                                                {
                                                    p->m_ImageFileName = fn;
                                                    break;
                                                }
                                        }
                                }
                        }

                    a.Format ( MSG ( 148 ), OldName );
                    b.Format ( MSG ( 149 ), p->m_ImageFileName );
                    cs.Format ( "%s\n%s\n%s", MSG ( 147 ), a, b );
                    ParentWnd->MessageBox ( cs, theSetting.m_Lang.m_Str[5], MB_OK );
                }

            p = p->GetNext();
        }

    p = m_FileList;

    while ( p != NULL )
        {
            {
                LPSTR q;
                char Buffer[2048];
                strcpy ( Buffer, ( LPCSTR ) p->m_ImageFileNameShort );
                q = Buffer + lstrlen ( Buffer );

                while ( q > Buffer )
                    {
                        if ( *q == '.' )
                            {
                                break;
                            }

                        q--;
                    }

                if ( *q != '.' )
                    {
                        q = Buffer + lstrlen ( Buffer );
                    }

                file = Buffer;
                ext = q;
            }

            if ( file.GetLength() > 12 || ext.GetLength() > 4 )
                {
                    int i;

                    for ( i = 0; i < 10; i++ )
                        {
                            NewName.Format ( "%s~%d%s", ( LPCSTR ) file.Left ( 6 ), i, ext.Left ( 4 ) );

                            if ( SearchNameShort ( NewName ) == NULL )
                                {
                                    p->m_ImageFileNameShort = NewName;
                                    break;
                                }
                        }

                    if ( i >= 10 )
                        {
                            for ( i = 0; i < 100; i++ )
                                {
                                    NewName.Format ( "%s~%02d%s", ( LPCSTR ) file.Left ( 5 ), i, ext.Left ( 4 ) );

                                    if ( SearchNameShort ( NewName ) == NULL )
                                        {
                                            p->m_ImageFileNameShort = NewName;
                                            break;
                                        }
                                }
                        }

                    if ( i >= 100 )
                        {
                            for ( i = 0; i < 10000; i++ )
                                {
                                    NewName.Format ( "%s~%04d%s", ( LPCSTR ) file.Left ( 3 ), i, ext.Left ( 4 ) );

                                    if ( SearchNameShort ( NewName ) == NULL )
                                        {
                                            p->m_ImageFileNameShort = NewName;
                                            break;
                                        }
                                }
                        }
                }

            if ( p->m_ImageFileName.GetLength() > 64 )
                {
                    LPSTR q;
                    CString ext, fn;
                    char Buffer[2048];
                    CString OldName, cs, a, b;
                    OldName = p->m_ImageFileName;
                    strcpy ( Buffer, ( LPCSTR ) p->m_ImageFileName );
                    q = Buffer + lstrlen ( Buffer );

                    while ( q > Buffer )
                        {
                            if ( *q == '.' )
                                {
                                    break;
                                }

                            q--;
                        }

                    if ( *q != '.' )
                        {
                            q = Buffer + lstrlen ( Buffer );
                        }

                    ext = q;
                    Buffer[64 - ext.GetLength()] = '\0';
                    TruncSJIS ( Buffer );
                    fn.Format ( "%s%s", Buffer, ext );

                    if ( SearchName ( fn ) == NULL )
                        {
                            p->m_ImageFileName = fn;
                        }

                    else
                        {
                            int i;
                            Buffer[64 - ext.GetLength() - 3] = '\0';
                            TruncSJIS ( Buffer );

                            for ( i = 0; i < 100; i++ )
                                {
                                    fn.Format ( "%s_%02d%s", Buffer, i, ext );

                                    if ( SearchName ( fn ) == NULL )
                                        {
                                            p->m_ImageFileName = fn;
                                            break;
                                        }
                                }

                            if ( i >= 100 )
                                {
                                    Buffer[64 - ext.GetLength() - 5] = '\0';
                                    TruncSJIS ( Buffer );

                                    for ( i = 0; i < 10000; i++ )
                                        {
                                            fn.Format ( "%s_%04d%s", Buffer, i, ext );

                                            if ( SearchName ( fn ) == NULL )
                                                {
                                                    p->m_ImageFileName = fn;
                                                    break;
                                                }
                                        }
                                }
                        }

                    a.Format ( MSG ( 148 ), OldName );
                    b.Format ( MSG ( 149 ), p->m_ImageFileName );
                    cs.Format ( "%s\n%s\n%s", MSG ( 147 ), a, b );

                    if ( theSetting.m_Mastering_NotifyTruncated )
                        {
                            MessageBox ( NULL, cs, theSetting.m_Lang.m_Str[5], MB_OK | MB_TASKMODAL );
                        }
                }

            p = p->GetNext();
        }
}

//   TypeFlag = 0 : level 1
//   TypeFlag = 1 : Joliet ext.
DWORD CDirStructure::CalcPathTableSize ( int TypeFlag )
{
    DWORD Size, r;

    if ( TypeFlag == 0 )
        {
            Size = 8 + m_ImageFileNameShort.GetLength() + ( m_ImageFileNameShort.GetLength() % 2 );
        }

    else
        {
            CStringW csw;
            csw = m_ImageFileName;
            Size = 8 + csw.GetLength() * 2;
        }

    if ( m_DirList != NULL )
        {
            r = m_DirList->CalcPathTableSize ( TypeFlag );
            /*
                    if((r % 2048) + Size > 2048){
                        r = ((r+2047) / 2048) * 2048;
                    }
                    */
            Size += r;
        }

    if ( m_Next != NULL )
        {
            r = m_Next->CalcPathTableSize ( TypeFlag );
            /*
                    if((r % 2048) + Size > 2048){
                        r = ((r+2047) / 2048) * 2048;
                    }
                    */
            Size += r;
        }

    return Size;
}
/*
DWORD CDirStructure::CalcRecordSize(int TypeFlag)
{
    DWORD Size;

    if(TypeFlag == 0){
        Size = 33 + m_ImageFileNameShort.GetLength() + 2 + ((m_ImageFileNameShort.GetLength()+3) % 2);
    } else {
        CStringW csw;
        csw = m_ImageFileName;
        Size = 34 + (csw.GetLength()+2) * 2;
    }

    return Size;
}

DWORD CDirStructure::CalcRecordTableSize(int TypeFlag)
{
    DWORD Size,r;
    CDirStructure *p;

    Size = 0x44;
    p = m_DirList;
    while(p != NULL){
        r = p->CalcRecordSize(TypeFlag);
        if(((Size % 2048) + r) > 2048){
            Size += 2048 - (Size % 2048);
        }
        Size += r;
        p = p->GetNext();
    }

    p = m_FileList;
    while(p != NULL){
        r = p->CalcRecordSize(TypeFlag);
        if(((Size % 2048) + r) > 2048){
            Size += 2048 - (Size % 2048);
        }
        Size += r;
        p = p->GetNext();
    }

    return Size;
}

DWORD CDirStructure::CalcRecordTableSectors(int TypeFlag)
{
    DWORD Sectors,RecordSize;

    RecordSize = CalcRecordTableSize(TypeFlag);
    Sectors = RecordSize / 2048;
    if(RecordSize % 2048){
        Sectors++;
    }

    if(m_Next != NULL){
        Sectors += m_Next->CalcRecordTableSectors(TypeFlag);
    }
    if(m_DirList != NULL){
        Sectors += m_DirList->CalcRecordTableSectors(TypeFlag);
    }

    return Sectors;
}
*/
void CDirStructure::Clear ( void )
{
    if ( m_DirList != NULL )
        {
            delete m_DirList;
        }

    if ( m_FileList != NULL )
        {
            delete m_FileList;
        }

    if ( m_Next != NULL )
        {
            delete m_Next;
        }

    m_DirList = NULL;
    m_FileList = NULL;
    m_DirListTail = NULL;
    m_FileListTail = NULL;
    m_Next = NULL;
    m_IsDirectory = false;
}

void CDirStructure::TruncSJIS ( LPSTR String )
{
    BYTE *p;
    bool TruncFlag;

    if ( *String == '\0' )
        {
            return;
        }

    p = ( BYTE* ) String;
    TruncFlag = false;

    while ( *p != '\0' )
        {
            if ( TruncFlag )
                {
                    TruncFlag = false;
                }

            else
                if ( ( *p >= 0x80 && *p <= 0x9f ) || ( *p > 0xe0 ) )
                    {
                        TruncFlag = true;
                    }

            p++;
        }

    if ( TruncFlag )
        {
            * ( p - 1 ) = '\0';
        }
}
