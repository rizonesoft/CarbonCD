#pragma once

#include "CDType.h"
#include "Aspi.h"

class CMMCSubcodeReader {
    public:
        CMMCSubcodeReader ( void );
        ~CMMCSubcodeReader ( void );
    protected:
        CAspi *m_Aspi;
        bool m_TransBCD;
        WORD m_SubcodeCRCTable[256];
    public:
        void Initialize ( CAspi * Aspi );
        BYTE m_SK;
        BYTE m_ASC;
        BYTE m_ASCQ;
        bool ReadRaw16 ( MSFAddress Address, BYTE * Buffer );
        bool ReadCD16 ( MSFAddress Address, BYTE * Buffer );
        bool ReadCDDA16 ( MSFAddress Address, BYTE * Buffer );
        bool ReadRaw96 ( MSFAddress Address, BYTE * Buffer );
        bool ReadCD96 ( MSFAddress Address, BYTE * Buffer );
        bool ReadCDDA96 ( MSFAddress Address, BYTE * Buffer );
        void SetBCDMode ( bool TransBCD );
        void CalcCRC ( BYTE * SubQ );
};
