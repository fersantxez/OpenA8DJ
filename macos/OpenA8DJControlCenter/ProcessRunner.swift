import Foundation

private final class LockedCapture: @unchecked Sendable {
    private let lock = NSLock()
    private var bytes = Data()
    private(set) var overflowed = false
    let limit: Int

    init(limit: Int) { self.limit = limit }

    func append(_ data: Data) -> Bool {
        lock.lock()
        defer { lock.unlock() }
        guard !overflowed else { return false }
        let remaining = max(0, limit - bytes.count)
        if data.count > remaining {
            bytes.append(data.prefix(remaining))
            overflowed = true
            return true
        }
        bytes.append(data)
        return false
    }

    func snapshot() -> (Data, Bool) {
        lock.lock()
        defer { lock.unlock() }
        return (bytes, overflowed)
    }
}

private final class RunningProcessBox: @unchecked Sendable {
    private let lock = NSLock()
    private var process: Process?

    func install(_ value: Process) {
        lock.lock()
        process = value
        lock.unlock()
    }

    func clear() {
        lock.lock()
        process = nil
        lock.unlock()
    }

    func terminate() {
        lock.lock()
        let value = process
        lock.unlock()
        guard let value, value.isRunning else { return }
        value.terminate()
    }
}

enum ProcessRunnerError: Error, Equatable, CustomStringConvertible {
    case missingTool(String)
    case untrustedTool(String)
    case launchFailed
    case timedOut
    case cancelled
    case truncated

    var description: String {
        switch self {
        case .missingTool(let tool): return "bundled tool missing: \(tool)"
        case .untrustedTool(let tool): return "bundled tool is not a trusted regular executable: \(tool)"
        case .launchFailed: return "process launch failed"
        case .timedOut: return "process timed out"
        case .cancelled: return "process cancelled"
        case .truncated: return "process output exceeded its bound"
        }
    }
}

