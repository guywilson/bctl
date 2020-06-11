#include "posixthread.h"

#ifndef _INCL_THREADS
#define _INCL_THREADS

class RaspiStillThread : public PosixThread
{
private:
    void        launchCapture();

public:
    RaspiStillThread() : PosixThread(false) {}

    void *      run();

    void        signalCapture();
};

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

    RaspiStillThread *      pRaspiStillThread = NULL;
    CaptureThread *         pCaptureThread = NULL;

public:
    void                    startThreads();
    void                    killThreads();
};

#endif
