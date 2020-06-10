#include <string>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "configmgr.h"
#include "logger.h"
#include "bctl_error.h"
#include "threads.h"

extern "C" {
#include "strutils.h"
}

using namespace std;

void ThreadManager::startThreads()
{
	Logger & log = Logger::getInstance();

	this->pRaspiStillThread = new RaspiStillThread();
	if (this->pRaspiStillThread->start()) {
		log.logStatus("Started RaspiStillThread successfully");
	}
	else {
		throw bctl_error("Failed to start RaspiStillThread", __FILE__, __LINE__);
	}
}

void ThreadManager::killThreads()
{
	if (this->pRaspiStillThread != NULL) {
		this->pRaspiStillThread->stop();
	}
}

void RaspiStillThread::launchCapture()
{
	const char * 	args[10];
	char			szEncoding[8];
	char			szQuality[8];
	char			szWidth[8];
	char			szHeight[8];
	char			szISO[9];
	char			szOutputTemplate[64];

	ConfigManager & cfg = ConfigManager::getInstance();

	strcpy(szEncoding, "-e ");
	strcat(szEncoding, cfg.getValue("capture.encoding"));

	strcpy(szQuality, "-q ");
	strcat(szQuality, cfg.getValue("capture.jpgquality"));

	strcpy(szWidth, "-w ");
	strcat(szWidth, cfg.getValue("capture.hres"));

	strcpy(szHeight, "-h ");
	strcat(szWidth, cfg.getValue("capture.vres"));

	strcpy(szISO, "-ISO ");
	strcat(szWidth, cfg.getValue("capture.iso"));

	strcpy(szISO, "-o ");
	strcat(szWidth, cfg.getValue("capture.outputtemplate"));

	args[0] = cfg.getValue("capture.progname");				// Name of the executable
	args[1] = "-n";											// No preview
	args[2] = "-s";											// Wait for signal to capture
	args[3] = szEncoding;									// Output image format
	args[4] = szQuality;									// JPEG quality
	args[5] = "-fs 1";										// Start frame number
	args[6] = szWidth;										// Image width
	args[7] = szHeight;										// Image height
	args[8] = szISO;										// ISO
	args[9]	= szOutputTemplate;								// Output filename format

	printf(
		"Running process %s %s %s %s %s %s %s %s %s %s", 
		args[0], 
		args[1], 
		args[2], 
		args[3], 
		args[4], 
		args[5], 
		args[6], 
		args[7], 
		args[8],
		args[9]);

	/*
	** Execute the capture program...
	*/
	int rtn = execl(
		cfg.getValue("capture.progname"), 
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
		(const char *)NULL);

	if (rtn) {
		printf("Failed to execute capture process");
		throw bctl_error("Failed to execute process", __FILE__, __LINE__);
	}
}

void * RaspiStillThread::run()
{
	pid_t		pid;

	Logger & log = Logger::getInstance();

	try {
		pid = fork();

		if (pid == -1) {
			log.logError("Fork failed...");
			throw bctl_error("Failed to fork process...", __FILE__, __LINE__);
		}
		else if (pid == 0) {
			/*
			** Child process...
			*/
			this->_pid = getpid();

			log.logDebug("Child process forked with pid %d", this->_pid);
		}
	}
	catch (bctl_error & e) {
		log.logError("Thread failed: %s", e.what());
	}

	return NULL;
}

void RaspiStillThread::signalCapture()
{
	kill(this->_pid, SIGUSR1);
}
