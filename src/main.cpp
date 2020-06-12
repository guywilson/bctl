#include <iostream>
#include <cstring>
#include <string>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>

#include "bctl.h"
#include "bctl_error.h"
#include "logger.h"
#include "configmgr.h"
#include "threads.h"

extern "C" {
#include "strutils.h"
#include "version.h"
}

using namespace std;

int pipeFd;

void cleanup(void)
{
	/*
	** Kill the threads...
	*/
	ThreadManager & threadMgr = ThreadManager::getInstance();

	threadMgr.killThreads();

	/*
	** Close the logger...
	*/
	Logger & log = Logger::getInstance();
	log.logInfo("Cleaning up and exiting...");
	log.closeLogger();

	closelog();

	close(pipeFd);
}

void handleSignal(int sigNum)
{
	Logger & log = Logger::getInstance();

	switch (sigNum) {
		case SIGCHLD:
			int captureStatus;

			wait(&captureStatus);
			
			if (WIFEXITED(captureStatus)) {
				log.logStatus("Capture program exited with status %d, cleaning up", WEXITSTATUS(captureStatus));
			}
			return;

		case SIGINT:
			log.logStatus("Detected SIGINT, cleaning up...");
			break;

		case SIGTERM:
			log.logStatus("Detected SIGTERM, cleaning up...");
			break;

		case SIGUSR1:
			/*
			** We're interpreting this as a request to turn on/off debug logging...
			*/
			log.logStatus("Detected SIGUSR1...");

			if (log.isLogLevel(LOG_LEVEL_INFO)) {
				int level = log.getLogLevel();
				level &= ~LOG_LEVEL_INFO;
				log.setLogLevel(level);
			}
			else {
				int level = log.getLogLevel();
				level |= LOG_LEVEL_INFO;
				log.setLogLevel(level);
			}

			if (log.isLogLevel(LOG_LEVEL_DEBUG)) {
				int level = log.getLogLevel();
				level &= ~LOG_LEVEL_DEBUG;
				log.setLogLevel(level);
			}
			else {
				int level = log.getLogLevel();
				level |= LOG_LEVEL_DEBUG;
				log.setLogLevel(level);
			}
			return;

		case SIGUSR2:
			/*
			** We're interpreting this as a request to reload config...
			*/
			log.logStatus("Detected SIGUSR2, reloading config...");

			ConfigManager & cfg = ConfigManager::getInstance();
			cfg.readConfig();

			/*
			** The only thing we can change dynamically (at present)
			** is the logging level...
			*/
			log.setLogLevel(cfg.getValue("log.level"));
			
			return;
	}

	cleanup();

    exit(0);
}

