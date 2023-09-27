#pragma once


// CButtonEx
class CButtonEx : public CButton
{
    DECLARE_DYNAMIC(CButtonEx)

public:
    CButtonEx();
    ~CButtonEx() override;

protected:
    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
};
