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

			const char * args[10];

			args[0] = "raspistill";			// Name of the executable
			args[1] = "-n";					// No preview
			args[2] = "-s";					// Wait for signal to capture
			args[3] = "-e jpg";				// Output image format
			args[4] = "-q 70";				// JPEG quality
			args[5] = "-fs 1";				// Start frame number
			args[6] = "-w 1280";			// Image width
			args[7] = "-h 720";				// Image height
			args[8] = "-ISO 200";			// ISO
			args[9]	= "-o img_%04d.jpg";	// Output filename format

			log.logDebug(
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
				"/usr/local/bin/raspistill", 
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
				log.logError("Failed to execute capture process");
				throw bctl_error("Failed to execute process", __FILE__, __LINE__);
			}
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