void printUsage(char * pszAppName)
{
	printf("\n Usage: %s [OPTIONS]\n\n", pszAppName);
	printf("  Options:\n");
	printf("   -h/?             Print this help\n");
	printf("   -version         Print the program version\n");
	printf("   -cfg configfile  Specify the cfg file, default is ./webconfig.cfg\n");
	printf("   -d               Daemonise this application\n");
	printf("   -log  filename   Write logs to the file\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	FILE *			fptr_pid;
	char *			pszAppName;
	char *			pszLogFileName = NULL;
	char *			pszConfigFileName = NULL;
	char			szPidFileName[PATH_MAX];
	int				i;
	bool			isDaemonised = false;
	bool			isDumpConfig = false;
	char			cwd[PATH_MAX];
	int				defaultLoggingLevel = LOG_LEVEL_INFO | LOG_LEVEL_ERROR | LOG_LEVEL_FATAL;
	pid_t			pid;
	char * 			args[18];

	CurrentTime::initialiseUptimeClock();
	
	pszAppName = strdup(argv[0]);
	getcwd(cwd, sizeof(cwd));
	
	strcpy(szPidFileName, cwd);
	strcat(szPidFileName, "/bctl.pid");

	printf("\nRunning %s from %s\n", pszAppName, cwd);

	if (argc > 1) {
		for (i = 1;i < argc;i++) {
			if (argv[i][0] == '-') {
				if (argv[i][1] == 'd') {
					isDaemonised = true;
				}
				else if (strcmp(&argv[i][1], "log") == 0) {
					pszLogFileName = strdup(&argv[++i][0]);
				}
				else if (strcmp(&argv[i][1], "cfg") == 0) {
					pszConfigFileName = strdup(&argv[++i][0]);
				}
				else if (strcmp(&argv[i][1], "-dump-config") == 0) {
					isDumpConfig = true;
				}
				else if (argv[i][1] == 'h' || argv[i][1] == '?') {
					printUsage(pszAppName);
					return 0;
				}
				else if (strcmp(&argv[i][1], "version") == 0) {
					printf("%s Version: [%s], Build date: [%s]\n\n", pszAppName, getVersion(), getBuildDate());
					return 0;
				}
				else {
					printf("Unknown argument '%s'", &argv[i][0]);
					printUsage(pszAppName);
					return 0;
				}
			}
		}
	}
	else {
		printUsage(pszAppName);
		return -1;
	}

	if (isDaemonised) {
		daemonise();
	}

	fptr_pid = fopen(szPidFileName, "wt");
	
	if (fptr_pid == NULL) {
		fprintf(stderr, "Failed to open PID file %s\n", szPidFileName);
		fflush(stderr);
	}
	else {
		fprintf(fptr_pid, "%d\n", getpid());
		fclose(fptr_pid);
	}
	
	openlog(pszAppName, LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started %s", pszAppName);

	ConfigManager & cfg = ConfigManager::getInstance();
	
	try {
		cfg.initialise(pszConfigFileName);
	}
	catch (bctl_error & e) {
		fprintf(stderr, "Could not read config file: %s [%s]\n", pszConfigFileName, e.what());
		fprintf(stderr, "Aborting!\n\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}

	if (pszConfigFileName != NULL) {
		free(pszConfigFileName);
	}

	if (isDumpConfig) {
		cfg.dumpConfig();
	}

	Logger & log = Logger::getInstance();

	if (pszLogFileName != NULL) {
		log.initLogger(pszLogFileName, defaultLoggingLevel);
		free(pszLogFileName);
	}
	else {
		const char * filename = cfg.getValue("log.filename");
		const char * level = cfg.getValue("log.level");

		if (strlen(filename) == 0 && strlen(level) == 0) {
			log.initLogger(defaultLoggingLevel);
		}
		else if (strlen(level) == 0) {
			log.initLogger(filename, defaultLoggingLevel);
		}
		else {
			log.initLogger(filename, level);
		}
	}

	/*
	 * Register signal handler for cleanup...
	 */
	if (signal(SIGINT, &handleSignal) == SIG_ERR) {
		log.logFatal("Failed to register signal handler for SIGINT");
		return -1;
	}

	if (signal(SIGTERM, &handleSignal) == SIG_ERR) {
		log.logFatal("Failed to register signal handler for SIGTERM");
		return -1;
	}

	if (signal(SIGUSR1, &handleSignal) == SIG_ERR) {
		log.logFatal("Failed to register signal handler for SIGUSR1");
		return -1;
	}

	if (signal(SIGUSR2, &handleSignal) == SIG_ERR) {
		log.logFatal("Failed to register signal handler for SIGUSR2");
		return -1;
	}

	if (signal(SIGCHLD, &handleSignal) == SIG_ERR) {
		log.logFatal("Failed to register signal handler for SIGCHLD");
		return -1;
	}

	/*
	 * Start threads...
	 */
	ThreadManager & threadMgr = ThreadManager::getInstance();

	threadMgr.startThreads();

    /*
    ** Fork and run the capture programe...
    */
	args[0] = strdup(cfg.getValue("capture.progname"));				// Name of the executable
	args[1] = strdup("-n");											// No preview
	args[2] = strdup("-s");											// Wait for signal to capture
	args[3] = strdup("-e");											// Output image format
	args[4] = strdup(cfg.getValue("capture.encoding"));
	args[5] = strdup("-q");											// JPEG quality
	args[6] = strdup(cfg.getValue("capture.jpgquality"));
	args[7] = strdup("-fs");										// Start frame number
	args[8] = strdup("1");
	args[9] = strdup("-w");											// Image width
	args[10] = strdup(cfg.getValue("capture.hres"));
	args[11] = strdup("-h");										// Image height
	args[12] = strdup(cfg.getValue("capture.vres"));
	args[13] = strdup("-ISO");										// ISO
	args[14] = strdup(cfg.getValue("capture.iso"));
	args[15] = strdup("-o");										// Output filename format
	args[16] = strdup(cfg.getValue("capture.outputtemplate"));
	args[17] = (char *)NULL;

	pid = fork();

	if (pid == -1) {
		log.logError("Fork failed...");
		cleanup();
		exit(-1);
	}
	else if (pid == 0) {
		/*
		** Child process...
		*/
		pid = getpid();

		pipeFd = open("bctlPidPipe", O_WRONLY);

		if (pipeFd < 0) {
			fprintf(stderr, "Failed to open named pipe bctlPidPipe");
			cleanup();
			exit(-1);
		}

		write(pipeFd, &pid, sizeof(pid_t));

		fprintf(stderr, "Child process forked with pid %d\n", pid);

		fprintf(
			stderr,
			"Running process: %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n", 
			args[0], 
			args[1], 
			args[2], 
			args[3], 
			args[4], 
			args[5], 
			args[6], 
			args[7], 
			args[8],
			args[9],
			args[10],
			args[11],
			args[12],
			args[13],
			args[14],
			args[15],
			args[16]);

		/*
		** Execute the capture program...
		*/
		int rtn = execvp(args[0], args); 

		if (rtn) {
			fprintf(stderr, "Failed to execute capture process: [%s]\n", strerror(errno));
			exit(-1);
		}
	}

	while (1) {
		PosixThread::sleep(PosixThread::seconds, 5L);
	}
	
	cleanup();

	return 0;
}
