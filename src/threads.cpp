#include <string>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

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
	const char * 	args[17];

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

			fprintf(stderr, "Child process forked with pid %d\n", this->_pid);

			this->launchCapture();
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
