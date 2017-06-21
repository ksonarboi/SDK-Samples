#include <stdint.h>
#include <iostream>
#include <sstream>

#include "PageFetcher.h"
#include "KleinSonar.h"

// from main.cpp
namespace klein
{
	extern void writePage(const uint8_t*, const size_t);
	extern void printTime(std::ostream&);
	extern std::ostream& printError(TPU_HANDLE tpu, std::ostream& os);
}

using namespace klein;
const char* const _id = "$Id: //TPU-4XXX-Stream/2.13/Recorder/PageFetcher.cpp#1 $";

//-----------------------------------------------------------------------------
// PageFetcher DTOR
//-----------------------------------------------------------------------------
PageFetcher::~PageFetcher()
{
	if (bufSize && buffer)
	{
		delete [] buffer;
		bufSize = 0;
	}
	buffer = NULL;
}
//-----------------------------------------------------------------------------
// PageFetcher::fetchPage()
//-----------------------------------------------------------------------------
bool PageFetcher::fetchPage()
{
	U32 numBytes = 0;

	U32 pageStatus = NGS_FAILURE;

	BoolStat tpuStatus = DllGetTheTpuDataPageInfo2(
			recorder.tpuHandle(), pageType,
			lastPingNum + 1, &pageStatus, &numBytes);

	if (tpuStatus != NGS_SUCCESS)
	{
		std::ostringstream os;
		os << "GetDataPageInfo2() failed" 
			<< ", PT: " << pageType
			<< ", Requested Ping: " << lastPingNum+1 << " ";
		printError(recorder.tpuHandle(), os);
		std::cerr << os.str() << std::endl;


		// handle special case where tpu doesn't terminate SDFX properly
		// don't want a reset, just skip this page
		{
			DLLErrorCode theCode = NGS_NO_ERROR;
			(void) DllGetLastError(recorder.tpuHandle(), &theCode);

			if (theCode != NGS_SDFX_RECORD_TYPE_UNKNOWN)
			{
				throw os.str().c_str();
			}
			// the idea here is that this will fail until
			// either the page is gone or it is fixed
			// return false;
			// or we move on
			// lastPingNum++;
			return false;
		}
	}

	switch(pageStatus)
	{
		case NGS_GETDATA_SUCCESS: // 1
			{
				if (numBytes == 0) return false;

				// re-allocate buffer for page retrieval
				if (numBytes > bufSize)
				{
					if (bufSize)
					{
						delete [] buffer;
						bufSize = 0;
					}
					buffer = new U8[numBytes];
					bufSize = numBytes;
				}

				tpuStatus = DllGetTheTpuDataPage(recorder.tpuHandle(), buffer, numBytes);

				// ocasssionally the above getTheTpuDataPage failes...
				// it seems to expect 216 bytes in the SDFX but
				// low and behold there are only 152 bytes, just enough
				// to forget about the SDFX end record

				if (tpuStatus != NGS_SUCCESS)
				{
					std::ostringstream os;
					os << "GetTheTpuDataPage() failed" 
						<< ", PT: " << pageType
						<< ", Requested Ping: " << lastPingNum+1
						<< ", Expect " << numBytes << " Bytes ";
					printError(recorder.tpuHandle(), os);
					std::cerr << os.str() << std::endl;
					throw os.str().c_str();
				}
						
				// cast to a page, really should peek at number of bytes which is first 32 bits
				if (numBytes && numBytes <= bufSize)
				{
					const CKleinType3Header* headerInfo = reinterpret_cast<const CKleinType3Header*>(&buffer[0]);
					::printTime(std::cout);

					std::cout 
						<< " - Ping : " << headerInfo->pingNumber
						// << ", Error: 0x" << std::hex << std::setfill('0') << std::setw(8) << headerInfo.errorFlags
						// << ", Pitch: " << std::dec << std::setfill(' ') << headerInfo.pitch
						// << ", Roll: " << std::dec << std::setfill(' ') << headerInfo.roll
						// << ", Altitude: " << std::dec << std::setfill(' ') << headerInfo.altitude
						<< ", numberBytes: " << headerInfo->numberBytes
						<< ", pageVersion: " << headerInfo->pageVersion
						// << ", headerSize: " << headerInfo->headerSize
						// << ", header3ExtensionSize: " << headerInfo->header3ExtensionSize
						<< ", sdfExtensionSize: " << headerInfo->sdfExtensionSize
						<< std::endl;

					lastPingNum = headerInfo->pingNumber;

				}
				else
				{
					throw "failed to read numBytes for page";
				}
				// only successful return
				return true;
			}
			break;
		case NGS_GETDATA_ERROR_OLD: // -1
			{
				std::ostringstream os;
				os << "Page Status (NGS_GETDATA_ERROR_OLD): " << ((int)pageStatus) 
					<< ", PT: " << pageType
					<< ", Requested Ping: " << lastPingNum+1;
				std::cerr << os.str() << std::endl;

				lastPingNum = -1;	//ask for the latest ping next
			}
			break;
		default:		// 0
			{
				// nothing to do - no pages yet available
			}
			break;
		}
	return false;
}
//-----------------------------------------------------------------------------
// PageFetcher::writePage()
//-----------------------------------------------------------------------------
void PageFetcher::writePage()
{
	if (bufSize < 4) return;

	if (lastPingNum > 0)
	{
		const CKleinType3Header* h= reinterpret_cast<const CKleinType3Header*>(&buffer[0]);
		recorder.writePage(buffer, (const size_t)h->numberBytes);
	}
}
