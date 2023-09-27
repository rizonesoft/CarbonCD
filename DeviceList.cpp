#include "stdafx.h"
#include "CDM.h"
#include "DeviceList.h"


// CDeviceList

IMPLEMENT_DYNAMIC(CDeviceList, CComboBox)

CDeviceList::CDeviceList()
{
    m_Aspi = nullptr;
}

CDeviceList::~CDeviceList()
{
}


BEGIN_MESSAGE_MAP(CDeviceList, CComboBox)
    //  ON_CONTROL_REFLECT(CBN_SELCHANGE, OnCbnSelchange)
END_MESSAGE_MAP()


void CDeviceList::Initialize(CAspi* Aspi)
{
    while (GetCount()) { this->DeleteString(0); }

    m_Aspi = Aspi;
    int i, cur;
    cur = m_Aspi->GetCurrentDevice();

    for (i = 0; i < m_Aspi->GetDeviceCount(); i++)
    {
        CString Vendor, Product, Revision, Address, cs;
        m_Aspi->SetDevice(i);
        m_Aspi->GetDeviceString(Vendor, Product, Revision, Address);
        cs.Format("(%s) %s %s %s", Address, Vendor, Product, Revision);
        InsertString(i, cs);
    }

    m_Aspi->SetDevice(cur);
    SetCurSel(0);
}

void CDeviceList::InitializeShortVer(CAspi* Aspi)
{
    m_Aspi = Aspi;
    int i, cur;
    cur = m_Aspi->GetCurrentDevice();

    for (i = 0; i < m_Aspi->GetDeviceCount(); i++)
    {
        CString Vendor, Product, Revision, Address, cs;
        m_Aspi->SetDevice(i);
        m_Aspi->GetDeviceString(Vendor, Product, Revision, Address);
        cs.Format("%s", Product);
        InsertString(i, cs);
    }

    m_Aspi->SetDevice(cur);
    SetCurSel(0);
}
