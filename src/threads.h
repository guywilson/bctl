#include "posixthread.h"

#ifndef _INCL_THREADS
#define _INCL_THREADS

class RaspiStillThread : public PosixThread
{
private:
    pid_t       _pid;

    void        launchCapture();

public:
    RaspiStillThread() : PosixThread(true) {}

    void *      run();

    void        signalCapture();
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

public:
    void                    startThreads();
    void                    killThreads();
};

#endif
