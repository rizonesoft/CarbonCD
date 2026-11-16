#include "StdAfx.h"
#include "Resource.h"
#include "readthread.h"
#include "ReadProgressDialog.h"
#include "Setting.h"

#include "CheckSector.h"
#define PGB(a) ((BYTE *)(((DWORD)(a) + 0x0f) & ~0x0f))

DWORD CReadThread::ReadDiscAlpha(void)
{
    return 0;
}

int CReadThread::ReadAlphaSession(DWORD lbaStart, DWORD lbaEnd, BYTE& LeadInMode, int ReadingMethod)
{
    return 0;
}
