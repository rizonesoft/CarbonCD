// Hermit2.cpp: Defines the class behaviors for the application.

#include "stdafx.h"
#include "CDM.h"
#include "CDMDlg.h"
#include "Setting.h"
#include "ThemeController.h"
#ifdef _DEBUG
    #define new DEBUG_NEW
#endif


// CHermit2App

BEGIN_MESSAGE_MAP ( CCDMApp, CWinApp )
    ON_COMMAND ( ID_HELP, CWinApp::OnHelp )
END_MESSAGE_MAP()


// CHermit2App construction

CCDMApp::CCDMApp()
{
    // TODO: Please add the code for building in this position.
    // Please describe all the initialization process in the InitInstance important here.
}


// This is CHermit2App object.

CCDMApp theApp;
CSetting theSetting;
CThemeController theTheme;

// Initialize CHermit2App

BOOL CCDMApp::InitInstance()
{
    // For the application manifest to enable visual styles,
    // If you want to specify the use of version 6 or later ComCtl32.dll,
    // You must have () InitCommonControls to Windows XP. Otherwise, any window creation will fail.
    theSetting.Load();
    InitCommonControls();
    CWinApp::InitInstance();
    CCDMDlg dlg;
    m_pMainWnd = &dlg;
    INT_PTR nResponse = dlg.DoModal();

    if ( nResponse == IDOK )
        {
            // TODO: the code is erased when the dialog is <OK>
        }
    else
        if ( nResponse == IDCANCEL )
            {
                // TODO: the code when the dialog is dismissed with <Cancel>
            }

    // Dialog has been closed. Do not start the application's message pump
    // Please return FALSE to exit the application.
    return FALSE;
}
