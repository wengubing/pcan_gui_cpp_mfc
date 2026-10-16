#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////
// Stress-test core
//
// This class holds the whole measurement logic of the "automatic send/receive stress test"
// page. It has no dependency on MFC or on any window, so it can be exercised by the headless
// self-test ("--stresssim") even when no PCAN hardware is available.
//
// Measuring principle:
//   * every sent frame is registered together with a high resolution timestamp (microseconds,
//     taken with QueryPerformanceCounter) and the payload that was transmitted
//   * when a frame comes back, it is matched by its CAN-ID; the delay is
//     receive-timestamp minus send-timestamp
//   * frames that do not come back within the configured timeout are counted as lost
//
#include <windows.h>
#include <map>

// Result of a received frame
//
enum
{
    STRESS_OK = 0,            // echoed in time, payload identical
    STRESS_LATE = 1,          // answered, but after the timeout (still matched)
    STRESS_DUPLICATE = 2,     // received without a matching request (duplicate / foreign frame)
    STRESS_DATA_MISMATCH = 3, // answered in time, but the payload was changed
    STRESS_TIMEOUT = 4,       // never answered
    STRESS_ECHO = 5           // own frame, reported back by the hardware (self reception)
};

#define STRESS_MAX_DATA     8
#define STRESS_RING_SIZE    5000

// One record of the frame log (also used for the CSV export)
//
struct StressFrameRecord
{
    DWORD sequence;       // consecutive number of the frame
    DWORD id;             // CAN id of the frame
    ULONGLONG txUs;       // send timestamp in microseconds
    ULONGLONG rxUs;       // receive timestamp in microseconds (0 when not answered)
    double latencyUs;     // rxUs - txUs, negative when not answered
    int status;           // one of the STRESS_* codes
};

// Accumulated statistics
//
struct StressStats
{
    ULONGLONG sent;           // frames handed over to the API without error
    ULONGLONG received;       // frames that came back (in time)
    ULONGLONG lost;           // frames without any answer (timed out)
    ULONGLONG duplicates;     // answers without matching request
    ULONGLONG outOfOrder;     // answers that arrived out of sequence
    ULONGLONG dataErrors;     // answers with modified payload
    ULONGLONG sendErrors;     // API errors while sending
    ULONGLONG echoFrames;     // own frames reported back by the hardware
    ULONGLONG latencySamples; // number of delay measurements
    double minLatencyUs;
    double maxLatencyUs;
    double sumLatencyUs;      // sum of all delays, needed for the average
    double sumSquaredUs;      // sum of the squared delays, needed for the jitter
    double averageUs;
    double jitterUs;          // standard deviation of the delay
    double lastLatencyUs;
    DWORD pending;            // frames waiting for an answer
    ULONGLONG startUs;
    ULONGLONG endUs;
    DWORD nextSequence;
};

class CStressTestCore
{
public:
    CStressTestCore();
    ~CStressTestCore();

    // Discards every counter, pending frame and log entry
    //
    void Reset();
    void SetTimeoutUs(ULONGLONG timeoutUs);
    void SetCheckEcho(bool enable);
    void MarkStart(ULONGLONG nowUs);

    // Registers a frame that was just transmitted
    //
    void OnFrameSent(DWORD id, ULONGLONG txUs, const BYTE* data, int length);
    // Registers a failing send operation
    //
    void OnSendError();
    // Registers a received frame and returns the STRESS_* result
    //
    int OnFrameReceived(DWORD id, const BYTE* data, int length, ULONGLONG rxUs, bool isEcho);
    // Moves every frame older than the timeout into the "lost" counter
    //
    DWORD SweepTimeouts(ULONGLONG nowUs);

    // Thread safe snapshot of the statistics
    //
    void GetStats(StressStats& stats);
    ULONGLONG GetSentCount();

    // Copies the newest entries of the frame log into buffer
    //
    int CopyRecords(StressFrameRecord* buffer, int maxCount);

private:
    struct PendingFrame
    {
        ULONGLONG txUs;
        DWORD sequence;
        int length;
        BYTE data[STRESS_MAX_DATA];
    };

    void AddRecord(const StressFrameRecord& record);

    CRITICAL_SECTION m_cs;
    std::map<DWORD, PendingFrame> m_Pending;
    StressStats m_Stats;
    ULONGLONG m_TimeoutUs;
    bool m_CheckEcho;
    DWORD m_LastRxId;
    bool m_HasRx;

    StressFrameRecord m_Ring[STRESS_RING_SIZE];
    int m_RingHead;
    int m_RingCount;
};
