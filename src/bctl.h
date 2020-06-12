#include <unistd.h>

#ifndef _INCL_BCTL
#define _INCL_BCTL

void    capturePhoto(pid_t pid);
void    daemonise();
float   getCPUTemp();

#endif