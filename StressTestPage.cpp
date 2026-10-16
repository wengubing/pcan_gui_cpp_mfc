#include "stdafx.h"
#include "StressTestPage.h"
#include "PCANBasicExampleDlg.h"
#include "PCANBasicExample.h"

#include <afxdlgs.h>
#include <mmsystem.h>
#include <stdio.h>
#include <math.h>

#pragma comment(lib, "winmm.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
// Texts of the page: control id, Chinese text, English text
//
struct STRESS_LANGUAGE_TEXT
{
    UINT nID;
    LPCSTR pszChinese;
    LPCSTR pszEnglish;
};

static LPCSTR SLT(LPCSTR chinese, LPCSTR english)
{
    return (g_nLanguage == LANG_ZH) ? chinese : english;
}

static STRESS_LANGUAGE_TEXT g_StressTexts[] =
{
    { IDC_GB_SENDSET,       "发送设置",           "Send Settings" },
    { IDC_LA_IDSTART,       "起始 ID (Hex):",     "Start ID (Hex):" },
    { IDC_LA_IDSTEP,        "ID 步长:",           "ID step:" },
    { IDC_RADIO_STD,        "标准帧 (11 位)",     "Standard (11 bit)" },
    { IDC_RADIO_EXT,        "扩展帧 (29 位)",     "Extended (29 bit)" },
    { IDC_LA_STRESSLENGTH,  "数据长度:",          "Length:" },
    { IDC_LA_PAYLOAD,       "数据内容:",          "Payload:" },
    { IDC_RADIO_PAY_TS,     "时间戳 (us)",        "Timestamp (us)" },
    { IDC_RADIO_PAY_INC,    "递增计数",           "Counter" },
    { IDC_RADIO_PAY_FIX,    "固定值",             "Fixed" },
    { IDC_LA_INTERVAL,      "发送间隔 (ms):",     "Interval (ms):" },
    { IDC_LA_COUNT,         "发送帧数 (0=连续):", "Frames (0=endless):" },
    { IDC_LA_TIMEOUT,       "超时 (ms):",         "Timeout (ms):" },
    { IDC_CHK_ECHOCHECK,    "校验回传数据",       "Verify echoed data" },
    { IDC_CHK_LOGDETAIL,    "记录帧明细",         "Log frame details" },
    { IDC_BTN_START,        "开始",               "Start" },
    { IDC_BTN_PAUSE,        "暂停",               "Pause" },
    { IDC_BTN_STOP,         "停止",               "Stop" },
    { IDC_BTN_SENDONCE,     "手动发送一帧",       "Send one" },
    { IDC_BTN_RESETSTATS,   "重置统计",           "Reset" },
    { IDC_BTN_EXPORT,       "导出 CSV",           "Export CSV" },
    { IDC_LA_STATE,         "状态:",              "State:" },
    { IDC_GB_STATS,         "统计",               "Statistics" },
    { IDC_LA_TX,            "已发送:",            "Sent:" },
    { IDC_LA_RX,            "已接收:",            "Received:" },
    { IDC_LA_LOST,          "未返回:",            "Not returned:" },
    { IDC_LA_TIMEOUTC,      "超时:",              "Timeout:" },
    { IDC_LA_OOO,           "乱序:",              "Out of order:" },
    { IDC_LA_DUP,           "重复/未知:",         "Unknown/dup.:" },
    { IDC_LA_DATAERR,       "数据错误:",          "Data error:" },
    { IDC_LA_SENDERR,       "发送错误:",          "Send error:" },
    { IDC_LA_ECHO,          "自发自收:",          "Self received:" },
    { IDC_LA_MIN,           "最短延时:",          "Min latency:" },
    { IDC_LA_MAX,           "最长延时:",          "Max latency:" },
    { IDC_LA_AVG,           "平均延时:",          "Avg latency:" },
    { IDC_LA_LAST,          "最近延时:",          "Last latency:" },
    { IDC_LA_JITTER,        "抖动 (标准差):",     "Jitter (sigma):" },
    { IDC_LA_RATE,          "速率 (帧/秒):",      "Rate (fps):" },
    { IDC_LA_ELAPSED,       "运行时长:",          "Elapsed:" },
    { IDC_LA_PENDING,       "在途帧数:",          "In flight:" },
    { IDC_GB_FRAMES,        "帧明细",             "Frame Details" },
};

