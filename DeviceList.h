#pragma once
#include "Aspi.h"

// CDeviceList
class CDeviceList : public CComboBox {
        DECLARE_DYNAMIC ( CDeviceList )

    public:
        CDeviceList();
        virtual ~CDeviceList();

    protected:
        DECLARE_MESSAGE_MAP()
    public:
        void Initialize ( CAspi *Aspi );
    protected:
        CAspi *m_Aspi;
    public:
        //  afx_msg void OnCbnSelchange();
        void InitializeShortVer ( CAspi *Aspi );
};


