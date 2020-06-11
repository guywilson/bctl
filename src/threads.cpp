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

/*
** Global PID of capture process...
*/
pid_t _capturePID = 0;

void _capturePhoto()
{
	/*
	** Send SIGUSR1 to the capture program to signal it
	** to capture a photo...
	*/
	printf("In _capturePhoto()...\n");

	if (_capturePID != 0) {
		printf("Sending SIGUSR1 to process %d\n", _capturePID);
		kill(_capturePID, SIGUSR1);
	}
}


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

	this->pCaptureThread = new CaptureThread();
	if (this->pCaptureThread->start()) {
		log.logStatus("Started CaptureThread successfully");
	}
	else {
		throw bctl_error("Failed to start CaptureThread", __FILE__, __LINE__);
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
	int			pipeFdArray[2];

	Logger & log = Logger::getInstance();

	try {
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

			this->launchCapture();
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

	return NULL;
}

void * CaptureThread::run()
{
	bool			go = true;
	unsigned long	frequency;

	ConfigManager & cfg = ConfigManager::getInstance();
	Logger & log = Logger::getInstance();

	frequency = (unsigned long)cfg.getValueAsInteger("capture.frequency");

	log.logDebug("Capture frequency read as %ld", frequency);

	while (go) {
		log.logDebug("Capturing photo");

		_capturePhoto();

		log.logDebug("Sleeping for %ld seconds zzzz", frequency);

		PosixThread::sleep(PosixThread::seconds, frequency);
	}

	return NULL;
}