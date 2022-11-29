#pragma once

#include <string>

namespace commlib
{

/** socket path (macOS) or pipe name (Windows); may throw on macOS
    aDeleteSocket is only meaningful on macOS; deletes the socket file if true.
    
    In general, careless deletion of the socket file may cause confusion if
    another process is currently listening on this socket.
    However, in AM case we're protected by the single-instance check happening earlier
 */
std::string pipeName(bool aDeleteSocket);

#ifdef __APPLE__
using ipc_lock_data_t = int;
#elif defined(_WIN32)
using ipc_lock_data_t = void*; // actually, HANDLE
#endif

class ipc_lock
{
    ipc_lock();
    ~ipc_lock();
    
    ipc_lock(ipc_lock && aOther) noexcept
    {
        mData = aOther.mData;
        mValid = aOther.mValid;
        aOther.mValid = false;
    }
    
    ipc_lock & operator = (ipc_lock const&) = delete;
    
    /** Returns true on successful lock, false if lock cannot be obtained; throws on error */
    bool try_lock();
    
    void unlock();
    
    /* blocks until the lock can be obtained;
     blocks indefinitely but that should not be a problem in our case unless we made a mistake:
     - the plug-in is only supposed to hold the lock briefly; AM won't wait long
     - the plug-in uses try_lock()
     */
    void lock();
    
private:
    ipc_lock_data_t mData;
    bool            mValid { false };
};


class ipc_sem
{
public:
    
    ipc_sem();
    ~ipc_sem();
    
    ipc_sem(ipc_sem && aOther) noexcept
    {
        mData = aOther.mData;
        mValid = aOther.mValid;
        aOther.mValid = false;
    }
    
    ipc_sem & operator = (ipc_sem &&) = delete;
    
    /** true on success, false if the semaphore is non-signaled; throws on error
     */
    bool try_wait();
    
    /** True on success, false on timeout; throws on error.
     milliseconds = 0 means INFINITE
     Timeout NOT currently implemented (on macOS, at least).
     */
    bool wait(int milliseconds);
    
    void post();
    
private:
    void *mData;
    bool  mValid { false };
};



}

