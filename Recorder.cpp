#include <iostream>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <stdint.h>

#include "KleinSonar.h"
#include "Recorder.h"
#include "PageFetcher.h"
#include "PageWriter.h"

const char* const _id = "$Id: //TPU-4XXX-Stream/2.13/Recorder/Recorder.cpp#1 $";

namespace klein
{
	// from main.cpp
	extern bool shutdown;
	extern void writePage(const uint8_t*, const size_t);
	extern void printTime(std::ostream&);
	extern std::ostream& printError(TPU_HANDLE tpu, std::ostream& os);
}

using namespace klein;

//-------------------------------------------------------------------------------------
// Recorder DTOR
//-------------------------------------------------------------------------------------
Recorder::~Recorder(void)
{
	if (_tpuHandle);
	{
		DllCloseTheTpu(_tpuHandle);
	}
	if (_pageWriter)
	{
		delete _pageWriter;
		_pageWriter = NULL;
	}
}
//-------------------------------------------------------------------------------------
// Recorder::execute()
//-------------------------------------------------------------------------------------
const int Recorder::execute()
{
	connectToTPU();

	PageFetcher pf3501(*this, NGS_PAGE_TYPE_3500_UUV_LF); // low freq, 21
	PageFetcher pf3502(*this, NGS_PAGE_TYPE_3500_UUV_HF); // high freq, 22
	PageFetcher pf3503(*this, NGS_PAGE_TYPE_3500_UUV_BATHY_PC); // raw bathy, 23
	PageFetcher pf3511(*this, 24); // proc bathy, 24
	// PageFetcher pf3511(*this, NGS_PAGE_TYPE_3500_UUV_BATHY); // proc bathy, 24

	// instantiate a writer
	_pageWriter = new PageWriter(*this);

	// write invokes this chain:
	// pf.write() -> recorder.write(p,n) -> pw.write(p,n)

	// get pages loop
	while (!shutdown)
	{
		try
		{
			// remember when we start fetching
			setStartTime();

			// update record settings
			_pageWriter->update();

			// only bother if we are recording
			if (_pageWriter->record())
			{
				// while we fetch a page, write it
				while (pf3501.fetchPage()) pf3501.writePage();
				while (pf3503.fetchPage()) pf3503.writePage();
				while (pf3511.fetchPage()) pf3511.writePage();
				while (pf3502.fetchPage()) pf3502.writePage();

				_pageWriter->flush();
			}

			// sleep until next expected ping
			nap();
		}
		catch (const char*& e)
		{
			std::cerr << "Caught: " << e << std::endl;
			disconnectFromTPU();
			connectToTPU();
			pf3501.reset(); 
			pf3502.reset();
			pf3503.reset(); 
			pf3511.reset();
		}
	}

	disconnectFromTPU();

	return 0;
}
//-------------------------------------------------------------------------------------
// Recorder::connectToTPU()
//-------------------------------------------------------------------------------------
void Recorder::connectToTPU()
{
	DLLErrorCode errorCode = NGS_NO_CONNECTION_WITH_TPU;

	while (errorCode != NGS_NO_ERROR && errorCode != NGS_ALREADY_CONNECTED && !shutdown)
	{
		U32 protocolVersion = 0;

		// to use 'set' methods we need to be master, only 1 master per tpu
		// U32 config = S5KCONF_MASTER;
		// otherwise slave
		U32 config = 0;

		if (_useBlockingSockets)
		{
			// cast away const :(
			_tpuHandle = DllOpenTheTpu(config, (char *)_spuIP.c_str(), &protocolVersion);
		}
		else
		{
			static const U32 connectTimeoutMs = 250;
			_tpuHandle = DllOpenTheTpuNonBlocking(config, (char *)_spuIP.c_str(), connectTimeoutMs, &protocolVersion);
		}

		DllGetLastError(_tpuHandle, &errorCode);

		switch(errorCode)
		{
			case NGS_NO_ERROR:
			case NGS_ALREADY_CONNECTED:
				// break while loop and lower alarm
				goto lowerAlarm;
				break;
			case NGS_NO_CONNECTION_WITH_TPU:
				printTime(std::cerr);
				std::cerr << " - No connection with TPU " << std::endl; 
				// need to free the TPUHandle on failure
				disconnectFromTPU(); 
				break;
			case NGS_MASTER_ALREADY_CONNECTED:
				printTime(std::cerr);
				std::cerr << " - Master already connected " << std::endl; 
				// need to free the TPUHandle on failure
				disconnectFromTPU();
				break;
			default:
				printTime(std::cerr);
				std::cerr << " - Error code: " << errorCode << std::endl;
				// need to free the TPUHandle on failure
				disconnectFromTPU();
				break;
		}

		{ printTime(std::cerr); std::cerr << " - Alarm raised " << std::endl; usleep(1e6); }
	} // while 

lowerAlarm:
	{ printTime(std::cerr); std::cerr << " - Alarm lowered " << std::endl; usleep(1e6); }

}
//-------------------------------------------------------------------------------------
// Recorder::disconnectFromTPU()
//-------------------------------------------------------------------------------------
void Recorder::disconnectFromTPU()
{
	if (_tpuHandle == NULL)	//Nothing to do, already disconnected
		return;

	try
	{
		DllCloseTheTpu(_tpuHandle);
	}
	catch (...)
	{
		std::cerr << "Connection to the TPU was not properly closed." << std::endl;
	}

	_tpuHandle = NULL;	
}
//-------------------------------------------------------------------------------------
// Recorder::checkStatus()
//-------------------------------------------------------------------------------------
void Recorder::checkStatus(const BoolStat status)
{
	if (!_tpuHandle) return;

	if (status != NGS_SUCCESS)
	{
		std::ostringstream os;
		printError(_tpuHandle, os);
	
		DLLErrorCode theCode = NGS_NO_ERROR;
		(void) DllGetLastError(_tpuHandle, &theCode);

		if (theCode != NGS_NO_ERROR)
		{
			os << " - error code: " << theCode;
			DllClearLastError(_tpuHandle);
			throw os.str().c_str();
		}
	}
}
//-------------------------------------------------------------------------------------
// Recorder::setStartTime()
//-------------------------------------------------------------------------------------
void Recorder::setStartTime()
{
	struct timespec current_ts;

	if (clock_gettime(CLOCK_MONOTONIC, &current_ts) == -1)
	{
		throw "clock_gettime() failed";
	}

	_startFetchTime_nsec = current_ts.tv_nsec;
}
//-------------------------------------------------------------------------------------
// Recorder::nap()
//-------------------------------------------------------------------------------------
void Recorder::nap()
{
	// fetch the ping interval,
	// delta time from start
	// nap for inter ping period - delta time
	//
	// TODO - there will be some timing errors introduced here
	// it may be useful to accumulate errors and compare exepected 
	// versus actual, maybe filter
	// the nap period, maybe consider fetch status of pages,
	// etc.

	static U32 ipp_msec = 0; // ms

	if (DllGetTheTpuPingInterval(_tpuHandle, &ipp_msec) != NGS_SUCCESS)
	{
		usleep(1e5); // default, rather kind to tpu
		return;
	}

	struct timespec current_ts;
	if (clock_gettime(CLOCK_MONOTONIC, &current_ts) == -1)
	{
		throw "clock_gettime() failed";
	}

	// find delta
	{
		long delta_nsec = current_ts.tv_nsec - _startFetchTime_nsec;

		if (delta_nsec < 0)
		{
			delta_nsec += 1e9;
		}

		const long nap_usec = (ipp_msec * 1e3) - (delta_nsec / 1e3);

		// Debug dump
		if (0)
		{
			std::ostringstream os;
			os << " ipp(ms): " << ipp_msec
				<< ", start(ns): " << _startFetchTime_nsec
				<< ", curr(ns): " << current_ts.tv_nsec
				<< ", delta(ns): " << delta_nsec
				<< ", nap(us): " << nap_usec
				;
			printTime(std::cout);
			std::cout << os.str() << std::endl;
		}
		
		// don't ever go back in time
		if (nap_usec > 0)
			usleep(nap_usec);
	}
}
//-------------------------------------------------------------------------------------
// Recorder::writePage()
//-------------------------------------------------------------------------------------
void Recorder::writePage(const uint8_t* p, const size_t n)
{
	_pageWriter->writePage(p, n);
}
