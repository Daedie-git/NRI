// © 2026 NVIDIA Corporation

Result FenceWGPU::Create(uint64_t initialValue) {
    m_IsSwapChainSemaphore = initialValue == SWAPCHAIN_SEMAPHORE;
    m_SubmittedValue = m_IsSwapChainSemaphore ? 0 : initialValue;
    m_CompletedValue = m_SubmittedValue;

    return Result::SUCCESS;
}

uint64_t FenceWGPU::UpdateCompletedValue() const {
#if defined(__EMSCRIPTEN__)
    uint32_t completedNum = 0;
    for (const FenceSubmissionWGPU& submission : m_Submissions) {
        if (!WaitForFuture(m_Device.GetInstance(), submission.future, 0))
            break;

        m_CompletedValue = std::max(m_CompletedValue, submission.value);
        completedNum++;
    }
#else
    // TODO: Fences are emulated with WGPUSubmissionIndex polling, not native timeline fence objects.
    uint32_t completedNum = 0;
    for (const FenceSubmissionWGPU& submission : m_Submissions) {
        WGPUSubmissionIndex index = submission.index;
        if (wgpuDevicePoll(m_Device, WGPU_FALSE, &index) != WGPU_TRUE)
            break;

        m_CompletedValue = std::max(m_CompletedValue, submission.value);
        completedNum++;
    }
#endif

    if (completedNum) {
        m_Submissions.erase(m_Submissions.begin(), m_Submissions.begin() + completedNum);
    }

    return m_CompletedValue;
}

uint64_t FenceWGPU::GetValue() const {
#if defined(__EMSCRIPTEN__)
    ExclusiveScope lock(m_Lock);
#else
    std::lock_guard<std::mutex> lock(m_Mutex);
#endif

    return UpdateCompletedValue();
}

void FenceWGPU::Wait(uint64_t value) {
    if (m_IsSwapChainSemaphore)
        return;

#if defined(__EMSCRIPTEN__)
    FenceSubmissionWGPU targetSubmission = {};
    bool hasTargetSubmission = false;
    {
        ExclusiveScope lock(m_Lock);

        if (value <= UpdateCompletedValue() || value > m_SubmittedValue)
            return;

        uint64_t submittedValue = m_CompletedValue;
        for (const FenceSubmissionWGPU& submission : m_Submissions) {
            submittedValue = std::max(submittedValue, submission.value);
            if (submittedValue >= value) {
                targetSubmission = submission;
                hasTargetSubmission = true;
                break;
            }
        }
    }

    if (!hasTargetSubmission)
        return;

    WaitForFuture(m_Device.GetInstance(), targetSubmission.future);

    ExclusiveScope lock(m_Lock);
    UpdateCompletedValue();
#else
    std::unique_lock<std::mutex> lock(m_Mutex);
    while (m_CompletedValue < value) {
        m_ConditionVariable.wait(lock, [this, value] {
            return m_SubmittedValue >= value;
        });

        uint32_t completedNum = 0;
        for (const FenceSubmissionWGPU& submission : m_Submissions) {
            WGPUSubmissionIndex index = submission.index;
            wgpuDevicePoll(m_Device, WGPU_TRUE, &index);
            m_CompletedValue = std::max(m_CompletedValue, submission.value);
            completedNum++;

            if (m_CompletedValue >= value)
                break;
        }

        if (completedNum)
            m_Submissions.erase(m_Submissions.begin(), m_Submissions.begin() + completedNum);
    }
#endif
}

bool FenceWGPU::IsSatisfiedBySubmissionOrder(uint64_t value) const {
#if defined(__EMSCRIPTEN__)
    ExclusiveScope lock(m_Lock);
#else
    std::lock_guard<std::mutex> lock(m_Mutex);
#endif

    return m_IsSwapChainSemaphore || value <= m_SubmittedValue;
}

#if defined(__EMSCRIPTEN__)
void FenceWGPU::Signal(uint64_t value) {
    ExclusiveScope lock(m_Lock);

    if (m_IsSwapChainSemaphore)
        return;

    WGPUQueueWorkDoneCallbackInfo callbackInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPUQueueWorkDoneStatus, WGPUStringView, void*, void*) {};

    m_SubmittedValue = std::max(m_SubmittedValue, value);
    m_Submissions.push_back({value, wgpuQueueOnSubmittedWorkDone(m_Device.GetQueue(), callbackInfo)});
}
#else
void FenceWGPU::Signal(uint64_t value, WGPUSubmissionIndex submissionIndex) {
    if (m_IsSwapChainSemaphore)
        return;

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_SubmittedValue = std::max(m_SubmittedValue, value);
        if (submissionIndex)
            m_Submissions.push_back({value, submissionIndex});
        else
            m_CompletedValue = std::max(m_CompletedValue, value);
    }

    m_ConditionVariable.notify_all();
}
#endif
