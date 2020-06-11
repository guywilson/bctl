#include <string>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "configmgr.h"
#include "logger.h"
#include "bctl_error.h"
#include "threads.h"
#include "bctl.h"

extern "C" {
#include "strutils.h"
}

using namespace std;

void ThreadManager::startThreads()
{
	Logger & log = Logger::getInstance();

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
	if (this->pCaptureThread != NULL) {
		this->pCaptureThread->stop();
	}
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

		capturePhoto();

		log.logDebug("Sleeping for %ld seconds zzzz", frequency);

		PosixThread::sleep(PosixThread::seconds, frequency);
	}

	return NULL;
}
