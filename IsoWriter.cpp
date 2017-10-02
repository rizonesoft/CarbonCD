#include "StdAfx.h"
#include "isowriter.h"

CIsoWriter::CIsoWriter ( void )
{
    m_Address = 150;
    m_Buffer = NULL;
    m_BufferTail = NULL;
}

CIsoWriter::~CIsoWriter ( void )
{
    Close();
}

void CIsoWriter::Close ( void )
{
    while ( m_Buffer != NULL )
        {
            m_BufferTail = m_Buffer;
            m_Buffer = m_Buffer->Next;
            delete m_BufferTail;
        }

    m_BufferTail = NULL;
}

void CIsoWriter::WriteHeader ( BYTE * Buffer )
{
    CreateNewBuffer();
    memcpy ( ( m_BufferTail->Data ) + 16, Buffer, 2048 );
    m_EDC.Mode1Raw ( ( m_BufferTail->Data ), m_Address.Minute, m_Address.Second, m_Address.Frame );
    m_Address = m_Address.GetByLBA() + 1;
}

DWORD CIsoWriter::GetLBA ( void )
{
    return m_Address.GetByLBA();
}

void CIsoWriter::CreateNewBuffer ( void )
{
    if ( m_Buffer == NULL || m_BufferTail == NULL )
        {
            m_Buffer = new HeaderBuffer;
            m_BufferTail = m_Buffer;
        }

    else
        {
            m_BufferTail->Next = new HeaderBuffer;
            m_BufferTail = m_BufferTail->Next;
        }

    m_BufferTail->Next = NULL;
}

bool CIsoWriter::GetHeaderFrame ( BYTE *Buffer )
{
    HeaderBuffer *p;

    if ( m_Buffer == NULL || m_BufferTail == NULL )
        {
            return false;
        }

    memcpy ( Buffer, m_Buffer->Data, 2352 );
    p = m_Buffer;
    m_Buffer = m_Buffer->Next;
    delete p;
    return true;
}

void CIsoWriter::GenerateData ( BYTE * Buffer, BYTE *Src )
{
    memcpy ( Buffer + 16, Src, 2048 );
    m_EDC.Mode1Raw ( Buffer, m_Address.Minute, m_Address.Second, m_Address.Frame );
    m_Address = m_Address.GetByLBA() + 1;
}

void CIsoWriter::GenerateRaw ( BYTE * Buffer, BYTE * Src )
{
    memcpy ( Buffer, Src, 2352 );
    m_Address = m_Address.GetByLBA() + 1;
}
