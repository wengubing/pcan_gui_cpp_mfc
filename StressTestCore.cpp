#include "stdafx.h"
#include "StressTestCore.h"

#include <math.h>

CStressTestCore::CStressTestCore()
{
    InitializeCriticalSection(&m_cs);
    m_TimeoutUs = 1000000;
    m_CheckEcho = true;
    m_RingHead = 0;
    m_RingCount = 0;
    m_LastRxId = 0;
    m_HasRx = false;
    memset(&m_Stats, 0, sizeof(m_Stats));
    Reset();
}

CStressTestCore::~CStressTestCore()
{
    DeleteCriticalSection(&m_cs);
}

void CStressTestCore::Reset()
{
    EnterCriticalSection(&m_cs);

    m_Pending.clear();

    // The sequence numbering is kept, otherwise the first frames after a reset
    // would be skipped by the frame list ("sequence <= last shown")
    //
    DWORD nextSequence = m_Stats.nextSequence;

    memset(&m_Stats, 0, sizeof(m_Stats));
    m_Stats.nextSequence = nextSequence;
    m_Stats.minLatencyUs = 0;
    m_Stats.maxLatencyUs = 0;
    m_Stats.startUs = 0;
    m_Stats.endUs = 0;

    m_RingHead = 0;
    m_RingCount = 0;
    m_LastRxId = 0;
    m_HasRx = false;

    LeaveCriticalSection(&m_cs);
}

void CStressTestCore::SetTimeoutUs(ULONGLONG timeoutUs)
{
    EnterCriticalSection(&m_cs);
    m_TimeoutUs = (timeoutUs == 0) ? 1000000 : timeoutUs;
    LeaveCriticalSection(&m_cs);
}

void CStressTestCore::SetCheckEcho(bool enable)
{
    EnterCriticalSection(&m_cs);
    m_CheckEcho = enable;
    LeaveCriticalSection(&m_cs);
}

void CStressTestCore::MarkStart(ULONGLONG nowUs)
{
    EnterCriticalSection(&m_cs);
    if (m_Stats.startUs == 0)
        m_Stats.startUs = nowUs;
    m_Stats.endUs = nowUs;
    LeaveCriticalSection(&m_cs);
}

void CStressTestCore::AddRecord(const StressFrameRecord& record)
{
    m_Ring[m_RingHead] = record;
    m_RingHead = (m_RingHead + 1) % STRESS_RING_SIZE;
    if (m_RingCount < STRESS_RING_SIZE)
        m_RingCount++;
}

void CStressTestCore::OnFrameSent(DWORD id, ULONGLONG txUs, const BYTE* data, int length)
{
    PendingFrame frame;

    memset(&frame, 0, sizeof(frame));
    frame.txUs = txUs;
    frame.sequence = m_Stats.nextSequence;
    frame.length = length;
    if (length > STRESS_MAX_DATA)
        length = STRESS_MAX_DATA;
    if (data != NULL && length > 0)
        memcpy(frame.data, data, length);

    EnterCriticalSection(&m_cs);

    m_Stats.nextSequence++;
    m_Stats.sent++;
    if (m_Stats.startUs == 0)
        m_Stats.startUs = txUs;
    m_Stats.endUs = txUs;

    // A repeated id replaces the old request, the old one is counted as lost
    //
    std::map<DWORD, PendingFrame>::iterator found = m_Pending.find(id);
    if (found != m_Pending.end())
    {
        StressFrameRecord record;
        memset(&record, 0, sizeof(record));
        record.sequence = found->second.sequence;
        record.id = id;
        record.txUs = found->second.txUs;
        record.latencyUs = -1.0;
        record.status = STRESS_TIMEOUT;
        AddRecord(record);
        m_Stats.lost++;
        m_Pending.erase(found);
    }

    m_Pending[id] = frame;

    LeaveCriticalSection(&m_cs);
}

void CStressTestCore::OnSendError()
{
    EnterCriticalSection(&m_cs);
    m_Stats.sendErrors++;
    LeaveCriticalSection(&m_cs);
}

