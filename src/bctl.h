#ifndef _INCL_BCTL
#define _INCL_BCTL

void    capturePhoto();
void    cleanup(void);
void    handleSignal(int sigNum);
void    daemonise();
float   getCPUTemp();
void    forkCaptureProg();

#endif