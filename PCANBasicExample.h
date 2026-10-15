// PCANBasicExample.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
    #error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols


// PCANBasicExampleApp:
// See PCANBasicExample.cpp for the implementation of this class
//

class CPCANBasicExampleApp : public CWinAppEx
{
public:
    CPCANBasicExampleApp();

// Overrides
    public:
    virtual BOOL InitInstance();
    virtual int ExitInstance();

// Implementation

    // Headless (no GUI) self-test, started with the "--selftest" command line switch.
    // Optional switches: "--handle=<hex>" (force a PCAN channel), "--fd" (use CAN-FD),
    // "--wait=<ms>" (read window, default 1000).
    //
    int RunSelfTest();

    bool m_SelfTestMode;    // true when running without GUI
    int m_SelfTestResult;   // process exit code of the self-test

    DECLARE_MESSAGE_MAP()
};

extern CPCANBasicExampleApp theApp;