////////////////////////////////////////////////////////////////////////////////////////////////
// CStressTestPage
//
CStressTestPage::CStressTestPage(CWnd* pParent)
    : CDialog(CStressTestPage::IDD, pParent)
{
    m_Pcan = NULL;
    m_Handle = PCAN_NONEBUS;
    m_IsFD = false;
    m_Connected = false;

    m_IdStart = 0;
    m_IdStep = 1;
    m_IntervalMs = 10;
    m_FrameCount = 0;
    m_TimeoutMs = 1000;
    m_Dlc = 8;
    m_Extended = false;
    m_PayloadMode = 0;
    m_FixedValue = 0xAA;
    m_LogDetails = true;
    m_CheckEcho = true;

    m_NextId = 0;
    m_Counter = 0;
    InitializeCriticalSection(&m_csIds);

    m_Running = false;
    m_Paused = false;
    m_hStopEvent = NULL;
    m_hPauseEvent = NULL;
    m_hResumeEvent = NULL;
    m_hSendThread = NULL;
    m_hRecvThread = NULL;

    m_StartUs = 0;
    m_LastShownSequence = 0;
    m_State = 0;
}

void CStressTestPage::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

void CStressTestPage::PostNcDestroy()
{
    // The page is a member of the main dialog, it must never delete itself
}

BEGIN_MESSAGE_MAP(CStressTestPage, CDialog)
    ON_WM_TIMER()
    ON_BN_CLICKED(IDC_BTN_START, &CStressTestPage::OnBnClickedStart)
    ON_BN_CLICKED(IDC_BTN_PAUSE, &CStressTestPage::OnBnClickedPause)
    ON_BN_CLICKED(IDC_BTN_STOP, &CStressTestPage::OnBnClickedStop)
    ON_BN_CLICKED(IDC_BTN_SENDONCE, &CStressTestPage::OnBnClickedSendOnce)
    ON_BN_CLICKED(IDC_BTN_RESETSTATS, &CStressTestPage::OnBnClickedResetStats)
    ON_BN_CLICKED(IDC_BTN_EXPORT, &CStressTestPage::OnBnClickedExport)
END_MESSAGE_MAP()

BOOL CStressTestPage::OnInitDialog()
{
    CDialog::OnInitDialog();

    m_ListFrames.SubclassDlgItem(IDC_LIST_FRAMES, this);
    m_ListFrames.SetExtendedStyle(m_ListFrames.GetExtendedStyle() | LVS_EX_FULLROWSELECT);

    SetDlgItemText(IDC_EDIT_IDSTART, "0");
    SetDlgItemText(IDC_EDIT_IDSTEP, "1");
    SetDlgItemText(IDC_EDIT_DLC, "8");
    SetDlgItemText(IDC_EDIT_FIXVAL, "AA");
    SetDlgItemText(IDC_EDIT_INTERVAL, "10");
    SetDlgItemText(IDC_EDIT_COUNT, "0");
    SetDlgItemText(IDC_EDIT_TIMEOUT, "1000");

    ((CButton*)GetDlgItem(IDC_RADIO_STD))->SetCheck(1);
    ((CButton*)GetDlgItem(IDC_RADIO_PAY_TS))->SetCheck(1);
    ((CButton*)GetDlgItem(IDC_CHK_ECHOCHECK))->SetCheck(1);
    ((CButton*)GetDlgItem(IDC_CHK_LOGDETAIL))->SetCheck(1);

    CSpinButtonCtrl* pSpin = (CSpinButtonCtrl*)GetDlgItem(IDC_SPIN_DLC);
    if (pSpin != NULL)
    {
        pSpin->SetRange(0, 8);
        pSpin->SetPos(8);
    }

    ApplyLanguage();
    UpdateControlStates();

    return TRUE;
}

