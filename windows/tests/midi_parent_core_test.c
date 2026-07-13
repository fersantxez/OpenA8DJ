#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "OpenA8DJMidiParentCore.h"

#include <stdio.h>
#include <string.h>

#define TEST_STREAM_BYTES (1024u * 1024u)

#define EXPECT_TRUE(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "FAIL:%s:%d:%s\n", __FILE__, __LINE__, #expression); \
            return 1; \
        } \
    } while (0)

static void
TestNotify(void *context)
{
    LONG *count = (LONG *)context;
    InterlockedIncrement(count);
}

static int
PrepareRunningCore(
    OPENA8DJ_MIDI_PARENT_CORE *core,
    OPENA8DJ_MIDI_RING *rxRing,
    OPENA8DJ_MIDI_RING *txRing,
    LONG *notifyCount)
{
    OPENA8DJ_MIDI_PARENT_EVENTS events;

    EXPECT_TRUE(OpenA8DJMidiParentInitialize(core, rxRing, txRing) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(core->state == OPENA8DJ_MIDI_PARENT_OFFLINE);
    EXPECT_TRUE(OpenA8DJMidiParentPrepare(core, &events) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE((events & OPENA8DJ_MIDI_PARENT_EVENT_STATE_CHANGED) != 0u);
    EXPECT_TRUE(OpenA8DJMidiParentOpen(
                    core,
                    OPENA8DJ_MIDI_PARENT_RX,
                    TestNotify,
                    notifyCount,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentOpen(
                    core,
                    OPENA8DJ_MIDI_PARENT_TX,
                    NULL,
                    NULL,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentSetState(
                    core,
                    OPENA8DJ_MIDI_PARENT_ACCEPTING_IO,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    return 0;
}

static int
TestLifecycleAndAtomicIo(void)
{
    OPENA8DJ_MIDI_PARENT_CORE core;
    OPENA8DJ_MIDI_RING rxRing;
    OPENA8DJ_MIDI_RING txRing;
    OPENA8DJ_MIDI_PARENT_EVENTS events;
    OPENA8DJ_MIDI_PARENT_RUNDOWN rundown;
    uint8_t bytes[OPENA8DJ_MIDI_RING_CAPACITY];
    uint8_t frame[OPENA8DJ_MIDI_EP1_PACKET_BYTES];
    OPENA8DJ_MIDI_FRAME_VIEW view;
    size_t frameLength;
    size_t bytesRead;
    size_t index;
    LONG notifyCount = 0;

    EXPECT_TRUE(OpenA8DJMidiParentInitialize(NULL, &rxRing, &txRing) ==
                OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT);
    EXPECT_TRUE(OpenA8DJMidiParentInitialize(&core, &rxRing, &rxRing) ==
                OPENA8DJ_MIDI_PARENT_INVALID_ARGUMENT);
    EXPECT_TRUE(PrepareRunningCore(&core, &rxRing, &txRing, &notifyCount) == 0);
    EXPECT_TRUE(OpenA8DJMidiParentOpen(
                    &core,
                    OPENA8DJ_MIDI_PARENT_RX,
                    TestNotify,
                    &notifyCount,
                    &events) == OPENA8DJ_MIDI_PARENT_ALREADY_OPEN);
    EXPECT_TRUE(OpenA8DJMidiParentRead(&core, bytes, sizeof(bytes), &bytesRead) ==
                OPENA8DJ_MIDI_PARENT_NO_DATA);
    EXPECT_TRUE(bytesRead == 0u);

    bytes[0] = 0x90u;
    bytes[1] = 0x40u;
    bytes[2] = 0x7fu;
    EXPECT_TRUE(OpenA8DJMidiParentWrite(&core, bytes, 3u, &events) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentTakeTxFrame(
                    &core,
                    frame,
                    sizeof(frame),
                    &frameLength) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(frameLength == 6u);
    EXPECT_TRUE(frame[0] == OPENA8DJ_MIDI_COMMAND_WRITE);
    frame[0] = OPENA8DJ_MIDI_COMMAND_READ;
    EXPECT_TRUE(OpenA8DJMidiDecodeReadFrame(frame, frameLength, &view) ==
                OPENA8DJ_MIDI_OK);
    EXPECT_TRUE(view.length == 3u && memcmp(view.bytes, bytes, 3u) == 0);

    for (index = 0u; index < sizeof(bytes); index++) {
        bytes[index] = (uint8_t)(index & 0x7fu);
    }
    EXPECT_TRUE(OpenA8DJMidiParentWrite(&core, bytes, sizeof(bytes), &events) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&txRing) == sizeof(bytes));
    EXPECT_TRUE(OpenA8DJMidiParentWrite(&core, bytes, 1u, &events) ==
                OPENA8DJ_MIDI_PARENT_RING_OVERFLOW);
    EXPECT_TRUE((events & OPENA8DJ_MIDI_PARENT_EVENT_TX_OVERFLOW) != 0u);
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&txRing) == sizeof(bytes));

    EXPECT_TRUE(OpenA8DJMidiParentFaultTx(&core, UINT32_C(0xdeadbeef), &events) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentWrite(&core, bytes, 1u, &events) ==
                OPENA8DJ_MIDI_PARENT_TX_FAULTED);
    EXPECT_TRUE(OpenA8DJMidiParentClose(
                    &core,
                    OPENA8DJ_MIDI_PARENT_TX,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentOpen(
                    &core,
                    OPENA8DJ_MIDI_PARENT_TX,
                    NULL,
                    NULL,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentWrite(&core, bytes, 1u, &events) ==
                OPENA8DJ_MIDI_PARENT_TX_FAULTED);

    EXPECT_TRUE(OpenA8DJMidiParentSetState(
                    &core,
                    OPENA8DJ_MIDI_PARENT_STOPPING,
                    &events) == OPENA8DJ_MIDI_PARENT_RUNDOWN_PENDING);
    EXPECT_TRUE(OpenA8DJMidiParentSetState(
                    &core,
                    OPENA8DJ_MIDI_PARENT_ACCEPTING_IO,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(core.txFaulted == 0u);
    EXPECT_TRUE(core.stopRunResetCount == 1u);
    EXPECT_TRUE(OpenA8DJMidiRingAvailable(&txRing) == 0u);
    EXPECT_TRUE(OpenA8DJMidiParentWrite(&core, bytes, 1u, &events) ==
                OPENA8DJ_MIDI_PARENT_OK);

    EXPECT_TRUE(OpenA8DJMidiParentSetState(
                    &core,
                    OPENA8DJ_MIDI_PARENT_STOPPING,
                    &events) == OPENA8DJ_MIDI_PARENT_RUNDOWN_PENDING);
    EXPECT_TRUE(OpenA8DJMidiParentClose(
                    &core,
                    OPENA8DJ_MIDI_PARENT_RX,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentClose(
                    &core,
                    OPENA8DJ_MIDI_PARENT_TX,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE((events & OPENA8DJ_MIDI_PARENT_EVENT_TEARDOWN_READY) != 0u);
    EXPECT_TRUE(OpenA8DJMidiParentGetRundown(&core, &rundown) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(rundown.teardownReady != 0u);
    EXPECT_TRUE(OpenA8DJMidiParentSetState(
                    &core,
                    OPENA8DJ_MIDI_PARENT_OFFLINE,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentSetState(
                    &core,
                    OPENA8DJ_MIDI_PARENT_REMOVED,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentPrepare(&core, &events) ==
                OPENA8DJ_MIDI_PARENT_INVALID_STATE);
    return 0;
}

static int
TestNotifyCloseRundown(void)
{
    static const uint8_t midiFrame[] = { 0x06u, 0x00u, 0x03u, 0x90u, 0x45u, 0x7fu };
    OPENA8DJ_MIDI_PARENT_CORE core;
    OPENA8DJ_MIDI_RING rxRing;
    OPENA8DJ_MIDI_RING txRing;
    OPENA8DJ_MIDI_PARENT_DEMUX_RESULT demux;
    OPENA8DJ_MIDI_PARENT_NOTIFY_SNAPSHOT snapshot;
    OPENA8DJ_MIDI_PARENT_NOTIFY_DISPATCH dispatch;
    OPENA8DJ_MIDI_PARENT_EVENTS events;
    uint8_t bytes[8];
    size_t bytesRead;
    LONG notifyCount = 0;

    EXPECT_TRUE(PrepareRunningCore(&core, &rxRing, &txRing, &notifyCount) == 0);
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    midiFrame,
                    sizeof(midiFrame),
                    &demux,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(snapshot.valid != 0u);
    EXPECT_TRUE(OpenA8DJMidiParentClose(
                    &core,
                    OPENA8DJ_MIDI_PARENT_RX,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentAcquireNotify(&core, &snapshot, &dispatch) ==
                OPENA8DJ_MIDI_PARENT_STALE_NOTIFY);
    EXPECT_TRUE(dispatch.callback == NULL && notifyCount == 0);

    EXPECT_TRUE(OpenA8DJMidiParentOpen(
                    &core,
                    OPENA8DJ_MIDI_PARENT_RX,
                    TestNotify,
                    &notifyCount,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentRead(&core, bytes, sizeof(bytes), &bytesRead) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(bytesRead == 3u);
    EXPECT_TRUE(OpenA8DJMidiParentDemuxEp1(
                    &core,
                    midiFrame,
                    sizeof(midiFrame),
                    &demux,
                    &snapshot,
                    &events) == OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(OpenA8DJMidiParentAcquireNotify(&core, &snapshot, &dispatch) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE(dispatch.callback != NULL);
    EXPECT_TRUE(OpenA8DJMidiParentClose(
                    &core,
                    OPENA8DJ_MIDI_PARENT_RX,
                    &events) == OPENA8DJ_MIDI_PARENT_RUNDOWN_PENDING);
    dispatch.callback(dispatch.context);
    EXPECT_TRUE(notifyCount == 1);
    EXPECT_TRUE(OpenA8DJMidiParentCompleteNotify(&core, &events) ==
                OPENA8DJ_MIDI_PARENT_OK);
    EXPECT_TRUE((events & OPENA8DJ_MIDI_PARENT_EVENT_NOTIFY_DRAINED) != 0u);
    EXPECT_TRUE(OpenA8DJMidiParentAcquireNotify(&core, &snapshot, &dispatch) ==
                OPENA8DJ_MIDI_PARENT_STALE_NOTIFY);
    EXPECT_TRUE(notifyCount == 1);
    return 0;
}

typedef struct STREAM_CONTEXT {
    OPENA8DJ_MIDI_PARENT_CORE core;
    OPENA8DJ_MIDI_RING rxRing;
    OPENA8DJ_MIDI_RING txRing;
    CRITICAL_SECTION lock;
    volatile LONG producerDone;
    volatile LONG failed;
    size_t produced;
    size_t consumed;
    LONG notifyCount;
} STREAM_CONTEXT;

static DWORD WINAPI
ProducerThread(void *parameter)
{
    STREAM_CONTEXT *context = (STREAM_CONTEXT *)parameter;
    uint8_t bytes[47];

    while (context->produced < TEST_STREAM_BYTES &&
           InterlockedCompareExchange(&context->failed, 0, 0) == 0) {
        size_t index;
        size_t remaining = TEST_STREAM_BYTES - context->produced;
        size_t length = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
        OPENA8DJ_MIDI_PARENT_STATUS status;
        OPENA8DJ_MIDI_PARENT_EVENTS events;

        for (index = 0u; index < length; index++) {
            bytes[index] = (uint8_t)((context->produced + index) & 0xffu);
        }
        EnterCriticalSection(&context->lock);
        status = OpenA8DJMidiParentWrite(
            &context->core,
            bytes,
            length,
            &events);
        if (status == OPENA8DJ_MIDI_PARENT_OK) {
            context->produced += length;
        } else if (status != OPENA8DJ_MIDI_PARENT_RING_OVERFLOW) {
            InterlockedExchange(&context->failed, 1);
        }
        LeaveCriticalSection(&context->lock);
        if (status == OPENA8DJ_MIDI_PARENT_RING_OVERFLOW) {
            SwitchToThread();
        }
    }
    InterlockedExchange(&context->producerDone, 1);
    return 0u;
}

static DWORD WINAPI
ConsumerThread(void *parameter)
{
    STREAM_CONTEXT *context = (STREAM_CONTEXT *)parameter;
    uint8_t frame[OPENA8DJ_MIDI_EP1_PACKET_BYTES];

    while (InterlockedCompareExchange(&context->failed, 0, 0) == 0) {
        OPENA8DJ_MIDI_PARENT_STATUS status;
        size_t frameLength = 0u;

        EnterCriticalSection(&context->lock);
        if (InterlockedCompareExchange(&context->producerDone, 0, 0) != 0 &&
            OpenA8DJMidiRingAvailable(&context->txRing) == 0u) {
            LeaveCriticalSection(&context->lock);
            break;
        }
        status = OpenA8DJMidiParentTakeTxFrame(
            &context->core,
            frame,
            sizeof(frame),
            &frameLength);
        if (status == OPENA8DJ_MIDI_PARENT_OK) {
            OPENA8DJ_MIDI_FRAME_VIEW view;
            size_t index;

            frame[0] = OPENA8DJ_MIDI_COMMAND_READ;
            if (OpenA8DJMidiDecodeReadFrame(frame, frameLength, &view) !=
                OPENA8DJ_MIDI_OK) {
                InterlockedExchange(&context->failed, 1);
            } else {
                for (index = 0u; index < view.length; index++) {
                    if (view.bytes[index] !=
                        (uint8_t)((context->consumed + index) & 0xffu)) {
                        InterlockedExchange(&context->failed, 1);
                        break;
                    }
                }
                context->consumed += view.length;
            }
        } else if (status != OPENA8DJ_MIDI_PARENT_NO_DATA) {
            InterlockedExchange(&context->failed, 1);
        }
        LeaveCriticalSection(&context->lock);
        if (status == OPENA8DJ_MIDI_PARENT_NO_DATA) {
            SwitchToThread();
        }
    }
    return 0u;
}

static int
TestCallerLockedOneMiB(void)
{
    STREAM_CONTEXT context;
    HANDLE producer;
    HANDLE consumer;
    HANDLE threads[2];
    DWORD waitStatus;

    memset(&context, 0, sizeof(context));
    InitializeCriticalSection(&context.lock);
    EXPECT_TRUE(PrepareRunningCore(
                    &context.core,
                    &context.rxRing,
                    &context.txRing,
                    &context.notifyCount) == 0);
    producer = CreateThread(NULL, 0u, ProducerThread, &context, 0u, NULL);
    consumer = CreateThread(NULL, 0u, ConsumerThread, &context, 0u, NULL);
    EXPECT_TRUE(producer != NULL && consumer != NULL);
    threads[0] = producer;
    threads[1] = consumer;
    waitStatus = WaitForMultipleObjects(2u, threads, TRUE, 30000u);
    EXPECT_TRUE(waitStatus == WAIT_OBJECT_0);
    EXPECT_TRUE(InterlockedCompareExchange(&context.failed, 0, 0) == 0);
    EXPECT_TRUE(context.produced == TEST_STREAM_BYTES);
    EXPECT_TRUE(context.consumed == TEST_STREAM_BYTES);
    EXPECT_TRUE(context.core.txBytes == TEST_STREAM_BYTES);
    CloseHandle(producer);
    CloseHandle(consumer);
    DeleteCriticalSection(&context.lock);
    return 0;
}

typedef struct STOP_CONTEXT {
    OPENA8DJ_MIDI_PARENT_CORE core;
    OPENA8DJ_MIDI_RING rxRing;
    OPENA8DJ_MIDI_RING txRing;
    CRITICAL_SECTION lock;
    HANDLE started;
    volatile LONG observedStop;
    volatile LONG failed;
    LONG notifyCount;
} STOP_CONTEXT;

static DWORD WINAPI
StopWriterThread(void *parameter)
{
    STOP_CONTEXT *context = (STOP_CONTEXT *)parameter;
    uint8_t byte = 0x7fu;
    uint8_t frame[OPENA8DJ_MIDI_EP1_PACKET_BYTES];
    size_t frameLength;
    size_t iterations = 0u;

    for (;;) {
        OPENA8DJ_MIDI_PARENT_STATUS status;
        OPENA8DJ_MIDI_PARENT_EVENTS events;

        EnterCriticalSection(&context->lock);
        status = OpenA8DJMidiParentWrite(&context->core, &byte, 1u, &events);
        if (status == OPENA8DJ_MIDI_PARENT_OK) {
            if (OpenA8DJMidiParentTakeTxFrame(
                    &context->core,
                    frame,
                    sizeof(frame),
                    &frameLength) != OPENA8DJ_MIDI_PARENT_OK) {
                InterlockedExchange(&context->failed, 1);
            }
        } else if (status == OPENA8DJ_MIDI_PARENT_INVALID_STATE) {
            InterlockedExchange(&context->observedStop, 1);
            LeaveCriticalSection(&context->lock);
            break;
        } else {
            InterlockedExchange(&context->failed, 1);
        }
        LeaveCriticalSection(&context->lock);
        iterations++;
        if (iterations == 1u) {
            SetEvent(context->started);
        }
        if (InterlockedCompareExchange(&context->failed, 0, 0) != 0) {
            break;
        }
        SwitchToThread();
    }
    return 0u;
}

static int
TestConcurrentStop(void)
{
    STOP_CONTEXT context;
    OPENA8DJ_MIDI_PARENT_EVENTS events;
    HANDLE writer;

    memset(&context, 0, sizeof(context));
    InitializeCriticalSection(&context.lock);
    context.started = CreateEvent(NULL, TRUE, FALSE, NULL);
    EXPECT_TRUE(context.started != NULL);
    EXPECT_TRUE(PrepareRunningCore(
                    &context.core,
                    &context.rxRing,
                    &context.txRing,
                    &context.notifyCount) == 0);
    writer = CreateThread(NULL, 0u, StopWriterThread, &context, 0u, NULL);
    EXPECT_TRUE(writer != NULL);
    EXPECT_TRUE(WaitForSingleObject(context.started, 5000u) == WAIT_OBJECT_0);
    EnterCriticalSection(&context.lock);
    EXPECT_TRUE(OpenA8DJMidiParentSetState(
                    &context.core,
                    OPENA8DJ_MIDI_PARENT_STOPPING,
                    &events) == OPENA8DJ_MIDI_PARENT_RUNDOWN_PENDING);
    LeaveCriticalSection(&context.lock);
    EXPECT_TRUE(WaitForSingleObject(writer, 5000u) == WAIT_OBJECT_0);
    EXPECT_TRUE(InterlockedCompareExchange(&context.observedStop, 0, 0) != 0);
    EXPECT_TRUE(InterlockedCompareExchange(&context.failed, 0, 0) == 0);
    CloseHandle(writer);
    CloseHandle(context.started);
    DeleteCriticalSection(&context.lock);
    return 0;
}

int
main(void)
{
    EXPECT_TRUE(TestLifecycleAndAtomicIo() == 0);
    EXPECT_TRUE(TestNotifyCloseRundown() == 0);
    EXPECT_TRUE(TestCallerLockedOneMiB() == 0);
    EXPECT_TRUE(TestConcurrentStop() == 0);
    printf("PASS: OpenA8DJ MIDI parent core\n");
    return 0;
}
