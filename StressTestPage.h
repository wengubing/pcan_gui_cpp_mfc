#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////
// Second page of the main dialog: automatic send/receive stress test
//
// The page sends CAN frames automatically (id is increased with every frame, the payload
// contains a microsecond timestamp of the moment of transmission), waits for the frames to be
// echoed by the device under test and measures the round trip delay.
//
// The measurement itself lives in CStressTestCore, this class only owns the controls, the
// sending thread, the receiving thread and the periodic UI refresh.
//
#include "resource.h"
#include "StressTestCore.h"
#include "PCANBasic.h"
#include "PCANBasicClass.h"

#ifndef LANG_ZH
#define LANG_ZH 0
#define LANG_EN 1
#endif

// Language currently selected in the application (owned by PCANBasicExampleDlg.cpp)
//
extern int g_nLanguage;

// The dialog that owns this page. Only the pointer is needed here, the header
// of the dialog cannot be included (it includes this file).
//
class CPCANBasicExampleDlg;

class CStressTestPage : public CDialog
{
public:
    CStressTestPage(CWnd* pParent = NULL);

    enum { IDD = IDD_PAGE_STRESS };

    // Called by the main dialog every time the connection state changes
    //
    void SetChannel(PCANBasicClass* pcan, TPCANHandle handle, bool isFD, bool connected);
    // Translates the whole page
    //
    void ApplyLanguage();
    // Stops the sending, safe to call at any time. Frames that are still in
    // flight are waited for, so the reception can outlive the test.
    //
    void StopTest();
    // Stops the receive thread and gives the receive queue back to the main page.
    // "restoreReading" must be false when the channel itself is gone.
    //
    void StopReception(bool restoreReading = true);
    // Tells the page that the user switched to (or away from) the stress test tab
    //
    void NotifyPageActive(bool active);
    bool IsRunning();

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy();

    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBnClickedStart();
    afx_msg void OnBnClickedPause();
    afx_msg void OnBnClickedStop();
    afx_msg void OnBnClickedSendOnce();
    afx_msg void OnBnClickedResetStats();
    afx_msg void OnBnClickedExport();

    DECLARE_MESSAGE_MAP()

private:
    // Thread entry points
    //
    static DWORD WINAPI SendThreadStub(LPVOID param);
    static DWORD WINAPI RecvThreadStub(LPVOID param);
    void SendThreadFunc();
    void RecvThreadFunc();

    // Sends a single frame, returns false when the API reported an error
    //
    bool SendOneFrame();
    void ProcessReceivedFrame(DWORD id, const BYTE* data, int length, ULONGLONG rxUs, bool isEcho);

    // Takes the receive queue away from the main page and starts the receive
    // thread. Nothing happens when the reception is already active.
    //
    void StartReception();
    // The main dialog, NULL as long as the page has no window
    //
    CPCANBasicExampleDlg* GetMainDialog();

    // Reads the settings out of the controls
    //
    void ReadConfiguration();
    void UpdateStatistics();
    void UpdateFrameList();
    void UpdateControlStates();
    void SetStateText(LPCSTR text);
    void ExportCsv(LPCSTR fileName);

    // High resolution timestamp in microseconds
    //
    static ULONGLONG NowUs();
    static int LengthFromDlc(int dlc);

private:
    // Hardware access (owned by the main dialog)
    //
    PCANBasicClass* m_Pcan;
    TPCANHandle m_Handle;
    bool m_IsFD;
    bool m_Connected;

    // Measurement
    //
    CStressTestCore m_Core;

    // Configuration
    //
    DWORD m_IdStart;
    DWORD m_IdStep;
    DWORD m_IntervalMs;
    DWORD m_FrameCount;
    DWORD m_TimeoutMs;
    int m_Dlc;
    bool m_Extended;
    int m_PayloadMode;      // 0 = timestamp, 1 = counter, 2 = fixed value
    BYTE m_FixedValue;
    bool m_LogDetails;
    bool m_CheckEcho;

    DWORD m_NextId;
    DWORD m_Counter;
    CRITICAL_SECTION m_csIds;

    // Thread handling. The sending and the receiving have their own stop
    // events, so that the reception can outlive a test: otherwise the answers
    // of the last frames would be read by the main page.
    //
    bool m_Running;
    bool m_Paused;
    HANDLE m_hSendStopEvent;
    HANDLE m_hRecvStopEvent;
    HANDLE m_hPauseEvent;
    HANDLE m_hResumeEvent;
    HANDLE m_hSendThread;
    HANDLE m_hRecvThread;

    // The receive thread is running and this page owns the receive queue
    //
    bool m_Receiving;
    // Latest moment until which the reception has to stay alive after a test
    // or a single shot, so that the answers are not taken by the main page
    //
    ULONGLONG m_ReceptionDeadlineUs;

    ULONGLONG m_StartUs;

    // Highest sequence number that was already added to the frame list,
    // -1 means that nothing was shown yet
    //
    LONG m_LastShownSequence;
    int m_State;            // 0 idle, 1 running, 2 paused, 3 finished

    CListCtrl m_ListFrames;
};
