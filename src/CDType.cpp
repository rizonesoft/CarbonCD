#include "stdafx.h"
#include "CDType.h"

MSFAddress::MSFAddress(void)
{
    //  Hour = 0;
    Minute = 0;
    Second = 0;
    Frame = 0;
}

MSFAddress::~MSFAddress(void)
{
}

void MSFAddress::operator =(DWORD LBA)
{
    LBA;
    Frame = static_cast<BYTE>(LBA % 75);
    LBA /= 75;
    Second = static_cast<BYTE>(LBA % 60);
    LBA /= 60;
    Minute = static_cast<BYTE>(LBA);
}

MSFAddress MSFAddress::operator +(MSFAddress MSF)
{
    MSFAddress msf;
    DWORD LBA;
    LBA = GetByLBA() + MSF.GetByLBA();
    msf = LBA;
    return msf;
}

MSFAddress MSFAddress::operator -(MSFAddress MSF)
{
    MSFAddress msf;
    DWORD lba;
    lba = GetByLBA() - MSF.GetByLBA();
    msf = lba;
    return msf;
}

MSFAddress MSFAddress::operator -(DWORD LBA)
{
    MSFAddress msf;
    LBA = GetByLBA() - LBA;
    msf = LBA;
    return msf;
}

void MSFAddress::operator =(MSFAddress MSF)
{
    //  Hour = MSF.Hour;
    Minute = MSF.Minute;
    Second = MSF.Second;
    Frame = MSF.Frame;
}

DWORD MSFAddress::GetByLBA(void)
{
    return Frame + 75 * (Second + 60 * Minute);
}

bool MSFAddress::operator ==(MSFAddress comp)
{
    if (Minute == comp.Minute && Second == comp.Second && Frame == comp.Frame)
    {
        return true;
    }

    return false;
}
