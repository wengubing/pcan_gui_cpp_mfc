// PCANBasicExample.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "PCANBasicExample.h"
#include "PCANBasicExampleDlg.h"
#include "PCANBasicClass.h"
#include "StressTestCore.h"

#include <stdio.h>
#include <stdarg.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
// Headless self-test support
//
// The sample is a Windows (GUI) subsystem application, so it has no console by default.
// When started with "--selftest" the console of the calling process is attached (or a new
// one is created) and every result is printed there AND written to "selftest.log".
//
#pragma region Headless self-test
static FILE* g_pSelfTestLog = NULL;

// Prints to console and to the log file
//
static void SelfTestPrint(const char* format, ...)
{
    char buffer[1024];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    printf("%s\n", buffer);
    fflush(stdout);

    if (g_pSelfTestLog != NULL)
    {
        fprintf(g_pSelfTestLog, "%s\n", buffer);
        fflush(g_pSelfTestLog);
    }
}

// Makes stdout/stderr usable from a GUI subsystem process
//
static void SelfTestInitConsole()
{
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole())
    {
        FILE* fp = NULL;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
    }
}

// Reads "--name=value" or "--name value" from the command line
//
static CString SelfTestOption(const CString& commandLine, const char* name)
{
    int pos = commandLine.Find(name);
    if (pos < 0)
        return "";

    int start = pos + (int)strlen(name);
    if (start < commandLine.GetLength() && commandLine[start] == '=')
        start++;

    int end = start;
    while (end < commandLine.GetLength() && commandLine[end] != ' ')
        end++;

    return commandLine.Mid(start, end - start);
}

// Translates a TPCANStatus into text using the API itself
//
static CString SelfTestStatusText(PCANBasicClass* pcan, TPCANStatus status)
{
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));

    if (pcan->GetErrorText(status, 0, buffer) != PCAN_ERROR_OK)
    {
        CString result;
        result.Format("unknown error (%Xh)", status);
        return result;
    }

    CString result;
    result.Format("%s (%Xh)", buffer, status);
    return result;
}

// Checks the measurement engine of the stress test page with synthetic frames.
// It needs no hardware, so it can be used at any time:
//      PCANBasicExample.exe --selftest --stresssim
//
// 1000 frames are "sent", 999 come back with a delay between 100 us and 1100 us,
// one frame is never answered, one is answered twice, one is returned with a
// modified payload and two frames swap their order.
//
static int SelfTestStressEngine()
{
    const int frames = 1000;
    const ULONGLONG base = 1000000;     // 1 s
    const ULONGLONG spacing = 1000;     // one frame per millisecond
    const int lostIndex = 500;
    const int dupIndex = 700;
    const int badIndex = 800;
    const int swapIndex = 900;

    CStressTestCore core;
    core.Reset();
    core.SetTimeoutUs(1000000);
    core.SetCheckEcho(true);

    SelfTestPrint("=== stress test engine simulation ===");
    SelfTestPrint("frames        : %d (1 lost, 1 duplicate, 1 data error, 2 swapped)", frames);

    for (int i = 0; i < frames; i++)
    {
        DWORD id = (DWORD)i;
        ULONGLONG tx = base + (ULONGLONG)i * spacing;
        BYTE data[8];
        memcpy(data, &tx, sizeof(tx));
        core.OnFrameSent(id, tx, data, 8);
    }

    for (int i = 0; i < frames; i++)
    {
        if (i == lostIndex)
            continue;

        int order = i;
        if (i == swapIndex)
            order = swapIndex + 1;
        else if (i == swapIndex + 1)
            order = swapIndex;

        DWORD id = (DWORD)order;
        ULONGLONG tx = base + (ULONGLONG)order * spacing;
        ULONGLONG latency = 100 + (ULONGLONG)(order % 11) * 100;
        ULONGLONG rx = tx + latency;

        BYTE data[8];
        memcpy(data, &tx, sizeof(tx));
        if (order == badIndex)
            data[0] ^= 0xFF;

        core.OnFrameReceived(id, data, 8, rx, false);

        if (order == dupIndex)
            core.OnFrameReceived(id, data, 8, rx + 5, false);
    }

    core.SweepTimeouts(base + (ULONGLONG)frames * spacing + 2000000);

    StressStats stats;
    core.GetStats(stats);

    SelfTestPrint("sent          : %I64u (expected 1000)", stats.sent);
    SelfTestPrint("received      : %I64u (expected 999)", stats.received);
    SelfTestPrint("lost          : %I64u (expected 1)", stats.lost);
    SelfTestPrint("duplicates    : %I64u (expected 1)", stats.duplicates);
    SelfTestPrint("data errors   : %I64u (expected 1)", stats.dataErrors);
    SelfTestPrint("out of order  : %I64u (expected 1)", stats.outOfOrder);
    SelfTestPrint("in flight     : %u (expected 0)", stats.pending);
    SelfTestPrint("min latency   : %.1f us (expected 100)", stats.minLatencyUs);
    SelfTestPrint("max latency   : %.1f us (expected 1100)", stats.maxLatencyUs);
    SelfTestPrint("avg latency   : %.1f us (expected 600)", stats.averageUs);
    SelfTestPrint("jitter        : %.1f us", stats.jitterUs);

    bool ok = true;
    ok = ok && (stats.sent == 1000);
    ok = ok && (stats.received == 999);
    ok = ok && (stats.lost == 1);
    ok = ok && (stats.duplicates == 1);
    ok = ok && (stats.dataErrors == 1);
    ok = ok && (stats.outOfOrder == 1);
    ok = ok && (stats.pending == 0);
    ok = ok && (stats.minLatencyUs > 99.0) && (stats.minLatencyUs < 101.0);
    ok = ok && (stats.maxLatencyUs > 1099.0) && (stats.maxLatencyUs < 1101.0);
    ok = ok && (stats.averageUs > 560.0) && (stats.averageUs < 640.0);

    SelfTestPrint("=== RESULT: %s (exit code %d) ===", ok ? "PASS" : "FAIL", ok ? 0 : 6);
    return ok ? 0 : 6;
}