void CStressTestPage::ApplyLanguage()
{
    for (int i = 0; i < (int)(sizeof(g_StressTexts) / sizeof(g_StressTexts[0])); i++)
    {
        CWnd* pControl = GetDlgItem(g_StressTexts[i].nID);
        if (pControl != NULL)
            pControl->SetWindowText(SLT(g_StressTexts[i].pszChinese, g_StressTexts[i].pszEnglish));
    }

    // The start button shows "continue" while the test is paused
    //
    if (m_Running && m_Paused)
        SetDlgItemText(IDC_BTN_START, SLT("继续", "Resume"));

    // Columns of the frame list
    //
    if (::IsWindow(m_ListFrames.GetSafeHwnd()))
    {
        m_ListFrames.DeleteAllItems();
        while (m_ListFrames.DeleteColumn(0))
        {
        }
        m_ListFrames.InsertColumn(0, SLT("序号", "No."), LVCFMT_LEFT, 50);
        m_ListFrames.InsertColumn(1, SLT("ID", "ID"), LVCFMT_LEFT, 60);
        m_ListFrames.InsertColumn(2, SLT("发送 (us)", "Tx (us)"), LVCFMT_RIGHT, 90);
        m_ListFrames.InsertColumn(3, SLT("接收 (us)", "Rx (us)"), LVCFMT_RIGHT, 90);
        m_ListFrames.InsertColumn(4, SLT("延时 (us)", "Latency (us)"), LVCFMT_RIGHT, 85);
        m_ListFrames.InsertColumn(5, SLT("状态", "Status"), LVCFMT_LEFT, 100);
        m_LastShownSequence = 0;
    }

    UpdateControlStates();
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Hardware connection
//
void CStressTestPage::SetChannel(PCANBasicClass* pcan, TPCANHandle handle, bool isFD, bool connected)
{
    m_Pcan = pcan;
    m_Handle = handle;
    m_IsFD = isFD;

    if (m_Connected && !connected && m_Running)
        StopTest();

    m_Connected = connected;

    if (::IsWindow(GetSafeHwnd()))
        UpdateControlStates();
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
//
ULONGLONG CStressTestPage::NowUs()
{
    static ULONGLONG frequency = 0;
    LARGE_INTEGER counter;

    if (frequency == 0)
    {
        LARGE_INTEGER freq;
        if (QueryPerformanceFrequency(&freq))
            frequency = (ULONGLONG)freq.QuadPart;
        if (frequency == 0)
            frequency = 1000;
    }

    QueryPerformanceCounter(&counter);
    return (ULONGLONG)(counter.QuadPart * 1000000LL / (LONGLONG)frequency);
}

int CStressTestPage::LengthFromDlc(int dlc)
{
    static const int lengths[16] = { 0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64 };
    if (dlc < 0)
        return 0;
    if (dlc > 15)
        return 64;
    return lengths[dlc];
}

void CStressTestPage::ReadConfiguration()
{
    CString text;

    GetDlgItemText(IDC_EDIT_IDSTART, text);
    m_IdStart = (DWORD)strtoul(text, NULL, 16);
    m_IdStart = m_Extended ? (m_IdStart & 0x1FFFFFFF) : (m_IdStart & 0x7FF);

    GetDlgItemText(IDC_EDIT_IDSTEP, text);
    m_IdStep = (DWORD)strtoul(text, NULL, 16);
    if (m_IdStep == 0)
        m_IdStep = 1;

    GetDlgItemText(IDC_EDIT_INTERVAL, text);
    m_IntervalMs = (DWORD)strtoul(text, NULL, 10);

    GetDlgItemText(IDC_EDIT_COUNT, text);
    m_FrameCount = (DWORD)strtoul(text, NULL, 10);

    GetDlgItemText(IDC_EDIT_TIMEOUT, text);
    m_TimeoutMs = (DWORD)strtoul(text, NULL, 10);
    if (m_TimeoutMs == 0)
        m_TimeoutMs = 1000;

    GetDlgItemText(IDC_EDIT_DLC, text);
    m_Dlc = (int)strtoul(text, NULL, 10);
    if (m_Dlc < 0)
        m_Dlc = 0;
    if (m_Dlc > 8)
        m_Dlc = 8;

    GetDlgItemText(IDC_EDIT_FIXVAL, text);
    m_FixedValue = (BYTE)(strtoul(text, NULL, 16) & 0xFF);

    m_Extended = (((CButton*)GetDlgItem(IDC_RADIO_EXT))->GetCheck() == 1);
    m_LogDetails = (((CButton*)GetDlgItem(IDC_CHK_LOGDETAIL))->GetCheck() == 1);
    m_CheckEcho = (((CButton*)GetDlgItem(IDC_CHK_ECHOCHECK))->GetCheck() == 1);

    if (((CButton*)GetDlgItem(IDC_RADIO_PAY_INC))->GetCheck() == 1)
        m_PayloadMode = 1;
    else if (((CButton*)GetDlgItem(IDC_RADIO_PAY_FIX))->GetCheck() == 1)
        m_PayloadMode = 2;
    else
        m_PayloadMode = 0;
}

void CStressTestPage::SetStateText(LPCSTR text)
{
    SetDlgItemText(IDC_EDIT_STATE, text);
}

void CStressTestPage::UpdateControlStates()
{
    if (!::IsWindow(GetSafeHwnd()))
        return;

    GetDlgItem(IDC_BTN_START)->EnableWindow(m_Connected && (!m_Running || m_Paused));
    GetDlgItem(IDC_BTN_PAUSE)->EnableWindow(m_Running && !m_Paused);
    GetDlgItem(IDC_BTN_STOP)->EnableWindow(m_Running);
    GetDlgItem(IDC_BTN_SENDONCE)->EnableWindow(m_Connected);

    SetDlgItemText(IDC_BTN_START, (m_Running && m_Paused) ? SLT("继续", "Resume") : SLT("开始", "Start"));

    switch (m_State)
    {
    case 1: SetStateText(SLT("运行中", "Running")); break;
    case 2: SetStateText(SLT("已暂停", "Paused")); break;
    case 3: SetStateText(SLT("已完成", "Finished")); break;
    default:
        SetStateText(m_Connected ? SLT("就绪", "Ready") : SLT("未连接", "Not connected"));
        break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Sending
//
bool CStressTestPage::SendOneFrame()
{
    if (m_Pcan == NULL || !m_Connected)
        return false;

    DWORD id;
    DWORD counter;

    EnterCriticalSection(&m_csIds);
    id = m_NextId;
    m_NextId += m_IdStep;
    if (m_Extended)
    {
        if (m_NextId > 0x1FFFFFFF)
            m_NextId = m_IdStart;
    }
    else
    {
        if (m_NextId > 0x7FF)
            m_NextId = m_IdStart;
    }
    counter = m_Counter++;
    LeaveCriticalSection(&m_csIds);

    BYTE data[STRESS_MAX_DATA];
    memset(data, 0, sizeof(data));

    // The timestamp is taken as close as possible to the transmission
    //
    ULONGLONG txUs = NowUs();

    switch (m_PayloadMode)
    {
    case 1:
        memcpy(data, &counter, sizeof(counter));
        break;
    case 2:
        memset(data, m_FixedValue, sizeof(data));
        break;
    default:
        memcpy(data, &txUs, sizeof(txUs));
        break;
    }

    TPCANStatus status;

    if (m_IsFD)
    {
        TPCANMsgFD message;
        memset(&message, 0, sizeof(message));
        message.ID = id;
        message.DLC = (BYTE)m_Dlc;
        message.MSGTYPE = m_Extended ? PCAN_MESSAGE_EXTENDED : PCAN_MESSAGE_STANDARD;
        message.MSGTYPE |= PCAN_MESSAGE_FD | PCAN_MESSAGE_BRS;
        memcpy(message.DATA, data, sizeof(data));
        status = m_Pcan->WriteFD(m_Handle, &message);
    }
    else
    {
        TPCANMsg message;
        memset(&message, 0, sizeof(message));
        message.ID = id;
        message.LEN = (BYTE)m_Dlc;
        message.MSGTYPE = m_Extended ? PCAN_MESSAGE_EXTENDED : PCAN_MESSAGE_STANDARD;
        memcpy(message.DATA, data, sizeof(data));
        status = m_Pcan->Write(m_Handle, &message);
    }

    if (status != PCAN_ERROR_OK)
    {
        m_Core.OnSendError();
        return false;
    }

    m_Core.OnFrameSent(id, txUs, data, m_Dlc);
    return true;
}

void CStressTestPage::SendThreadFunc()
{
    timeBeginPeriod(1);

    HANDLE hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
    LARGE_INTEGER due;
    due.QuadPart = -1;

    while (WaitForSingleObject(m_hStopEvent, 0) != WAIT_OBJECT_0)
    {
        // Paused: wait until the test is continued or stopped
        //
        if (WaitForSingleObject(m_hPauseEvent, 0) == WAIT_OBJECT_0)
        {
            HANDLE waitObjects[2] = { m_hStopEvent, m_hResumeEvent };
            WaitForMultipleObjects(2, waitObjects, FALSE, INFINITE);
            continue;
        }

        if (m_FrameCount > 0 && m_Core.GetSentCount() >= m_FrameCount)
            break;

        SendOneFrame();

        if (m_IntervalMs == 0)
        {
            Sleep(0);
            continue;
        }

        due.QuadPart = -((LONGLONG)m_IntervalMs * 10000);
        SetWaitableTimer(hTimer, &due, 0, NULL, NULL, FALSE);

        HANDLE waitObjects[3] = { m_hStopEvent, m_hPauseEvent, hTimer };
        WaitForMultipleObjects(3, waitObjects, FALSE, INFINITE);
    }

    if (hTimer != NULL)
        CloseHandle(hTimer);

    timeEndPeriod(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Receiving
//
void CStressTestPage::ProcessReceivedFrame(DWORD id, const BYTE* data, int length, ULONGLONG rxUs, bool isEcho)
{
    m_Core.OnFrameReceived(id, data, length, rxUs, isEcho);
}

void CStressTestPage::RecvThreadFunc()
{
    timeBeginPeriod(1);

    while (WaitForSingleObject(m_hStopEvent, 0) != WAIT_OBJECT_0)
    {
        bool receivedAny = false;

        while (true)
        {
            DWORD id = 0;
            DWORD messageType = 0;
            int length = 0;
            BYTE data[64];
            memset(data, 0, sizeof(data));

            TPCANStatus status;

            if (m_IsFD)
            {
                TPCANMsgFD message;
                TPCANTimestampFD timestamp;
                memset(&message, 0, sizeof(message));
                memset(&timestamp, 0, sizeof(timestamp));

                status = m_Pcan->ReadFD(m_Handle, &message, &timestamp);
                if (status != PCAN_ERROR_OK)
                    break;

                id = message.ID;
                messageType = message.MSGTYPE;
                length = LengthFromDlc(message.DLC);
                memcpy(data, message.DATA, length);
            }
            else
            {
                TPCANMsg message;
                TPCANTimestamp timestamp;
                memset(&message, 0, sizeof(message));
                memset(&timestamp, 0, sizeof(timestamp));

                status = m_Pcan->Read(m_Handle, &message, &timestamp);
                if (status != PCAN_ERROR_OK)
                    break;

                id = message.ID;
                messageType = message.MSGTYPE;
                length = message.LEN;
                if (length > 8)
                    length = 8;
                memcpy(data, message.DATA, length);
            }

            receivedAny = true;
            ULONGLONG rxUs = NowUs();
            ProcessReceivedFrame(id, data, length, rxUs, (messageType & PCAN_MESSAGE_ECHO) != 0);
        }

        if (!receivedAny)
            WaitForSingleObject(m_hStopEvent, 1);
    }

    timeEndPeriod(1);
}

DWORD WINAPI CStressTestPage::SendThreadStub(LPVOID param)
{
    ((CStressTestPage*)param)->SendThreadFunc();
    return 0;
}

DWORD WINAPI CStressTestPage::RecvThreadStub(LPVOID param)
{
    ((CStressTestPage*)param)->RecvThreadFunc();
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Test control
//
void CStressTestPage::OnBnClickedStart()
{
    if (!m_Connected)
    {
        SetStateText(SLT("未连接", "Not connected"));
        return;
    }

    if (m_Running)
    {
        if (m_Paused)
        {
            ResetEvent(m_hPauseEvent);
            SetEvent(m_hResumeEvent);
            m_Paused = false;
            m_State = 1;
            UpdateControlStates();
        }
        return;
    }

    ReadConfiguration();

    m_Core.Reset();
    m_Core.SetTimeoutUs((ULONGLONG)m_TimeoutMs * 1000);
    m_Core.SetCheckEcho(m_CheckEcho);

    EnterCriticalSection(&m_csIds);
    m_NextId = m_IdStart;
    m_Counter = 0;
    LeaveCriticalSection(&m_csIds);

    m_StartUs = NowUs();
    m_Core.MarkStart(m_StartUs);
    m_LastShownSequence = 0;
    m_ListFrames.DeleteAllItems();

    m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hPauseEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hResumeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    m_Running = true;
    m_Paused = false;
    m_State = 1;

    // The main page must not read from the same channel while the test is running
    //
    CWnd* pParent = GetParent();
    if (pParent != NULL)
        ((CPCANBasicExampleDlg*)pParent)->SuspendMainReading(true);

    m_hSendThread = CreateThread(NULL, 0, SendThreadStub, this, 0, NULL);
    m_hRecvThread = CreateThread(NULL, 0, RecvThreadStub, this, 0, NULL);

    SetTimer(1, 200, NULL);
    UpdateControlStates();
}

void CStressTestPage::OnBnClickedPause()
{
    if (!m_Running || m_Paused)
        return;

    ResetEvent(m_hResumeEvent);
    SetEvent(m_hPauseEvent);
    m_Paused = true;
    m_State = 2;
    UpdateControlStates();
}

void CStressTestPage::OnBnClickedStop()
{
    StopTest();
}

void CStressTestPage::StopTest()
{
    if (!m_Running)
        return;

    ResetEvent(m_hResumeEvent);
    SetEvent(m_hStopEvent);

    if (m_hSendThread != NULL)
    {
        WaitForSingleObject(m_hSendThread, 5000);
        CloseHandle(m_hSendThread);
        m_hSendThread = NULL;
    }
    if (m_hRecvThread != NULL)
    {
        WaitForSingleObject(m_hRecvThread, 5000);
        CloseHandle(m_hRecvThread);
        m_hRecvThread = NULL;
    }

    if (m_hStopEvent != NULL) { CloseHandle(m_hStopEvent); m_hStopEvent = NULL; }
    if (m_hPauseEvent != NULL) { CloseHandle(m_hPauseEvent); m_hPauseEvent = NULL; }
    if (m_hResumeEvent != NULL) { CloseHandle(m_hResumeEvent); m_hResumeEvent = NULL; }

    m_Running = false;
    m_Paused = false;
    m_State = 3;

    KillTimer(1);
    m_Core.SweepTimeouts(NowUs());
    UpdateStatistics();

    CWnd* pParent = GetParent();
    if (pParent != NULL)
        ((CPCANBasicExampleDlg*)pParent)->SuspendMainReading(false);

    UpdateControlStates();
}

bool CStressTestPage::IsRunning()
{
    return m_Running;
}

void CStressTestPage::OnBnClickedSendOnce()
{
    if (!m_Connected)
    {
        SetStateText(SLT("未连接", "Not connected"));
        return;
    }

    ReadConfiguration();
    m_Core.SetTimeoutUs((ULONGLONG)m_TimeoutMs * 1000);
    m_Core.SetCheckEcho(m_CheckEcho);

    if (m_Core.GetSentCount() == 0)
        m_Core.MarkStart(NowUs());

    SendOneFrame();
    UpdateStatistics();
}

void CStressTestPage::OnBnClickedResetStats()
{
    m_Core.Reset();
    m_LastShownSequence = 0;

    if (::IsWindow(m_ListFrames.GetSafeHwnd()))
        m_ListFrames.DeleteAllItems();

    m_State = 0;
    UpdateStatistics();
    UpdateControlStates();
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Periodic refresh
//
void CStressTestPage::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1)
    {
        m_Core.SweepTimeouts(NowUs());
        UpdateStatistics();
        UpdateFrameList();

        if (m_Running && m_FrameCount > 0 && m_Core.GetSentCount() >= m_FrameCount)
        {
            // All requested frames are out, wait a little for the answers, then stop
            //
            StressStats stats;
            m_Core.GetStats(stats);
            if (stats.pending == 0)
                StopTest();
        }
    }

    CDialog::OnTimer(nIDEvent);
}

void CStressTestPage::UpdateStatistics()
{
    StressStats stats;
    m_Core.GetStats(stats);

    CString text;

    text.Format("%I64u", stats.sent);            SetDlgItemText(IDC_EDIT_TX, text);
    text.Format("%I64u", stats.received);        SetDlgItemText(IDC_EDIT_RX, text);
    text.Format("%I64u", stats.lost);            SetDlgItemText(IDC_EDIT_LOST, text);
    text.Format("%I64u", stats.lost);            SetDlgItemText(IDC_EDIT_TIMEOUTC, text);
    text.Format("%I64u", stats.outOfOrder);      SetDlgItemText(IDC_EDIT_OOO, text);
    text.Format("%I64u", stats.duplicates);      SetDlgItemText(IDC_EDIT_DUP, text);
    text.Format("%I64u", stats.dataErrors);      SetDlgItemText(IDC_EDIT_DATAERR, text);
    text.Format("%I64u", stats.sendErrors);      SetDlgItemText(IDC_EDIT_SENDERR, text);
    text.Format("%I64u", stats.echoFrames);      SetDlgItemText(IDC_EDIT_ECHO, text);
    text.Format("%I64u", stats.pending);         SetDlgItemText(IDC_EDIT_PENDING, text);

    text.Format("%.1f", stats.minLatencyUs);     SetDlgItemText(IDC_EDIT_MIN, text);
    text.Format("%.1f", stats.maxLatencyUs);     SetDlgItemText(IDC_EDIT_MAX, text);
    text.Format("%.1f", stats.averageUs);        SetDlgItemText(IDC_EDIT_AVG, text);
    text.Format("%.1f", stats.lastLatencyUs);    SetDlgItemText(IDC_EDIT_LAST, text);
    text.Format("%.1f", stats.jitterUs);         SetDlgItemText(IDC_EDIT_JITTER, text);

    double elapsed = (stats.startUs > 0 && stats.endUs > stats.startUs)
        ? (stats.endUs - stats.startUs) / 1000000.0 : 0.0;
    text.Format("%.3f s", elapsed);              SetDlgItemText(IDC_EDIT_ELAPSED, text);

    if (elapsed > 0)
        text.Format("%.1f", stats.sent / elapsed);
    else
        text = "0.0";
    SetDlgItemText(IDC_EDIT_RATE, text);
}

void CStressTestPage::UpdateFrameList()
{
    if (!m_LogDetails || !::IsWindow(m_ListFrames.GetSafeHwnd()))
        return;

    StressFrameRecord records[512];
    int count = m_Core.CopyRecords(records, 512);

    for (int i = 0; i < count; i++)
    {
        if (records[i].sequence <= m_LastShownSequence)
            continue;

        m_LastShownSequence = records[i].sequence;

        LPCSTR statusText;
        switch (records[i].status)
        {
        case STRESS_LATE:          statusText = SLT("超时后到达", "Late"); break;
        case STRESS_DUPLICATE:     statusText = SLT("重复/未知", "Unknown"); break;
        case STRESS_DATA_MISMATCH: statusText = SLT("数据错误", "Data error"); break;
        case STRESS_TIMEOUT:       statusText = SLT("未返回", "No answer"); break;
        case STRESS_ECHO:          statusText = SLT("自发自收", "Self"); break;
        default:                   statusText = SLT("正常", "OK"); break;
        }

        CString text;
        int item = m_ListFrames.GetItemCount();

        text.Format("%u", records[i].sequence);
        m_ListFrames.InsertItem(item, text);
        text.Format("%Xh", records[i].id);
        m_ListFrames.SetItemText(item, 1, text);
        text.Format("%I64u", records[i].txUs);
        m_ListFrames.SetItemText(item, 2, text);
        text.Format("%I64u", records[i].rxUs);
        m_ListFrames.SetItemText(item, 3, text);
        text.Format("%.1f", records[i].latencyUs);
        m_ListFrames.SetItemText(item, 4, text);
        m_ListFrames.SetItemText(item, 5, statusText);
    }

    // Keep only the last 200 entries
    //
    while (m_ListFrames.GetItemCount() > 200)
        m_ListFrames.DeleteItem(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////
// CSV export
//
void CStressTestPage::ExportCsv(LPCSTR fileName)
{
    FILE* file = NULL;
    if (fopen_s(&file, fileName, "w") != 0 || file == NULL)
        return;

    StressStats stats;
    m_Core.GetStats(stats);
    double elapsed = (stats.startUs > 0 && stats.endUs > stats.startUs)
        ? (stats.endUs - stats.startUs) / 1000000.0 : 0.0;

    fprintf(file, "PCAN-Basic stress test\n");
    fprintf(file, "Sent,%I64u\n", stats.sent);
    fprintf(file, "Received,%I64u\n", stats.received);
    fprintf(file, "Lost,%I64u\n", stats.lost);
    fprintf(file, "OutOfOrder,%I64u\n", stats.outOfOrder);
    fprintf(file, "Duplicates,%I64u\n", stats.duplicates);
    fprintf(file, "DataErrors,%I64u\n", stats.dataErrors);
    fprintf(file, "SendErrors,%I64u\n", stats.sendErrors);
    fprintf(file, "SelfReceived,%I64u\n", stats.echoFrames);
    fprintf(file, "MinLatencyUs,%.1f\n", stats.minLatencyUs);
    fprintf(file, "MaxLatencyUs,%.1f\n", stats.maxLatencyUs);
    fprintf(file, "AvgLatencyUs,%.1f\n", stats.averageUs);
    fprintf(file, "JitterUs,%.1f\n", stats.jitterUs);
    fprintf(file, "ElapsedSeconds,%.3f\n", elapsed);
    fprintf(file, "\n");
    fprintf(file, "Sequence,ID,TxUs,RxUs,LatencyUs,Status\n");

    StressFrameRecord* records = new StressFrameRecord[STRESS_RING_SIZE];
    int count = m_Core.CopyRecords(records, STRESS_RING_SIZE);
    for (int i = 0; i < count; i++)
        fprintf(file, "%u,%Xh,%I64u,%I64u,%.1f,%d\n",
            records[i].sequence, records[i].id, records[i].txUs,
            records[i].rxUs, records[i].latencyUs, records[i].status);

    delete[] records;
    fclose(file);
}

void CStressTestPage::OnBnClickedExport()
{
    CFileDialog dialog(FALSE, "csv", "stress_test.csv",
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        "CSV Files (*.csv)|*.csv|All Files (*.*)|*.*||", this);

    if (dialog.DoModal() != IDOK)
        return;

    ExportCsv(dialog.GetPathName());
}
