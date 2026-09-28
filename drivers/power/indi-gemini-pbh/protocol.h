/*
    Gemini Power Box Hub Advanced v3 INDI Driver

    Copyright (C) 2026 Dieter R Kedrowitsch <dieter@kedrowitsch.net>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA  02111-1307, USA.

    The full GNU General Public License is included in this distribution in the
    file called LICENSE.
*/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <sys/types.h>
#include "indi-gemini-pbh/connection_event.h"
#include "indi-gemini-pbh/instrumentation.h"
#include "indi-gemini-pbh/status_frame.h"
#include "indi-gemini-pbh/telemetry_snapshot.h"

namespace geminipbh
{

class GeminiProtocol
{
    public:
        using TelemetryCallback = std::function<void(const TelemetrySnapshot &)>;
        using TelemetryCallbackHandle = std::uint64_t;
        using ConnectionCallback = std::function<void(ConnectionEvent)>;
        using ConnectionCallbackHandle = std::uint64_t;
        using InstrumentationListener = instrumentation::Dispatcher::Listener;
        using InstrumentationListenerHandle = instrumentation::Dispatcher::ListenerHandle;

        enum class DirectQueryStatus
        {
            Success,
            Timeout,
            UnexpectedFrame,
            WriteFailed,
            MalformedResponse
        };

        GeminiProtocol();
        ~GeminiProtocol();

        // Open the serial port at 19200 8N1, enable RTS/DTR. settleMs is an optional
        // delay (ms) after open() to let the bridge stabilize.
        bool open(const std::string &port, int settleMs = 0);
        bool open(const std::string &port, int settleMs, bool exclusive);
        void close();
        bool isOpen() const;
        int lastOpenErrno() const;
        bool lastOpenWasBusy() const;

        // Connect: full H → V → G → first-frame sequence with 8 s overall budget.
        // On success, the reader and writer threads are running and the cached
        // status reflects the first telemetry frame. On failure, no threads are
        // running and fd_ is closed.
        // Returns 0 on success; non-zero on failure. Diagnostics are delivered to
        // the optional diagnostic callback when configured.
        //   2 = handshake failed
        //   3 = firmware < 308 (rejected)
        //   4 = first G frame not received in time
        //   5 = open failed
        int connect(const std::string &port, int settleMs = 0);

        // Reconnect: same as connect() but assumes an existing fd_. Closes fd_,
        // joins both worker threads, opens a new fd, runs the full sequence,
        // and respawns worker threads. Retries up to 3 times with delays 0, 1 s,
        // 2 s. Returns 0 on success; non-zero on final failure.
        int reconnect();

        // Disconnect: signal both threads to stop, join, close fd. Used by SIGINT
        // and by callers wanting a clean teardown.
        void disconnect();

        bool handshake();
        bool readFirmware(std::string &raw, int &numeric);
        DirectQueryStatus queryIdentity(std::string &identity, int timeoutMs,
                                        std::string *detail = nullptr);
        DirectQueryStatus queryFirmware(std::string &raw, int &numeric, int timeoutMs,
                                        std::string *detail = nullptr);
        bool firmwareVersion(std::string &raw, int &numeric) const;

        // Cached status snapshot. Returns true if at least one frame has been
        // parsed; false otherwise. The caller-provided StatusFrame is filled
        // from the cached state.
        bool latestStatus(StatusFrame &out, unsigned long long &frameSeq);

        // Wait until a frame with seq > minSeq has been received, or timeout
        // elapses. Returns true if the condition was met, false on timeout.
        bool waitForFrameAfter(unsigned long long minSeq, int timeoutMs);
        bool waitForFrameAfter(unsigned long long minSeq, int timeoutMs,
                               StatusFrame &out, unsigned long long &frameSeq);

        using StatusVerifier = std::function<bool(const StatusFrame &)>;
        bool waitForVerifiedState(unsigned long long preWriteSeq,
                                  StatusVerifier verifier,
                                  int timeoutMs,
                                  std::string &error,
                                  unsigned long long &verifiedSeq);

        // Telemetry callbacks receive immutable semantic snapshots after decoded
        // status frames have been stored in the cache. See docs/api.md for the
        // callback threading and lifetime contract.
        TelemetryCallbackHandle registerTelemetryCallback(TelemetryCallback callback);
        bool removeTelemetryCallback(TelemetryCallbackHandle handle);

