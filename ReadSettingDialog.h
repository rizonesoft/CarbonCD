#pragma once
#include "afxwin.h"

#include "CDController.h"
#include "afxcmn.h"
#include "devicelist.h"
#include "readsettingdialog1.h"
#include "readsettingdialog2.h"
#include "readsettingdialog3.h"
#include "readsettingdialog4.h"


class CReadSettingDialog : public CDialog {
        DECLARE_DYNAMIC ( CReadSettingDialog )

    public:
        CReadSettingDialog ( CWnd* pParent = NULL );
        virtual ~CReadSettingDialog();

        enum { IDD = IDD_READSETTING };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        afx_msg void OnBnClickedBrowsfile();
        CString m_ImageName;
        afx_msg void OnBnClickedOk();
        virtual BOOL OnInitDialog();
        afx_msg void OnBnClickedCancel();
        CCDController *m_CD;
        bool m_SettingAtCopy;
        void SetLanguage ( void );
        CDeviceList m_DriveList;
        afx_msg void OnCbnSelchangeDrivelist();
        CComboBox m_EngineList;
    protected:
        CReadSettingDialog1 m_Page1;
        CReadSettingDialog2 m_Page2;
        CReadSettingDialog3 m_Page3;
        CReadSettingDialog4 m_Page4;
        int m_CurrentEngine;
    public:
        afx_msg void OnCbnSelchangeEnginelist();
};
