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

/*
** Global PID of capture process...
*/
pid_t _capturePID = 0;

void capturePhoto()
{
	/*
	** Send SIGUSR1 to the capture program to signal it
	** to capture a photo...
	*/
	if (_capturePID != 0) {
		kill(_capturePID, SIGUSR1);
	}
}

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
}

void handleSignal(int sigNum)
{
	Logger & log = Logger::getInstance();

	switch (sigNum) {
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

void forkCaptureProg()
{
	pid_t			pid;
	int				pipeFdArray[2];
	const char * 	args[17];

	Logger & log = Logger::getInstance();

	try {
		ConfigManager & cfg = ConfigManager::getInstance();

		args[0] = cfg.getValue("capture.progname");				// Name of the executable
		args[1] = "-n";											// No preview
		args[2] = "-s";											// Wait for signal to capture
		args[3] = "-e";											// Output image format
		args[4] = cfg.getValue("capture.encoding");
		args[5] = "-q";											// JPEG quality
		args[6] = cfg.getValue("capture.jpgquality");
		args[7] = "-fs";										// Start frame number
		args[8] = "1";
		args[9] = "-w";											// Image width
		args[10] = cfg.getValue("capture.hres");
		args[11] = "-h";										// Image height
		args[12] = cfg.getValue("capture.vres");
		args[13] = "-ISO";										// ISO
		args[14] = cfg.getValue("capture.iso");
		args[15] = "-o";										// Output filename format
		args[16] = cfg.getValue("capture.outputtemplate");

		pipe(pipeFdArray);

		pid = fork();

		if (pid == -1) {
			log.logError("Fork failed...");
			throw bctl_error("Failed to fork process...", __FILE__, __LINE__);
		}
		else if (pid == 0) {
			/*
			** Child process...
			*/
			close(pipeFdArray[0]);

			pid = getpid();

			write(pipeFdArray[1], &pid, sizeof(pid_t));

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
			int rtn = execlp(
				args[0], 					// Name of the executable
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
				(const char *)NULL);

			if (rtn) {
				fprintf(stderr, "Failed to execute capture process: [%s]\n", strerror(errno));
				throw bctl_error(bctl_error::buildMsg("Failed to execute capture process: [%s]", strerror(errno)), __FILE__, __LINE__);
			}
		}
		else {
			/*
			** Parent process...
			*/
			close(pipeFdArray[1]);
			read(pipeFdArray[0], &pid, sizeof(pid_t));

			_capturePID = pid;

			log.logDebug("Got capture PID [%d] from pipe", _capturePID);
		}
	}
	catch (bctl_error & e) {
		log.logError("Thread failed: %s", e.what());
	}
}