actor BoundedProcessRunner {
    typealias Resolver = @Sendable (BundledTool) throws -> URL

    private let resolver: Resolver
    private var active = false
    private var waiters: [CheckedContinuation<Void, Never>] = []
    private var launchCount = 0
    private var maximumConcurrent = 0
    private var concurrent = 0

    init(resolver: @escaping Resolver = { try BoundedProcessRunner.bundleResolver($0) }) {
        self.resolver = resolver
    }

    static func bundleResolver(_ tool: BundledTool) throws -> URL {
        guard let resourceURL = Bundle.main.resourceURL else {
            throw ProcessRunnerError.missingTool(tool.rawValue)
        }
        let url = resourceURL.appendingPathComponent(tool.rawValue, isDirectory: false)
        let values: URLResourceValues
        do {
            values = try url.resourceValues(forKeys: [
                .isRegularFileKey, .isSymbolicLinkKey, .isExecutableKey
            ])
        } catch {
            throw ProcessRunnerError.missingTool(tool.rawValue)
        }
        guard values.isRegularFile == true,
              values.isSymbolicLink != true,
              values.isExecutable == true else {
            throw ProcessRunnerError.untrustedTool(tool.rawValue)
        }
        return url
    }

    func run(_ operation: ControlOperation) async throws -> ProcessOutput {
        await acquire()
        defer { release() }
        let executable = try resolver(operation.tool)
        launchCount += 1
        concurrent += 1
        maximumConcurrent = max(maximumConcurrent, concurrent)
        defer { concurrent -= 1 }

        let box = RunningProcessBox()
        let task = Task.detached(priority: .userInitiated) {
            try Self.execute(
                executable: executable,
                operation: operation,
                processBox: box
            )
        }
        return try await withTaskCancellationHandler {
            do {
                return try await task.value
            } catch is CancellationError {
                throw ProcessRunnerError.cancelled
            }
        } onCancel: {
            box.terminate()
            task.cancel()
        }
    }

    func policyStatistics() -> (launches: Int, maximumConcurrent: Int) {
        (launchCount, maximumConcurrent)
    }

    private func acquire() async {
        if !active {
            active = true
            return
        }
        await withCheckedContinuation { waiters.append($0) }
    }

    private func release() {
        if waiters.isEmpty {
            active = false
        } else {
            waiters.removeFirst().resume()
        }
    }

    private nonisolated static func execute(
        executable: URL,
        operation: ControlOperation,
        processBox: RunningProcessBox
    ) throws -> ProcessOutput {
        let process = Process()
        process.executableURL = executable
        process.arguments = operation.arguments
        if operation.isRead {
            var environment = ProcessInfo.processInfo.environment
            environment["OPENA8DJ_CONTROL_NO_WAKE"] = "1"
            process.environment = environment
        }

        let stdoutPipe = Pipe()
        let stderrPipe = Pipe()
        process.standardOutput = stdoutPipe
        process.standardError = stderrPipe

        let stdout = LockedCapture(limit: 512 * 1024)
        let stderr = LockedCapture(limit: 32 * 1024)
        let completed = DispatchSemaphore(value: 0)
        let overflow = DispatchSemaphore(value: 0)

        stdoutPipe.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            if !data.isEmpty, stdout.append(data) { overflow.signal() }
        }
        stderrPipe.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            if !data.isEmpty, stderr.append(data) { overflow.signal() }
        }
        process.terminationHandler = { _ in completed.signal() }

        do {
            processBox.install(process)
            try process.run()
        } catch {
            processBox.clear()
            stdoutPipe.fileHandleForReading.readabilityHandler = nil
            stderrPipe.fileHandleForReading.readabilityHandler = nil
            throw ProcessRunnerError.launchFailed
        }

        let deadline = DispatchTime.now() + operation.timeout
        var timedOut = false
        var wasTruncated = false
        while process.isRunning {
            if overflow.wait(timeout: .now()) == .success {
                wasTruncated = true
                process.terminate()
                break
            }
            if completed.wait(timeout: .now() + .milliseconds(25)) == .success {
                break
            }
            if DispatchTime.now() >= deadline {
                timedOut = true
                process.terminate()
                break
            }
        }

        if process.isRunning {
            if completed.wait(timeout: .now() + .milliseconds(250)) == .timedOut,
               process.isRunning {
                process.interrupt()
                _ = completed.wait(timeout: .now() + .milliseconds(250))
            }
        }
        process.waitUntilExit()

        stdoutPipe.fileHandleForReading.readabilityHandler = nil
        stderrPipe.fileHandleForReading.readabilityHandler = nil
        let stdoutTail = stdoutPipe.fileHandleForReading.readDataToEndOfFile()
        let stderrTail = stderrPipe.fileHandleForReading.readDataToEndOfFile()
        if stdout.append(stdoutTail) { wasTruncated = true }
        if stderr.append(stderrTail) { wasTruncated = true }
        processBox.clear()

        if Thread.current.isCancelled { throw ProcessRunnerError.cancelled }
        if timedOut { throw ProcessRunnerError.timedOut }
        let (outData, outOverflow) = stdout.snapshot()
        let (errData, errOverflow) = stderr.snapshot()
        if wasTruncated || outOverflow || errOverflow {
            throw ProcessRunnerError.truncated
        }
        return ProcessOutput(
            status: process.terminationStatus,
            stdout: outData,
            stderr: errData
        )
    }
}

actor RefreshCoordinator {
    private var visible = false
    private var cycle: Task<Void, Never>?
    private var refreshQueued = false
    private var backendFailures = 0

    func setVisible(_ value: Bool, cycleBody: @escaping @Sendable () async -> Void) {
        visible = value
        if value {
            requestRefresh(cycleBody: cycleBody)
        } else {
            refreshQueued = false
            cycle?.cancel()
            cycle = nil
        }
    }

    func requestRefresh(cycleBody: @escaping @Sendable () async -> Void) {
        guard visible else { return }
        guard cycle == nil else {
            refreshQueued = true
            return
        }
        cycle = Task {
            repeat {
                refreshQueued = false
                await cycleBody()
            } while refreshQueued && !Task.isCancelled && visible
            cycle = nil
        }
    }

    func cancel() {
        visible = false
        refreshQueued = false
        cycle?.cancel()
        cycle = nil
    }

    func noteBackendContact(success: Bool) -> Duration {
        if success {
            backendFailures = 0
            return .seconds(1)
        }
        backendFailures += 1
        let sequence = [2, 4, 8, 15]
        return .seconds(sequence[min(backendFailures - 1, sequence.count - 1)])
    }

    func currentBackoff() -> Duration {
        guard backendFailures > 0 else { return .seconds(1) }
        let sequence = [2, 4, 8, 15]
        return .seconds(sequence[min(backendFailures - 1, sequence.count - 1)])
    }
}