// Executes the whole test sequence without any user interaction
//
int CPCANBasicExampleApp::RunSelfTest()
{
    SelfTestInitConsole();
    fopen_s(&g_pSelfTestLog, "selftest.log", "w");

    CString commandLine(GetCommandLineA());
    CString handleText = SelfTestOption(commandLine, "--handle");
    CString waitText = SelfTestOption(commandLine, "--wait");
    bool forceFD = commandLine.Find("--fd") >= 0;
    DWORD readWindow = (waitText.IsEmpty()) ? 1000 : (DWORD)atoi((LPCSTR)waitText);

    SelfTestPrint("=== PCAN-Basic headless self-test ===");
    SelfTestPrint("command line : %s", (LPCSTR)commandLine);
    SelfTestPrint("read window  : %u ms", readWindow);

    // "--stresssim" verifies the stress test engine, it needs no hardware
    //
    if (commandLine.Find("--stresssim") >= 0)
    {
        int simResult = SelfTestStressEngine();
        if (g_pSelfTestLog != NULL)
        {
            fclose(g_pSelfTestLog);
            g_pSelfTestLog = NULL;
        }
        return simResult;
    }

    // 1) DLL / API
    //
    PCANBasicClass pcan(true);
    if (!pcan.IsLoaded())
    {
        SelfTestPrint("[1] DLL/API    : FAILED - PCANBasic.dll could not be loaded (missing or wrong version)");
        SelfTestPrint("=== RESULT: FAIL (exit code 2) ===");
        if (g_pSelfTestLog != NULL) fclose(g_pSelfTestLog);
        return 2;
    }

    char version[256];
    memset(version, 0, sizeof(version));
    TPCANStatus sts = pcan.GetValue(PCAN_NONEBUS, PCAN_API_VERSION, version, sizeof(version));
    SelfTestPrint("[1] DLL/API    : loaded - API version = %s",
        (sts == PCAN_ERROR_OK) ? version : "(query failed)");

    // 2) Attached channels
    //
    DWORD channelCount = 0;
    sts = pcan.GetValue(PCAN_NONEBUS, PCAN_ATTACHED_CHANNELS_COUNT, &channelCount, sizeof(channelCount));
    if (sts != PCAN_ERROR_OK)
    {
        SelfTestPrint("[2] Channels   : FAILED - %s", (LPCSTR)SelfTestStatusText(&pcan, sts));
        SelfTestPrint("=== RESULT: FAIL (exit code 3) ===");
        if (g_pSelfTestLog != NULL) fclose(g_pSelfTestLog);
        return 3;
    }

    TPCANChannelInformation* channels = new TPCANChannelInformation[channelCount ? channelCount : 1];
    memset(channels, 0, (channelCount ? channelCount : 1) * sizeof(TPCANChannelInformation));
    if (channelCount > 0)
    {
        sts = pcan.GetValue(PCAN_NONEBUS, PCAN_ATTACHED_CHANNELS, channels,
            channelCount * sizeof(TPCANChannelInformation));
        if (sts != PCAN_ERROR_OK)
            SelfTestPrint("[2] Channels   : count = %u, enumeration failed - %s",
                channelCount, (LPCSTR)SelfTestStatusText(&pcan, sts));
    }

    SelfTestPrint("[2] Channels   : %u attached", channelCount);
    TPCANHandle selected = PCAN_NONEBUS;
    bool selectedIsFD = false;
    for (DWORD i = 0; i < channelCount; i++)
    {
        bool available = (channels[i].channel_condition & PCAN_CHANNEL_AVAILABLE) != 0;
        bool fdCapable = (channels[i].device_features & FEATURE_FD_CAPABLE) != 0;
        SelfTestPrint("      [%u] %-25s handle=%02Xh  condition=%u  %s%s",
            i, channels[i].device_name, channels[i].channel_handle, channels[i].channel_condition,
            available ? "available" : "not available", fdCapable ? ", FD capable" : "");

        if (available && selected == PCAN_NONEBUS)
        {
            selected = channels[i].channel_handle;
            selectedIsFD = fdCapable;
        }
    }

    // 3) Channel selection
    //
    if (!handleText.IsEmpty())
    {
        selected = (TPCANHandle)strtoul((LPCSTR)handleText, NULL, 16);
        selectedIsFD = forceFD;
        SelfTestPrint("[3] Selection  : forced handle %02Xh (%s)", selected, forceFD ? "FD" : "classic");
    }
    else if (selected == PCAN_NONEBUS)
    {
        SelfTestPrint("[3] Selection  : FAILED - no available PCAN channel found");
        SelfTestPrint("=== RESULT: FAIL (exit code 3) ===");
        delete[] channels;
        if (g_pSelfTestLog != NULL) fclose(g_pSelfTestLog);
        return 3;
    }
    else
        SelfTestPrint("[3] Selection  : handle %02Xh (%s)", selected, selectedIsFD ? "FD" : "classic");

    memset(version, 0, sizeof(version));
    if (pcan.GetValue(selected, PCAN_CHANNEL_VERSION, version, sizeof(version)) == PCAN_ERROR_OK)
    {
        CString driver(version);
        driver.Replace("\n", " | ");
        SelfTestPrint("[3] Driver     : %s", (LPCSTR)driver);
    }

    memset(version, 0, sizeof(version));
    if (pcan.GetValue(selected, PCAN_HARDWARE_NAME, version, sizeof(version)) == PCAN_ERROR_OK)
        SelfTestPrint("[3] Hardware   : %s", version);

    DWORD condition = 0;
    if (pcan.GetValue(selected, PCAN_CHANNEL_CONDITION, &condition, sizeof(condition)) == PCAN_ERROR_OK)
        SelfTestPrint("[3] Condition  : %u (%s)", condition,
            (condition == PCAN_CHANNEL_AVAILABLE) ? "available"
            : (condition == PCAN_CHANNEL_OCCUPIED) ? "occupied"
            : (condition == PCAN_CHANNEL_UNAVAILABLE) ? "unavailable" : "mixed");

    // 4) Initialize
    //
    char bitrateFD[256] = "f_clock_mhz=20, nom_brp=5, nom_tseg1=2, nom_tseg2=1, nom_sjw=1, "
                          "data_brp=2, data_tseg1=3, data_tseg2=1, data_sjw=1";
    sts = selectedIsFD ? pcan.InitializeFD(selected, bitrateFD)
                       : pcan.Initialize(selected, PCAN_BAUD_500K);

    // A channel left over by a previously crashed/killed process keeps the driver in a
    // wrong state (PCAN_ERROR_ILLMODE). Uninitializing it once clears that state.
    //
    for (int attempt = 0; sts == PCAN_ERROR_ILLMODE && attempt < 3; attempt++)
    {
        SelfTestPrint("[4] Initialize : %s - resetting the channel (attempt %d)",
            (LPCSTR)SelfTestStatusText(&pcan, sts), attempt + 1);
        pcan.Uninitialize(selected);
        pcan.Uninitialize(PCAN_NONEBUS);
        Sleep(500);
        sts = selectedIsFD ? pcan.InitializeFD(selected, bitrateFD)
                           : pcan.Initialize(selected, PCAN_BAUD_500K);
    }
    if (sts != PCAN_ERROR_OK && sts != PCAN_ERROR_CAUTION)
    {
        SelfTestPrint("[4] Initialize : FAILED - %s", (LPCSTR)SelfTestStatusText(&pcan, sts));
        SelfTestPrint("=== RESULT: FAIL (exit code 4) ===");
        delete[] channels;
        if (g_pSelfTestLog != NULL) fclose(g_pSelfTestLog);
        return 4;
    }
    SelfTestPrint("[4] Initialize : OK (%s)%s", selectedIsFD ? bitrateFD : "500 kbit/s",
        (sts == PCAN_ERROR_CAUTION) ? " [CAUTION: bitrate differs]" : "");

    // 5) Open the message filter
    //
    int filter = PCAN_FILTER_OPEN;
    sts = pcan.SetValue(selected, PCAN_MESSAGE_FILTER, &filter, sizeof(filter));
    SelfTestPrint("[5] Filter     : %s", (sts == PCAN_ERROR_OK) ? "opened"
        : (LPCSTR)SelfTestStatusText(&pcan, sts));

    // 6) Write one test frame
    //
    if (selectedIsFD)
    {
        TPCANMsgFD msg;
        memset(&msg, 0, sizeof(msg));
        msg.ID = 0x100;
        msg.DLC = 8;
        msg.MSGTYPE = PCAN_MESSAGE_STANDARD | PCAN_MESSAGE_FD;
        for (int i = 0; i < 8; i++)
            msg.DATA[i] = (BYTE)(i + 1);
        sts = pcan.WriteFD(selected, &msg);
    }
    else
    {
        TPCANMsg msg;
        memset(&msg, 0, sizeof(msg));
        msg.ID = 0x100;
        msg.LEN = 8;
        msg.MSGTYPE = PCAN_MESSAGE_STANDARD;
        for (int i = 0; i < 8; i++)
            msg.DATA[i] = (BYTE)(i + 1);
        sts = pcan.Write(selected, &msg);
    }
    SelfTestPrint("[6] Write      : ID=100h LEN=8 DATA=01 02 03 04 05 06 07 08 -> %s",
        (sts == PCAN_ERROR_OK) ? "OK" : (LPCSTR)SelfTestStatusText(&pcan, sts));

    // 7) Read everything that arrives inside the read window
    //
    DWORD startTick = GetTickCount();
    int received = 0;
    while (GetTickCount() - startTick < readWindow)
    {
        TPCANMsgFD msg;
        TPCANTimestampFD timestamp = 0;
        memset(&msg, 0, sizeof(msg));

        TPCANStatus readSts;
        if (selectedIsFD)
            readSts = pcan.ReadFD(selected, &msg, &timestamp);
        else
        {
            TPCANMsg msgStd;
            TPCANTimestamp tsStd;
            memset(&msgStd, 0, sizeof(msgStd));
            memset(&tsStd, 0, sizeof(tsStd));
            readSts = pcan.Read(selected, &msgStd, &tsStd);
            msg.ID = msgStd.ID;
            msg.DLC = msgStd.LEN;
            msg.MSGTYPE = msgStd.MSGTYPE;
            for (int i = 0; i < 8 && i < (int)msgStd.LEN; i++)
                msg.DATA[i] = msgStd.DATA[i];
            timestamp = tsStd.micros + 1000 * tsStd.millis
                + 0x100000000ULL * 1000 * tsStd.millis_overflow;
        }

        if (readSts == PCAN_ERROR_OK)
        {
            received++;
            CString data;
            CString byteText;
            for (int i = 0; i < (int)msg.DLC && i < 64; i++)
            {
                byteText.Format(" %02X", msg.DATA[i]);
                data += byteText;
            }
            SelfTestPrint("      frame %02d : ID=%03Xh type=%02Xh DLC=%d ts=%.3f s |%s",
                received, msg.ID, msg.MSGTYPE, msg.DLC, timestamp / 1000000.0, (LPCSTR)data);
            continue;
        }

        if (readSts == PCAN_ERROR_QRCVEMPTY)
        {
            Sleep(10);
            continue;
        }

        SelfTestPrint("      read stopped: %s", (LPCSTR)SelfTestStatusText(&pcan, readSts));
        break;
    }
    SelfTestPrint("[7] Read       : %d frame(s) received in %u ms", received, readWindow);

    // 8) Bus status and cleanup
    //
    sts = pcan.GetStatus(selected);
    SelfTestPrint("[8] Status     : %s", (LPCSTR)SelfTestStatusText(&pcan, sts));

    sts = pcan.Uninitialize(selected);
    bool cleaned = (sts == PCAN_ERROR_OK);
    SelfTestPrint("[9] Release    : %s", cleaned ? "OK" : (LPCSTR)SelfTestStatusText(&pcan, sts));

    delete[] channels;
    if (g_pSelfTestLog != NULL)
    {
        fclose(g_pSelfTestLog);
        g_pSelfTestLog = NULL;
    }

    int result = cleaned ? 0 : 5;
    SelfTestPrint("=== RESULT: %s (exit code %d) ===", (result == 0) ? "PASS" : "FAIL", result);
    return result;
}
#pragma endregion


