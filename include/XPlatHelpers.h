// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace XPlatHelpers
{
#ifdef _WIN32
    using Event = HANDLE;
    constexpr Event InvalidEvent = nullptr;
    inline void SetEvent(Event e) { ::SetEvent(e); }
    inline Event CreateEvent() { return ::CreateEvent(nullptr, false, false, nullptr); }
    inline bool WaitForEvent(Event e, DWORD timeoutMs) { return WaitForSingleObject(e, timeoutMs) == WAIT_OBJECT_0; }
    inline bool WaitForEvent(Event e) { return WaitForEvent(e, INFINITE); }
    inline Event DuplicateEvent(Event e)
    {
        Event eNew = nullptr;
        (void)DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &eNew, 0, FALSE, DUPLICATE_SAME_ACCESS);
        return eNew;
    }
    inline void CloseEvent(Event e) { CloseHandle(e); }
    inline Event EventFromHANDLE(HANDLE h) { return h; }
#else
    using Event = int;
    constexpr Event InvalidEvent = -1;
    inline void SetEvent(Event e) { eventfd_write(e, 1); }
    inline Event CreateEvent() { return eventfd(0, 0); }
    inline bool WaitForEvent(Event e)
    {
        eventfd_t val;
        return eventfd_read(e, &val) == 0;
    }
    inline bool WaitForEvent(Event e, int timeoutMs)
    {
        pollfd fds = { e, POLLIN, 0 };
        if (poll(&fds, 1, timeoutMs) && (fds.revents & POLLIN))
        {
            return WaitForEvent(e);
        }
        return false;
    }
    inline Event DuplicateEvent(Event e) { return dup(e); }
    inline void CloseEvent(Event e) { close(e); }
    inline Event EventFromHANDLE(HANDLE h)
    {
        return static_cast<int>(reinterpret_cast<intptr_t>(h));
    }
#endif

    class unique_event
    {
        Event m_event = InvalidEvent;
    public:
        struct copy_tag {};
        unique_event() = default;
        unique_event(Event e) : m_event(e) { }
        unique_event(Event e, copy_tag) : m_event(DuplicateEvent(e)) { }
        unique_event(unique_event&& e) : m_event(e.detach()) { }
        unique_event& operator=(unique_event&& e)
        {
            close();
            m_event = e.detach();
            return *this;
        }
        ~unique_event() { close(); }
        void close()
        {
            if (*this)
            {
                CloseEvent(m_event);
            }
            m_event = InvalidEvent;
        }
        void reset(Event e = InvalidEvent) { close(); m_event = e; }
        void create() { reset(CreateEvent()); }
        Event get() { return m_event; }
        Event detach() { Event e = m_event; m_event = InvalidEvent; return e; }
        void set() const { SetEvent(m_event); }
        bool poll() const { return WaitForEvent(m_event, 0); }
        void wait() const { WaitForEvent(m_event); }
        operator bool() const { return m_event != InvalidEvent; }
    };

    // This class relies on the fact that modules are void* in both, and using the same Windows API names in the Linux Windows.h.
    class unique_module
    {
        HMODULE _hM = nullptr;
    public:
        unique_module() = default;
        explicit unique_module(HMODULE hM) : _hM(hM) { }
        explicit unique_module(const char* pCStr) : _hM(LoadLibraryA(pCStr)) { }
#ifdef _WIN32
        explicit unique_module(const wchar_t* pWStr) : _hM(LoadLibraryW(pWStr)) { }
#else
        explicit unique_module(const wchar_t* pWStr)
            : _hM(LoadLibraryA(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(pWStr).c_str()))
        {   
        }
#endif

        void reset(HMODULE hM = nullptr)
        {
            if (_hM)
                FreeLibrary(_hM);
            _hM = hM;
        }
        void load(const char* pCStr) { reset(LoadLibraryA(pCStr)); }
#ifdef _WIN32
        void load(const wchar_t* pWStr) { reset(LoadLibraryW(pWStr)); }
#else
        void load(const wchar_t* pWStr) { *this = unique_module(pWStr); }
#endif
        HMODULE detach()
        {
            HMODULE hM = _hM;
            _hM = nullptr;
            return hM;
        }

        ~unique_module() { reset(); }
        unique_module(unique_module&& o) : _hM(o.detach()) { }
        unique_module& operator=(unique_module&& o)
        {
            reset(o.detach());
            return *this;
        }

        HMODULE* get_for_external_load()
        {
            reset();
            return &_hM;
        }
        HMODULE get() const { return _hM; }
        operator bool() const { return _hM != nullptr; }

        void* proc_address(const char* pCStr) const { return GetProcAddress(_hM, pCStr); }
        template <typename T> T proc_address(const char* pCStr) const { return reinterpret_cast<T>(proc_address(pCStr)); }
    };
}