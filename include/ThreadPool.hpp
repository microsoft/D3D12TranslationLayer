// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

class CThreadPoolWork
{
    friend class CThreadPool;

private:
    PTP_WORK m_pWork = nullptr;
    std::function<void()> m_WorkFunction;

    static void CALLBACK WorkCallback(PTP_CALLBACK_INSTANCE, PVOID Context, PTP_WORK)
    {
        CThreadPoolWork* pWork = reinterpret_cast<CThreadPoolWork*>(Context);
        pWork->m_WorkFunction();
    }

public:
    CThreadPoolWork() = default;

    // Non-copyable, non-movable, must be initialized in place to avoid spurious heap allocations.
    CThreadPoolWork(CThreadPoolWork const&) = delete;
    CThreadPoolWork(CThreadPoolWork&&) = delete;
    CThreadPoolWork& operator=(CThreadPoolWork const&) = delete;
    CThreadPoolWork& operator=(CThreadPoolWork&&) = delete;

    void Wait(bool bCancel = true)
    {
        if (m_pWork)
        {
            WaitForThreadpoolWorkCallbacks(m_pWork, bCancel);
            CloseThreadpoolWork(m_pWork);
            m_pWork = nullptr;
        }
    }
    operator bool() { return m_pWork != nullptr; }

    ~CThreadPoolWork()
    {
        Wait(true);
    }
};

class CThreadPool
{
private:
    TP_CALLBACK_ENVIRON m_Environment;
    PTP_POOL m_pPool = nullptr;
    PTP_CLEANUP_GROUP m_pCleanup = nullptr;
    bool m_bCancelPendingWorkOnCleanup = true;

public:
    CThreadPool()
    {
        InitializeThreadpoolEnvironment(&m_Environment);
        m_pPool = CreateThreadpool(nullptr);
        if (!m_pPool)
        {
            throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
        }

        m_pCleanup = CreateThreadpoolCleanupGroup();
        if (!m_pCleanup)
        {
            CloseThreadpool(m_pPool);
            throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
        }

        // No more failures
        SetThreadpoolCallbackPool(&m_Environment, m_pPool);
        SetThreadpoolCallbackCleanupGroup(&m_Environment, m_pCleanup, nullptr);
    }

    ~CThreadPool()
    {
        CloseThreadpoolCleanupGroupMembers(m_pCleanup, m_bCancelPendingWorkOnCleanup, nullptr);
        CloseThreadpoolCleanupGroup(m_pCleanup);
        CloseThreadpool(m_pPool);
        DestroyThreadpoolEnvironment(&m_Environment);
    }

    // Noncopyable, non-movable since m_Environment is not a pointer.
    CThreadPool(CThreadPool const&) = delete;
    CThreadPool(CThreadPool&&) = delete;
    CThreadPool& operator=(CThreadPool const&) = delete;
    CThreadPool& operator=(CThreadPool&&) = delete;

    void SetCancelPendingWorkOnCleanup(bool bCancel) { m_bCancelPendingWorkOnCleanup = bCancel; }

    void QueueThreadpoolWork(CThreadPoolWork& Work, std::function<void()> WorkFunction)
    {
        if (Work.m_pWork != nullptr)
        {
            throw _com_error(E_INVALIDARG);
        }

        Work.m_WorkFunction = std::move(WorkFunction);
        Work.m_pWork = CreateThreadpoolWork(&CThreadPoolWork::WorkCallback, &Work, &m_Environment);
        if (!Work.m_pWork)
        {
            throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
        }

        SubmitThreadpoolWork(Work.m_pWork);
    }
};