// PCANBasicExampleApp

BEGIN_MESSAGE_MAP(CPCANBasicExampleApp, CWinAppEx)
    ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// PCANBasicExampleApp construction

CPCANBasicExampleApp::CPCANBasicExampleApp()
{
    // TODO: add construction code here,
    // Place all significant initialization in InitInstance
    m_SelfTestMode = false;
    m_SelfTestResult = 0;
}


// The one and only PCANBasicExampleApp object

CPCANBasicExampleApp theApp;


// PCANBasicExampleApp initialization

BOOL CPCANBasicExampleApp::InitInstance()
{
    // Headless mode: "--selftest" runs the PCAN-Basic smoke test without any GUI
    //
    CString startCommandLine(GetCommandLineA());
    if (startCommandLine.Find("--selftest") >= 0 || startCommandLine.Find("--stresssim") >= 0)
    {
        m_SelfTestMode = true;
        m_SelfTestResult = RunSelfTest();
        return FALSE;
    }

    // InitCommonControlsEx() is required on Windows XP if an application
    // manifest specifies use of ComCtl32.dll version 6 or later to enable
    // visual styles.  Otherwise, any window creation will fail.
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    // Set this to include all the common control classes you want to use
    // in your application.
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinAppEx::InitInstance();

    // Standard initialization
    // If you are not using these features and wish to reduce the size
    // of your final executable, you should remove from the following
    // the specific initialization routines you do not need
    // Change the registry key under which our settings are stored
    // TODO: You should modify this string to be something appropriate
    // such as the name of your company or organization
    SetRegistryKey(_T("Local AppWizard-Generated Applications"));

    CPCANBasicExampleDlg dlg;
    m_pMainWnd = &dlg;
    INT_PTR nResponse = dlg.DoModal();
    if (nResponse == IDOK)
    {
        // TODO: Place code here to handle when the dialog is
        //  dismissed with OK
    }
    else if (nResponse == IDCANCEL)
    {
        // TODO: Place code here to handle when the dialog is
        //  dismissed with Cancel
    }

    // Since the dialog has been closed, return FALSE so that we exit the
    //  application, rather than start the application's message pump.
    return FALSE;
}

// Propagates the self-test result as process exit code
//
int CPCANBasicExampleApp::ExitInstance()
{
    if (m_SelfTestMode)
        return m_SelfTestResult;

    return CWinAppEx::ExitInstance();
}
