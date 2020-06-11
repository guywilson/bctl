#include "posixthread.h"

#ifndef _INCL_THREADS
#define _INCL_THREADS

class CaptureThread : public PosixThread
{
public:
    CaptureThread() : PosixThread(true) {}

    void *      run();
};

class ThreadManager
{
public:
    static ThreadManager &  getInstance() {
        static ThreadManager instance;
        return instance;
    }

private:
    ThreadManager() {}

    CaptureThread *         pCaptureThread = NULL;

public:
    void                    startThreads();
    void                    killThreads();
};

#endif
