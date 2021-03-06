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
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

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

	ConfigManager & cfg = ConfigManager::getInstance();

	int status = unlink(cfg.getValue("capture.pipename"));

	if (status) {
		fprintf(stderr, "Failed to remove pipe: %s\n", strerror(errno));
	}
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
    ** Fork and run the capture programe...
    */
   	const char * pipename = cfg.getValue("capture.pipename");
   	
	int status = mkfifo(pipename, 0644);

	if (status) {
		log.logStatus("Failed to create named pipe %s: %s", pipename, strerror(errno));
	}

   	const char * args[] = {
		cfg.getValue("capture.progname"),
		"-n",
		"-s",
		"-e",
		cfg.getValue("capture.encoding"),
		"-q",
		cfg.getValue("capture.jpgquality"),
		"-fs",
		"1",
		"-w",
		cfg.getValue("capture.hres"),
		"-h",
		cfg.getValue("capture.vres"),
		"-ISO",
		cfg.getValue("capture.iso"),
		"-o",
		cfg.getValue("capture.outputtemplate"),
		(char *)NULL
	};

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

		pipeFd = open(pipename, O_WRONLY);

		if (pipeFd < 0) {
			fprintf(stderr, "Failed to open named pipe %s", pipename);
			cleanup();
			exit(-1);
		}

		write(pipeFd, &pid, sizeof(pid_t));

		fprintf(stdout, "Child process forked with pid %d\n", pid);

		fprintf(stdout, "Running process: "); 

		for (int i = 0;i < 17;i++) {
			fprintf(stdout, "%s ", args[i]);
		}

		/*
		** Execute the capture program...
		*/
		int rtn = execlp(
			args[0], 
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
			args[16], 
			args[17]); 

		if (rtn) {
			fprintf(stderr, "Failed to execute capture process: [%s]\n", strerror(errno));
			exit(-1);
		}
	}

	/*
	 * Start threads...
	 */
	ThreadManager & threadMgr = ThreadManager::getInstance();

	threadMgr.startThreads();

	while (1) {
		PosixThread::sleep(PosixThread::seconds, 5L);
	}
	
	cleanup();

	return 0;
}
