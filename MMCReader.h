#pragma once

#include "Aspi.h"
#include "CDType.h"

class CMMCReader {
    protected:
        CAspi *m_Aspi;
    public:
        virtual ~CMMCReader ( void );
        CMMCReader ( void );
        bool SetErrorCorrectMode ( bool CorrectFlag );
        //  bool ReadTOC(TableOfContents & Toc);
        bool ReadTOCFromSession ( TableOfContents & Toc );
        void Initialize ( CAspi * Aspi );
        bool ReadCD ( MSFAddress Address, LPSTR Buffer );
        bool ReadCD_LBA ( MSFAddress Address, LPSTR Buffer );
        bool ReadCDDA ( MSFAddress Address, LPSTR Buffer );
        bool ReadCDDA_LBA ( MSFAddress Address, LPSTR Buffer );
        bool ReadCDRaw ( MSFAddress Address, LPSTR Buffer );
        bool ReadCDRaw_LBA ( MSFAddress Address, LPSTR Buffer );
        bool ReadCD_Read10 ( MSFAddress Address, LPSTR Buffer );
        bool ReadCD_D8 ( MSFAddress Address, LPSTR Buffer );
        bool ReadCD_D4 ( MSFAddress Address, LPSTR Buffer );
        bool ReadCD_D4_2 ( MSFAddress Address, LPSTR Buffer );
        bool ReadCD_D5 ( MSFAddress Address, LPSTR Buffer );
        bool ReadATIP ( BYTE *Buffer );
        void SetCDSpeed ( BYTE ReadSpeed, BYTE WriteSpeed );
        BYTE m_SRB_Status;
        BYTE m_SK;
        BYTE m_ASC;
        BYTE m_ASCQ;
        bool ReadSubQ ( MSFAddress msf, BYTE * Buffer );
    protected:
        int m_ReadSubQMethod;
    public:
        int ReadCDText ( BYTE * Buffer );
};