int CStressTestCore::OnFrameReceived(DWORD id, const BYTE* data, int length, ULONGLONG rxUs, bool isEcho)
{
    int result = STRESS_OK;
    StressFrameRecord record;

    memset(&record, 0, sizeof(record));
    record.id = id;
    record.rxUs = rxUs;
    record.latencyUs = -1.0;

    EnterCriticalSection(&m_cs);

    m_Stats.endUs = rxUs;

    if (isEcho)
    {
        m_Stats.echoFrames++;
        record.status = STRESS_ECHO;
        record.sequence = m_Stats.nextSequence;
        AddRecord(record);
        LeaveCriticalSection(&m_cs);
        return STRESS_ECHO;
    }

    std::map<DWORD, PendingFrame>::iterator found = m_Pending.find(id);
    if (found == m_Pending.end())
    {
        m_Stats.duplicates++;
        record.status = STRESS_DUPLICATE;
        record.sequence = m_Stats.nextSequence;
        AddRecord(record);
        LeaveCriticalSection(&m_cs);
        return STRESS_DUPLICATE;
    }

    PendingFrame frame = found->second;
    m_Pending.erase(found);

    record.sequence = frame.sequence;
    record.txUs = frame.txUs;
    record.latencyUs = (rxUs >= frame.txUs) ? (double)(rxUs - frame.txUs) : -1.0;

    bool late = (rxUs - frame.txUs) > m_TimeoutUs;
    if (late && record.latencyUs > 0)
        result = STRESS_LATE;

    // Payload verification: the echo must return exactly what was sent
    //
    bool payloadOk = true;
    if (m_CheckEcho)
    {
        int compareLength = frame.length < length ? frame.length : length;
        if (compareLength > STRESS_MAX_DATA)
            compareLength = STRESS_MAX_DATA;
        if (compareLength > 0 && data != NULL && memcmp(frame.data, data, compareLength) != 0)
            payloadOk = false;
        if (frame.length != length)
            payloadOk = false;
    }

    if (!payloadOk)
    {
        m_Stats.dataErrors++;
        result = STRESS_DATA_MISMATCH;
    }

    m_Stats.received++;

    if (m_HasRx && id <= m_LastRxId)
        m_Stats.outOfOrder++;
    m_LastRxId = id;
    m_HasRx = true;

    if (record.latencyUs >= 0)
    {
        double latency = record.latencyUs;
        if (m_Stats.latencySamples == 0)
        {
            m_Stats.minLatencyUs = latency;
            m_Stats.maxLatencyUs = latency;
        }
        else
        {
            if (latency < m_Stats.minLatencyUs)
                m_Stats.minLatencyUs = latency;
            if (latency > m_Stats.maxLatencyUs)
                m_Stats.maxLatencyUs = latency;
        }
        m_Stats.sumLatencyUs += latency;
        m_Stats.sumSquaredUs += latency * latency;
        m_Stats.latencySamples++;
        m_Stats.lastLatencyUs = latency;
    }

    record.status = result;
    AddRecord(record);

    LeaveCriticalSection(&m_cs);
    return result;
}

DWORD CStressTestCore::SweepTimeouts(ULONGLONG nowUs)
{
    DWORD count = 0;

    EnterCriticalSection(&m_cs);

    std::map<DWORD, PendingFrame>::iterator it = m_Pending.begin();
    while (it != m_Pending.end())
    {
        if (nowUs - it->second.txUs >= m_TimeoutUs)
        {
            StressFrameRecord record;
            memset(&record, 0, sizeof(record));
            record.sequence = it->second.sequence;
            record.id = it->first;
            record.txUs = it->second.txUs;
            record.latencyUs = -1.0;
            record.status = STRESS_TIMEOUT;
            AddRecord(record);

            m_Stats.lost++;
            count++;
            m_Pending.erase(it++);
        }
        else
            ++it;
    }

    LeaveCriticalSection(&m_cs);
    return count;
}

void CStressTestCore::GetStats(StressStats& stats)
{
    EnterCriticalSection(&m_cs);

    stats = m_Stats;
    stats.pending = (DWORD)m_Pending.size();

    if (m_Stats.latencySamples > 0)
    {
        double average = m_Stats.sumLatencyUs / (double)m_Stats.latencySamples;
        double variance = (m_Stats.sumSquaredUs / (double)m_Stats.latencySamples) - (average * average);
        stats.averageUs = average;
        stats.jitterUs = (variance > 0) ? sqrt(variance) : 0.0;
    }
    else
    {
        stats.averageUs = 0.0;
        stats.jitterUs = 0.0;
    }

    LeaveCriticalSection(&m_cs);
}

ULONGLONG CStressTestCore::GetSentCount()
{
    EnterCriticalSection(&m_cs);
    ULONGLONG result = m_Stats.sent;
    LeaveCriticalSection(&m_cs);
    return result;
}

int CStressTestCore::CopyRecords(StressFrameRecord* buffer, int maxCount)
{
    if (buffer == NULL || maxCount <= 0)
        return 0;

    EnterCriticalSection(&m_cs);

    int count = (m_RingCount < maxCount) ? m_RingCount : maxCount;
    for (int i = 0; i < count; i++)
    {
        int index = (m_RingHead - count + i + STRESS_RING_SIZE) % STRESS_RING_SIZE;
        buffer[i] = m_Ring[index];
    }

    LeaveCriticalSection(&m_cs);
    return count;
}