        // Connection callbacks receive semantic connection events. See docs/api.md
        // for the callback threading and lifetime contract.
        ConnectionCallbackHandle registerConnectionCallback(ConnectionCallback callback);
        bool removeConnectionCallback(ConnectionCallbackHandle handle);

        void setInstrumentationLevel(instrumentation::Level level);
        instrumentation::Level instrumentationLevel() const;
        bool instrumentationEnabled(instrumentation::Level level) const;
        InstrumentationListenerHandle registerInstrumentationListener(InstrumentationListener listener);
        bool removeInstrumentationListener(InstrumentationListenerHandle handle);

        // Synchronous output / dew command setters. Used by short-lived CLI
        // commands. They acquire writeMutex_ and return only after the complete
        // command frame has been transmitted or a hard write error occurs.
        // Returns 0 on success, 1 on invalid argument, 4 on write failure.
        int setOutputSync(unsigned firmwareIndex, bool enabled);
        int setDewModeSync(unsigned channel, DewMode mode);
        int setDewManualPercentSync(unsigned channel, unsigned percent);
        int setDewEnabledSync(unsigned channel, bool enabled);

        // Asynchronous output / dew command setters. Used by --follow mode and
        // future INDI integration. They enqueue into the writer FIFO and return
        // before transmission.
        // Returns 0 on success, 1 on invalid argument, 2 if the queue is full,
        // 3 if writes are not currently accepted (shutdown/reconnect in progress).
        int setOutput(unsigned firmwareIndex, bool enabled);
        int setDewMode(unsigned channel, DewMode mode);
        int setDewManualPercent(unsigned channel, unsigned percent);
        int setDewEnabled(unsigned channel, bool enabled);

        // Recovery: enqueue ">G#" into the writer queue so the firmware re-arms
        // its 3-second broadcast. Returns 0 on success, 2 if the queue is full,
        // 3 if writes are not currently accepted.
        int recoverStream();

        // Optional cancellation hook used by the CLI supervisor during reconnect.
        // Must be callable from normal thread context; never invoked from signal
        // handlers.
        void setCancelFunction(std::function<bool()> fn)
        {
            cancelFn_ = std::move(fn);
        }

#ifdef GEMINI_TESTING
        void setWriteFunctionForTesting(std::function<ssize_t(int, const void *, size_t)> fn)
        {
            writeFn_ = std::move(fn);
        }
        void setFdForTesting(int fd)
        {
            fd_ = fd;
        }
        int syncWriteForTesting(const std::string &command);
        size_t queuedCommandCountForTesting();
        void injectStatusForTesting(const StatusFrame &status);
        void emitConnectionEventForTesting(ConnectionEvent event);
        bool parseFirmwarePayloadForTesting(const std::string &frame, std::string &raw, int &numeric) const;
#endif

        // Deterministic parser: consumes a payload that already has the 2-char
        // response prefix removed. Public so it can be unit-tested without
        // opening a serial port.
        bool parseStatusPayload(const std::string &payload,
                                StatusFrame &status,
                                std::string &error) const;

        // Public for the supervisor / test code. The supervisor main thread
        // observes these:
        std::atomic<bool> running_{true};            // false on SIGINT (permanent shutdown)
        std::atomic<bool> sessionStopRequested_{false}; // true on reconnect decision
        std::atomic<bool> reconnectPending_{false};  // supervisor-only signal
        std::atomic<bool> recoveryAttempted_{false}; // supervisor-only signal
        std::atomic<bool> acceptingWrites_{false};   // false during shutdown/reconnect

    private:
        struct TelemetryCallbackEntry;
        struct ConnectionCallbackEntry;

        int fd_ = -1;
        int lastOpenErrno_ = 0;
        bool lastOpenBusy_ = false;
        int wakeReadFd_ = -1;
        int wakeWriteFd_ = -1;
        std::string lastPort_;
        int lastSettleMs_ = 0;
        std::atomic<bool> connected_{false};
        std::atomic<bool> communicationFailureNotified_{false};
        std::atomic<std::uint64_t> connectionGeneration_{0};
        mutable instrumentation::Dispatcher instrumentation_;

        mutable std::mutex firmwareMutex_;
        std::string firmwareVersionRaw_;
        int firmwareVersionNumeric_ = 0;
        bool haveFirmwareVersion_ = false;

        // Cached state
        std::mutex cacheMutex_;
        std::condition_variable frameCv_;
        StatusFrame cachedStatus_{};
        unsigned long long frameSeq_{0};
        std::chrono::steady_clock::time_point lastFrameTime_{};

