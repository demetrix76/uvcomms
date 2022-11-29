#include "commlib.h"
#include "commlib_mac.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <semaphore.h>

#include <stdexcept>

namespace commlib
{

/* We create a dedicated directory for our sockets/locks and stuff because the
   default /tmp dir has sticky bit set; all files directly under /tmp may only be deleted by their creator
   regardless of their permissions
 */

/// returns the directory for locks/sockets; creates it if necessary; throws on error
std::string lockSockDirectory()
{
    std::string dir = "/tmp/com.astutegraphics";
    
    int r;
    do {
        r = mkdir(dir.c_str(), 0777);
    } while(r < 0 && errno == EINTR);
    
    if(r < 0 && errno != EEXIST)
        throw std::runtime_error("Cannot create a directory for the lock file");
    
    // we need this directory world-writable so that launching from a different user doesn't fail
    chmod(dir.c_str(), 0777); // because umask affected permissions at creation time
    
    return dir;
}

std::string lockFileName()
{
    return lockSockDirectory() + "/envoy.availability.lock";
}

std::string socketFileName(bool aDeleteSocket)
{
    std::string fname = lockSockDirectory() + "/envoy.availability.socket";
    if(aDeleteSocket)
        unlink(fname.c_str());
    return fname;
}

std::string pipeName(bool aDeleteSocket)
{
    return socketFileName(aDeleteSocket);
}

// ====================================================================================================
// ipc_lock
// ====================================================================================================

ipc_lock::ipc_lock()
{
    std::string fn = lockFileName();
    int fd;
    do {
        fd = open(fn.c_str(), O_CREAT | O_CLOEXEC | O_RDWR, 0777);
    } while(fd < 0 && errno == EINTR);
    
    if(fd < 0)
        throw std::runtime_error("Cannot create or open the lock file");
    
    // the lock file may persist across runs from different users; better keep it world-writable
    fchmod(fd, 0777);
    
    mData = fd;
    mValid = true;
}

ipc_lock::~ipc_lock()
{
    if(mValid)
        close(mData);
}

bool ipc_lock::try_lock()
{
    if(!mValid)
        throw std::runtime_error("Cannot obtain a file lock (invalid lock object");
    
    int r;
    do {
        r = flock(mData, LOCK_EX | LOCK_NB);
    } while(r < 0 && errno == EINTR);
    
    if(0 == r)
        return true;
    else
    {
        if(EWOULDBLOCK == errno)
            return false;
        else
            throw std::runtime_error("Cannot obtain a file lock; unspecified error");
    }
}

void ipc_lock::lock()
{
    if(!mValid)
        throw std::runtime_error("Cannot obtain a file lock (invalid lock object");
    
    int r;
    do {
        r = flock(mData, LOCK_EX);
    } while(r < 0 && errno == EINTR);
    
    if(r < 0)
        throw std::runtime_error("Cannot obtain a file lock; unspecified error");
}

void ipc_lock::unlock()
{
    if(!mValid)
        throw std::runtime_error("Cannot unlock a file (invalid lock object");
    
    int r;
    do {
        r = flock(mData, LOCK_UN);
    } while(r < 0 && errno == EINTR);
}

// ====================================================================================================
// ipc_sem
// ====================================================================================================

ipc_sem::ipc_sem()
{
    constexpr char const *semaphore_name = "/com.astutegraphics.envoy.availability";
    
    mode_t oldmask = umask(0);
    sem_t *theSem = SEM_FAILED;
    do {
        theSem = sem_open(semaphore_name, O_CREAT, 0777, 0);
    } while(theSem == SEM_FAILED && errno == EINTR);
    umask(oldmask);
    
    if(theSem == SEM_FAILED)
        throw std::runtime_error("Unable to create a semaphore object");
    
    mData = theSem;
    mValid = true;
}

ipc_sem::~ipc_sem()
{
    if(mValid)
        sem_close(static_cast<sem_t*>(mData));
}

bool ipc_sem::try_wait()
{
    if(!mValid)
        throw std::runtime_error("Cannot wait on a semaphore (invalid object)");
    
    int r;
    do {
        r = sem_trywait(static_cast<sem_t *>(mData));
    } while(r < 0 && errno == EINTR);
    
    if(0 == r)
        return true;
    else
    {
        if(errno == EAGAIN)
            return false;
        else
            throw std::runtime_error("Cannot wait on a semaphore (unspecified error)");
    }
}

/** True on success, false on timeout; throws on error.
 milliseconds = 0 means INFINITE
 Timeout NOT currently implemented.
 */
bool ipc_sem::wait(int milliseconds)
{
    /* POSIX does not allow for timeouts in semaphore wait functions;
     implementing a timeout requires fiddling with signals (setitimer + SIGALRM handler probably) */
    
    if(!mValid)
        throw std::runtime_error("Cannot wait on a semaphore (invalid object)");
    
    int r;
    do {
        r = sem_wait(static_cast<sem_t *>(mData));
    } while(r < 0 && errno == EINTR);
    
    if(r == 0)
        return true;
    
    else
        throw std::runtime_error("Cannot wait on a semaphore (unspecified error)");
}

void ipc_sem::post()
{
    if(!mValid)
        throw std::runtime_error("Cannot release a semaphore (invalid object)");
    
    if(0 != sem_post(static_cast<sem_t*>(mData)))
        throw std::runtime_error("Cannot release a semaphore (bad descriptor)");
    
}


}

