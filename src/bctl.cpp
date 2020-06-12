#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "threads.h"
#include "logger.h"
#include "configmgr.h"
#include "posixthread.h"
#include "bctl_error.h"
#include "bctl.h"

void capturePhoto(pid_t pid)
{
	/*
	** Send SIGUSR1 to the capture program to signal it
	** to capture a photo...
	*/
	if (pid != 0) {
		kill(pid, SIGUSR1);
	}
}

void daemonise()
{
	pid_t			pid;
	pid_t			sid;

	fprintf(stdout, "Starting daemon...\n");
	fflush(stdout);

	do {
		pid = fork();
	}
	while ((pid == -1) && (errno == EAGAIN));

	if (pid < 0) {
		fprintf(stderr, "Forking daemon failed...\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		fprintf(stdout, "Exiting child process...\n");
		fflush(stdout);
		exit(EXIT_SUCCESS);
	}

	sid = setsid();
	
	if(sid < 0) {
		fprintf(stderr, "Failed calling setsid()...\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}

	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);    
	
	umask(0);

	if((chdir("/") == -1)) {
		fprintf(stderr, "Failed changing directory\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}
	
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
//	close(STDERR_FILENO);
}

float getCPUTemp()
{
    float       cpuTemp = -299.0;
#ifdef __arm__
    FILE *      fptr;
    char        szTemp[8];
    int         i = 0;

    ConfigManager & cfg = ConfigManager::getInstance();
    Logger & log = Logger::getInstance();

    fptr = fopen(cfg.getValue("wctl.cputempfile"), "rt");

    if (fptr == NULL) {
        log.logError("Could not open cpu temperature file %s", cfg.getValue("wctl.cputempfile"));
        return 0.0;
    }

    while (!feof(fptr)) {
        szTemp[i++] = (char)fgetc(fptr);
    }
    
    szTemp[i] = 0;

    fclose(fptr);

    cpuTemp = atof(szTemp) / 1000.0;

    log.logDebug("Got Rpi temperature: %.2f", cpuTemp);
#endif

    return cpuTemp;
}