        // Worker threads (only alive while connected)
        std::thread reader_;
        std::thread writer_;

        // Write queue (only used in --follow / INDI-style mode)
        std::mutex queueMutex_;
        std::condition_variable queueCv_;
        std::deque<std::string> queue_;
        static constexpr size_t kQueueCap = 16;

        // Mutex serializing any write to fd_
        std::mutex writeMutex_;
        std::function<ssize_t(int, const void *, size_t)> writeFn_;
        std::function<bool()> cancelFn_;

        std::mutex telemetryCallbackMutex_;
        std::mutex callbackDispatchMutex_;
        std::condition_variable callbackDispatchCv_;
        bool callbackDispatchInProgress_ = false;
        std::vector<std::shared_ptr<TelemetryCallbackEntry>> telemetryCallbacks_;
        TelemetryCallbackHandle nextTelemetryCallbackHandle_ = 1;

        std::mutex connectionCallbackMutex_;
        std::vector<std::shared_ptr<ConnectionCallbackEntry>> connectionCallbacks_;
        ConnectionCallbackHandle nextConnectionCallbackHandle_ = 1;

        // Direct (synchronous) one-shot helpers used during connect()/reconnect()
        // when no worker threads are running. timeoutMs is a wall-clock budget.
        bool transactOnce(const std::string &command, std::string &response,
                          int timeoutMs);
        DirectQueryStatus queryTokenFrame(const std::string &command,
                                          char token,
                                          int timeoutMs,
                                          std::string &response,
                                          std::string *detail);
        bool readFrameOnce(std::string &response, int timeoutMs);
        bool readTokenFrameOnce(char token, std::string &response, int timeoutMs);
        bool parseFirmwareFrame(const std::string &frame, std::string &raw, int &numeric) const;
        int connectInternal(const std::string &port, int settleMs, ConnectionEvent successEvent);
        bool cancelled() const;
        bool sleepCancelable(int milliseconds) const;

        // Worker thread bodies
        void readerLoop(int serialFd, int wakeFd);
        void writerLoop();

        TelemetrySnapshot storeStatusFrame(const StatusFrame &status, bool dispatchCallbacks);
        TelemetrySnapshot makeTelemetrySnapshot(const StatusFrame &status,
                                                unsigned long long sequence,
                                                std::chrono::steady_clock::time_point timestamp) const;
        void beginCallbackDispatch();
        void endCallbackDispatch();
        void dispatchTelemetrySnapshot(const TelemetrySnapshot &snapshot);
        void dispatchConnectionEvent(ConnectionEvent event, const std::string &reason = std::string());
        void notifyCommunicationFailure(const std::string &reason);

        // Under writeMutex_, send the raw command. For `>...#`, `\r\n` is added.
        // Handles partial writes and retries EINTR until the frame is fully
        // transmitted or a hard write error occurs.
        bool writeAllLocked(const std::string &command);

        int syncCommand(const std::string &command);

        // Enqueue without dropping oldest. Returns 0 on success, 2 on full.
        int enqueueCommand(const std::string &command);

        void drainInputLocked();
        bool openWakePipe();
        void closeWakePipe();
        void signalReaderWake();
        void drainReaderWake(int wakeFd);

        void emitInstrumentationEvent(instrumentation::Event event) const;
        void emitConnectionInstrumentation(instrumentation::ConnectionState state,
                                           instrumentation::Level level,
                                           const std::string &reason = std::string()) const;
        void emitFrameInstrumentation(instrumentation::FrameDirection direction,
                                      const std::string &frame) const;
        void emitWarningInstrumentation(instrumentation::Level level,
                                        instrumentation::WarningCode code,
                                        const std::string &message) const;
        void emitParserAnomalyInstrumentation(instrumentation::Level level,
                                              instrumentation::ParserAnomalyCode code,
                                              const std::string &detail,
                                              const std::string &frame = std::string()) const;
        void emitReconnectInstrumentation(unsigned attempt,
                                          unsigned maxAttempts,
                                          std::chrono::milliseconds delayBeforeAttempt,
                                          bool finalAttempt,
                                          instrumentation::ReconnectResult result) const;
        void emitTimingInstrumentation(instrumentation::TimingOperation operation,
                                       std::chrono::steady_clock::time_point start,
                                       bool success) const;
        void emitVerificationInstrumentation(instrumentation::VerificationOutcome outcome,
                                             const std::string &detail,
                                             unsigned long long relatedSequence) const;
};

} // namespace geminipbh
