#include <sys/statfs.h>
#include <sys/types.h>		// umask()
#include <sys/stat.h>		// umask()
#include <unistd.h>		// sync()

#ifdef S3KCONF_UUV_BATHY
#undef S3KCONF_UUV_BATHY
#endif

#include <iostream> // std::cout
#include <sstream>
#include <iomanip>
#include <error.h>
#include <cstring> // memset()
#include "PageWriter.h"
#include "Recorder.h"
#include "KleinSonarPrivate.h"

namespace klein
{
	extern void writePage(const uint8_t*, const size_t);
	extern void printTime(std::ostream&);
	extern std::ostream& printError(TPU_HANDLE tpu, std::ostream& os);
	extern bool shutdown;
	extern bool operator == (const DiskRecordingSettings& lhs, const DiskRecordingSettings& rhs);
}

using namespace klein;

const int PageWriter::pingWriteInterval = 3;
const int PageWriter::cacheSize = 10*1e6;

const char* const _id =
		"$Id: //TPU-4XXX-Stream/2.13/Recorder/PageWriter.cpp#2 $";

//-------------------------------------------------------------------------------------
// PageWriter CTOR
//-------------------------------------------------------------------------------------
PageWriter::PageWriter(Recorder& r) :
		recorder(r), _fp(NULL), _cacheBytes(0), _cache(NULL), _cachePtr(NULL), _fileSize(
				0), _numPings(0)
{
	// C++ 11 _filename = {};
	_filename[0] = '\0';

	// initialize settings to 0
	memset(&_settings, '\0', sizeof(_settings));

	memset(&_status, '\0', sizeof(_status));
	_status.nVersion = 14;
	// memset(_status.szFileName, '\0', sizeof(_status.szFileName));

	// Set file permissions so that any user can read/modify/delete the output files
	// umask(~(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH));

	// Set file permissions so that any user can read/modify/delete the output files
	umask(~(S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH));

	// allocate the cache, if using
	if (cacheSize > 0)
		_cache = new uint8_t[cacheSize];

	// send the status, get the settings
	// but we need the settings to get the status
	{
		const BoolStat theStatus = DllGetTheTpuRecordInfo(r.tpuHandle(), &_status, &_settings);
		r.checkStatus(theStatus);
		std::cout << "Before:" << std::endl;
		std::cout << _status << std::endl;
		std::cout << _settings << std::endl;
	}

	// Determine the amount of disk space available
	_status.nHardDiskPercent = (U32) getDiskUsedPercent();
	_status.nHardDiskFull = (_status.nHardDiskPercent >= 95) ? 1 : 0;

	{
		const BoolStat theStatus = DllGetTheTpuRecordInfo(r.tpuHandle(), &_status, &_settings);
		r.checkStatus(theStatus);
		std::cout << "After:" << std::endl;
		std::cout << _status << std::endl;
		std::cout << _settings << std::endl;
	}

	// construct the policy
	_policy.reset(new Policy(this));

}
//-------------------------------------------------------------------------------------
// PageWriter DTOR
//-------------------------------------------------------------------------------------
PageWriter::~PageWriter()
{
	closeDataFile();
	if (cacheSize > 0)
	{
		if (_cache)
		{
			delete [] _cache;
			_cache = NULL;
		}
	}
}
//-------------------------------------------------------------------------------------
// PageWriter::getDiskUsedPercent()
//-------------------------------------------------------------------------------------
float PageWriter::getDiskUsedPercent(void)
{
	// usually 5% of the disk is reserved by the OS for root/etc
	// so don't expect df and this method to agree

	struct statfs fs; // file system stats

	if (!_settings.szFilePath[0])
		return 99.0f;

	if (statfs(_settings.szFilePath, &fs) != 0)
		return 99.0f;

	const float usage = (float)fs.f_bavail / (float)fs.f_blocks;

	// return 99% if over 19/20ths used or % used
	return (usage > .95f) ? 99.0f : (100.0f -(usage * 100.0f));
}
//-------------------------------------------------------------------------------------
// PageWriter::getFramingMode()
//-------------------------------------------------------------------------------------
uint32_t PageWriter::getFramingMode(void)
{
	U32 mode = 0;
	const BoolStat theStatus = DllGetTheTpuFramingMode(recorder.tpuHandle(), &mode);
	if (theStatus != NGS_SUCCESS)
		throw "GetTheTpuFramingMode() failed";

	return mode;
}
//-------------------------------------------------------------------------------------
// PageWriter::openNewDataFile()
//-------------------------------------------------------------------------------------
void PageWriter::openNewDataFile(const CKleinType3Header* h, const U32 ff)
{
	// Open a new data file with a new file name based on the time in pHeader.
	// The file name contains the year, day, month, hour, minute and second
	// of the ping specified by pHeader.

	// Close the old file if it is open
	if (_fp)
		closeDataFile();

	if (!h)
		throw "No page header";

	_status.nHardDiskPercent = (int) getDiskUsedPercent();

	if (_status.nHardDiskPercent >= 95)
	{
		_status.nHardDiskFull = 1;
		throw "Couldn't open file, hard drive full.  Recording is stopped.";
	}


	// only SDF supported
	sprintf(_status.szFileName, "%s%4d%02d%02d%02d%02d%02d.sdf",
			_settings.szFilePrefix, (int) h->year, (int) h->month, (int) h->day,
			(int) h->hour, (int) h->minute, (int) h->second);

	sprintf(_filename, "%s/%s", _settings.szFilePath, _status.szFileName);

	if ((_fp = fopen(_filename, "wb")) == NULL)
	{
		std::ostringstream os;
		os << "Couldn't open file: error = " << strerror(errno)
				<< ", fileName = " << _filename;
		throw os.str().c_str();
	}

	_numPings = 0;
	_fileSize = 0;
}
//-------------------------------------------------------------------------------------
// PageWriter::openDataFile()
//-------------------------------------------------------------------------------------
void PageWriter::openDataFile(void)
{
	// Open current data file.  The file name is not regenerated, i.e.,
	// the last file name that was generated by openNewDataFile() is used.

	_status.nHardDiskPercent = (int) getDiskUsedPercent();

	if (_status.nHardDiskPercent >= 95)
	{
		_status.nHardDiskFull = 1;
		std::cerr <<  "Couldn't open file, hard drive full.  Recording is stopped.\n";
		return;
	}

	// already open?
	if (_fp) return;

	// no filename?
	if (!_filename[0]) return;

	if ((_fp = fopen(_filename, "ab")) == NULL)
	{
		std::ostringstream os;
		os << "Error = " << strerror(errno) << "  Couldn't open file "
				<< _filename;
		throw os.str().c_str();
	}
}
//-------------------------------------------------------------------------------------
// PageWriter::closeDataFile()
//-------------------------------------------------------------------------------------
void PageWriter::closeDataFile(void)
{
	// Close current data file
	if (_fp)
	{
		// flush out any unwritten pings.
		fileWriteForReal();

		fclose(_fp);
		_fp = NULL;

		// Syncs the file system 
		sync();
	}
}
//-------------------------------------------------------------------------------------
// PageWriter::update()
//-------------------------------------------------------------------------------------
void PageWriter::update()
{

	// use a temp for query settings
	DiskRecordingSettings t;

	// NB - the status is written to the tpu (server) and the settings
	// are written by the tpu (server)
	
	const BoolStat theStatus = DllGetTheTpuRecordInfo(recorder.tpuHandle(), &_status, &t);

	recorder.checkStatus(theStatus);

	// no changes from server
	if (t == _settings)
	{
		// check disk status
		_status.nHardDiskPercent = (U32)getDiskUsedPercent();

		if (_status.nHardDiskPercent >= 95)
		{
			_status.nHardDiskFull = 1;
			closeDataFile();
		}
		else
		{
			_status.nHardDiskFull = 0;
		}

		// next time the status is sent, ie next ping, the tpu will see
		// the diskfull flag and turn settings.nRecordMode to 0

		// numPings exceeded?
		if (_numPings >= t.nPingsPerFile)
		{
			closeDataFile();
			memset(_filename, '\0', sizeof(_filename));
		}

		return;
	}
	else
	{
		std::ostringstream os;
		os << "Settings changed: " << t;
		printTime(std::cout);
		std::cout << os.str() << std::endl;
	}

	// record mode changed from on to off
	if (t.nRecordMode == 0 && _settings.nRecordMode == 1)
	{
		// turning off
		closeDataFile();
	}

	// figure out if we need to open a new file
	//
	// framing mode changed?
	{
		static U32 fm = getFramingMode();
		U32 m = getFramingMode();
		if (m != fm)
		{
			// keep this mode for reference
			fm = m;
			// force new file
			t.nNewFile = 1;
		}
	}
	// explicitly asked for new file
	if (t.nNewFile)
	{
		closeDataFile();
		// indicate that a new file needs to open
		memset(_filename, '\0', sizeof(_filename));
	}

	// numPings increment will cause new file?
	if (_numPings >= t.nPingsPerFile)
	{
		closeDataFile();
		memset(_filename, '\0', sizeof(_filename));
	}

	// new action required?
	switch(t.nPathAction)
	{
		case 0:
		default:
			break;
		case 1: 
			// set dir
			closeDataFile();
			memset(_filename, '\0', sizeof(_filename));
			break;
		case 2:
			// create and set dir
			closeDataFile();
			memset(_filename, '\0', sizeof(_filename));
			break;
		case 3:
			// delete empty dir
			rmdir(t.szFilePath);
			break;
	}
	// update current settings
	_settings = t;

}
//-------------------------------------------------------------------------------------
// PageWriter::flush()
//-------------------------------------------------------------------------------------
void PageWriter::flush()
{
	// note - only clears userspace in 'C' lib - not kernel buffs, would need to 
	// sync metadata for file and directory, which blocks.

	if (!(_numPings % 100) && _numPings != 0)
	{
		closeDataFile();
		openDataFile();
	}
}

//-------------------------------------------------------------------------------------
// PageWriter::writePage()
//-------------------------------------------------------------------------------------
void PageWriter::writePage(const uint8_t* p, const size_t n)
{
	// write page to file
	
	// handle file related path actions
	switch(_settings.nPathAction)
	{
		case 0: // normal
		default:
		case 3: // deleted dir
			break;
		case 1: // set dir - handled by null _filename
			break;
		case 2: // mkdir - from settings.szFilePath
			{
				// only try once
				_settings.nPathAction = 0;

				const int ret = mkdir(_settings.szFilePath, 0x777);
				if (ret)
				{
					const int e = errno;
					std::ostringstream os;
					os << "mkdir(" << _settings.szFilePath << ")"
						<< " returned: " << e << " : " << strerror(e);
					printTime(std::cerr);
					std::cerr << os.str() << std::endl;

					throw os.str().c_str();
				}
			}
			break;
	}

	// check filename, do we need to build it?
	if (_filename[0] != '\0')
	{
		openDataFile();
	}
	else
	{
		// open a new file based on page header info
		const CKleinType3Header* h = reinterpret_cast<const CKleinType3Header*>(p);

		openNewDataFile(h);
	}

	// finally write page
	_policy->writePage(p, n);

}
//-------------------------------------------------------------------------------------
// PageWriter::fileWrite()
//-------------------------------------------------------------------------------------
void PageWriter::fileWrite(const uint8_t* p, const size_t n)
{
	// This method doesn't actually write to file.  It caches for write later
	if (_cacheBytes + n < cacheSize)
	{
		// make sure cachePtr is where it ought to be
		_cachePtr = _cache + _cacheBytes;

		memcpy(_cachePtr, p, n);

		// advance
		_cachePtr += n;
		_cacheBytes += n;
	}
	else
	{
		{
		std::ostringstream os;
		os << "Cache buffer size exceeded.  Data lost. cacheBytes: "
				<< _cacheBytes << ", n: " << n;

		}
		fileWriteForReal();
		
		// try again
		fileWrite(p, n);
	}
}
//-------------------------------------------------------------------------------------
// PageWriter::fileWriteForReal()
//-------------------------------------------------------------------------------------
void PageWriter::fileWriteForReal()
{
	if (_cacheBytes)
	{
		fwrite(_cache, 1, _cacheBytes, _fp);

		_fileSize += _cacheBytes;
		_cachePtr = _cache;
		_cacheBytes = 0;
		fflush(_fp);
	}
}
//-------------------------------------------------------------------------------------
// PageWriter::addBathySdfx()
//-------------------------------------------------------------------------------------
void PageWriter::addBathySdfx(klein::UcBuffer& p)
{
	// put the bathy sdfx onto the page in p, do this by
	//   - taking off the SDFX END
	//   - adding the BATHY SDFX
	//   - add the SDFX END back
	//   - fix the sizes

	// Danger - as we are modifying p, the header* could become invalid
	// note lack of const
	CKleinType3Header* h = reinterpret_cast<CKleinType3Header*>(p.data());

	{
		std::ostringstream os;
		os << "Before: " << h->pingNumber << ", " << h->numberBytes << " " << h->sdfExtensionSize;
		std::cout << os.str() << std::endl;
	}

	// reservera buffer for the bathy sdfx and the end record
	klein::UcBuffer bsdfx;

	// SP does, cal, eng then proc
	
	// bathy cal sdfx
	{
		U32 n = 0;
		if (NGS_SUCCESS != DllGetTheSdfxRecordSize(recorder.tpuHandle(), 
					SDFX_RECORD_ID_BATHY_CAL_1, &n))
		{
			std::ostringstream os; printTime(os); 
			os << " - Could not GetTheSdfxRecordSize(BATHY_CAL) - "; 
			printError(recorder.tpuHandle(), os);
			throw os.str().c_str();
		}

		// allocate space for it
		unsigned char b[n];

		if (NGS_SUCCESS != DllGetTheSdfxRecord(recorder.tpuHandle(), 
			SDFX_RECORD_ID_BATHY_CAL_1, b, n))
		{
			std::ostringstream os; printTime(os); 
			os << " - Could not GetTheSdfxRecord(BATHY_CAL) - "; 
			printError(recorder.tpuHandle(), os);
			throw os.str().c_str();
		}

		// the size returned by the getSdfxRecordSize is not the size of the
		// structure, basically a rounded up block size 
		const SDFX_RECORD_HEADER* r = reinterpret_cast<const SDFX_RECORD_HEADER*>(b);
		klein::UcBuffer b2(b, r->recordNumBytes);
		bsdfx += b2;
	}

	// bathy eng sdfx
	{
		U32 n = 0;
		if (NGS_SUCCESS != DllGetTheSdfxRecordSize(recorder.tpuHandle(), 
					SDFX_RECORD_ID_BATHY_ENG_SETTINGS_1, &n))
		{
			std::ostringstream os; printTime(os); 
			os << " - Could not GetTheSdfxRecordSize(BATHY_ENG_SETTINGS) - "; 
			printError(recorder.tpuHandle(), os);
			throw os.str().c_str();
		}

		// allocate space for it
		unsigned char b[n];

		if (NGS_SUCCESS != DllGetTheSdfxRecord(recorder.tpuHandle(), 
			SDFX_RECORD_ID_BATHY_ENG_SETTINGS_1, b, n))
		{
			std::ostringstream os; printTime(os); 
			os << " - Could not GetTheSdfxRecord(BATHY_ENG_SETTINGS) - "; 
			printError(recorder.tpuHandle(), os);
			throw os.str().c_str();
		}

		const SDFX_RECORD_HEADER* r = reinterpret_cast<const SDFX_RECORD_HEADER*>(b);
		klein::UcBuffer b2(b, r->recordNumBytes);
		bsdfx += b2;
	}

	// bathy proc sdfx
	{
		U32 n = 0;
		if (NGS_SUCCESS != DllGetTheSdfxRecordSize(recorder.tpuHandle(), 
					SDFX_RECORD_ID_BATHY_PROC_SETTINGS_1, &n))
		{
			std::ostringstream os; printTime(os); 
			os << " - Could not GetTheSdfxRecordSize(BATHY_PROC_SETTINGS) - "; 
			printError(recorder.tpuHandle(), os);
			throw os.str().c_str();
		}

		// allocate space for it
		unsigned char b[n];

		if (NGS_SUCCESS != DllGetTheSdfxRecord(recorder.tpuHandle(), 
			SDFX_RECORD_ID_BATHY_PROC_SETTINGS_1, b, n))
		{
			std::ostringstream os; printTime(os); 
			os << " - Could not GetTheSdfxRecord(BATHY_PROC_SETTINGS) - "; 
			printError(recorder.tpuHandle(), os);
			throw os.str().c_str();
		}

		const SDFX_RECORD_HEADER* r = reinterpret_cast<const SDFX_RECORD_HEADER*>(b);
		klein::UcBuffer b2(b, r->recordNumBytes);
		bsdfx += b2;
	}

	// end sdfx
	{
		SDFX_RECORD_HEADER endSdfx;
		memset(&endSdfx, 0, sizeof(endSdfx));
		endSdfx.recordId = SDFX_RECORD_ID_END;
		endSdfx.recordNumBytes = sizeof(endSdfx);
		endSdfx.headerVersion = SDFX_HEADER_VERSION_1;
		endSdfx.recordVersion = SDFX_RECORD_VERSION_END;

		unsigned char* q = reinterpret_cast<unsigned char*>(&endSdfx);
		UcBuffer eb(q, sizeof(endSdfx));

		bsdfx += eb;
	}

	// fix the page inside of p
	{
		// for some reason unknown to me, the sdfx size is
		// also written after the data section (as well as header)

		const U32 numberBytes = h->numberBytes;
		const U32 sdfxSize = h->sdfExtensionSize;

		// truncate p at end - sizeof sdfx end
		p.resize(numberBytes - sizeof(SDFX_RECORD_HEADER));

		// now add the sdfx
		klein::UcBuffer t = p + bsdfx;

		p.clear();
		p = t;

		// reset h to point to data
		h = reinterpret_cast<CKleinType3Header*>(p.data());

		// h points into p, p may change with the addition
		h->numberBytes = p.length();
		h->sdfExtensionSize = sdfxSize - sizeof(SDFX_RECORD_HEADER) + bsdfx.length();

		// overwrite the spot that has the sdfx size after the channel data
		unsigned char* xSize = p.data() + numberBytes - sdfxSize;
		U32* xx = (U32*)xSize;
		*xx = h->sdfExtensionSize;
	}
}
//-------------------------------------------------------------------------------------
// PageWriter::Policy CTOR
//-------------------------------------------------------------------------------------
PageWriter::Policy::Policy(PageWriter* pw) : _pw(pw),
	_queue(new PingQueue_t(50))
{
}
//-------------------------------------------------------------------------------------
// PageWriter::Policy::writePage()
//-------------------------------------------------------------------------------------
void PageWriter::Policy::writePage(const uint8_t* p, const size_t n)
{

	// sanity check ping
	if (n < 4) throw "writePage() empty ping";

	const CKleinType3Header* h = reinterpret_cast<const CKleinType3Header*>(p);

	if (h->numberBytes < n) throw "writePage() too small  ping";

	const uint32_t pingNum = h->pingNumber;

	// search the queue for this ping
	PingQueue_t::iterator pit = findPing(pingNum);
	if (pit == _queue->end())
	{
		// construct a new ping
		const uint32_t mode = _pw->getFramingMode();
		const bool bathy = h->capabilityMask & SYS_CAP_BATHY;
		// mask bits MSB to LSB [bathy, 3503, HF, LF]
		// don't try to get bathy pages without the 3503
		const uint32_t mask = mode + ((bathy && (mode & 0x4))? 8 : 0);

		// if the queue is full, we may want to write pings
		// before they fall off... - useful if bathy proc dies
		// btw, they don't get their dtors called so do it explicitly
		if (_queue->full())
		{
			Ping& p = _queue->front();
			writePings(p.pingNum);
			_queue->pop_front();
		}

		Ping ping(pingNum, mask);
		pit = _queue->insert(pit, ping);

	}

	// set an iterator to the ping
	// and update its buffers
	switch(h->pageVersion)
	{
		case 3501:
			pit->p3501 = UcBuffer(p, n);
			break;
		case 3502:
			pit->p3502 = UcBuffer(p, n);
			break;
		case 3503:
			pit->p3503 = UcBuffer(p, n);
			break;
		case 3511:
			pit->p3511 = UcBuffer(p, n);
			break;
		default:
			break;
	}

	if (pingReadyForWrite(*pit))
	{
		// write all pings upto this ping num
		writePings(pit->pingNum);
	}
}
//-------------------------------------------------------------------------------------
// PageWriter::Policy::pingReadyForWrite()
//-------------------------------------------------------------------------------------
bool PageWriter::Policy::pingReadyForWrite(const Ping& p)
{
	// the mask indicates which pages we want

	// nothing to do
	if (!p.mask) return false;

	// already written?
	if (p.written) return false;

	// if any are set but empty then not ready
	if (p.mask & 0x01 && p.p3501.empty()) return false;
	if (p.mask & 0x02 && p.p3502.empty()) return false;
	if (p.mask & 0x04 && p.p3503.empty()) return false;
	if (p.mask & 0x08 && p.p3511.empty()) return false;

	// can't think of any other reason why this ping isn't ready
	// to write
	return true;
}
//-------------------------------------------------------------------------------------
// PageWriter::Policy::writePings()
//-------------------------------------------------------------------------------------
void PageWriter::Policy::writePings(const uint32_t pingNum)
{
	// search the queue looking for pings before this ping
	// that have not been written, write them out.

	// this is a brute force approach chosen for its 
	// simplcity - a more advanced algorithm would use
	// the ping iterator found for the current ping and
	// walk backwards until it found a written ping and 
	// then start writing from the one previous until 
	// the current ping is reached - bleh

	for (auto & p : *_queue)
	{
		// all done
		if (p.pingNum > pingNum) return;

		if (!p.written) 
		{
			p.write(_pw);
			p.written = true;
		}
	}
}

//-------------------------------------------------------------------------------------
// PageWriter::Policy::Ping
//  - this is a containerized object, it may be okay
//    to use all of the default ctors, but coded them
//	  anyway, primarily because of the UcBuffer and move
//	  operations
//-------------------------------------------------------------------------------------
// PageWriter::Policy::Ping ctor(pingNum, mask)
PageWriter::Policy::Ping::Ping(const uint32_t p, const uint32_t m)
	: pingNum(p), mask(m), written(false) { }
// PageWriter::Policy::Ping copy ctor
PageWriter::Policy::Ping::Ping(const PageWriter::Policy::Ping& rhs)
{
	pingNum = rhs.pingNum;
	mask = rhs.mask;
	written = rhs.written;
	p3501 = rhs.p3501;
	p3502 = rhs.p3502;
	p3503 = rhs.p3503;
	p3511 = rhs.p3511;
}
// PageWriter::Policy::Ping move ctor
PageWriter::Policy::Ping::Ping(PageWriter::Policy::Ping&& rhs)
{
	pingNum = rhs.pingNum;
	mask = rhs.mask;
	written = rhs.written;
	p3501 = rhs.p3501; 
	p3502 = rhs.p3502; 
	p3503 = rhs.p3503; 
	p3511 = rhs.p3511;
}
// PageWriter::Policy::Ping assignment operator
PageWriter::Policy::Ping& PageWriter::Policy::Ping::operator = (const PageWriter::Policy::Ping& rhs)
{
	if (this == &rhs)
		return *this;

	pingNum = rhs.pingNum;
	mask = rhs.mask;
	written = rhs.written;
	p3501 = rhs.p3501;
	p3502 = rhs.p3502;
	p3503 = rhs.p3503;
	p3511 = rhs.p3511;

	return *this;
}
// PageWriter::Policy::Ping move operator
PageWriter::Policy::Ping& PageWriter::Policy::Ping::operator = (PageWriter::Policy::Ping&& rhs)
{
	if (this == &rhs)
		return *this;

	pingNum = rhs.pingNum;
	mask = rhs.mask;
	written = rhs.written;
	p3501 = rhs.p3501;
	p3502 = rhs.p3502;
	p3503 = rhs.p3503;
	p3511 = rhs.p3511;

	return *this;
}
//-------------------------------------------------------------------------------------
// PageWriter::Policy::Ping::write()
//-------------------------------------------------------------------------------------
void PageWriter::Policy::Ping::write(PageWriter* pw)
{
	// finally a ping that is ready to write
	// this is where the page ordering is implemented, probably 
	// could be generalized
	//
	// friend function of PageWriter
	//
	if (written) return;
	if (!mask) return;

	static const uint32_t pm = 0xffffffff;

	static bool addSdfx3503 = false;
	static bool addSdfx3511 = false;

	if (pw->_numPings == 0)
	{
		addSdfx3503 = true;
		addSdfx3511 = true;
	}

	if (!p3501.empty()) 
	{
		// write marker
		pw->fileWrite((uint8_t*) &pm, sizeof(uint32_t));
		// write page
		pw->fileWrite(p3501.uc_str(), p3501.length());
	}
	if (!p3503.empty())
	{
		pw->fileWrite((uint8_t*) &pm, sizeof(uint32_t));
		if (addSdfx3503)
		{
			// no pings writen to file yet, add bathy sdfx to first 3503 record in file
			addSdfx3503 = false;
			pw->addBathySdfx(p3503);
		}
		pw->fileWrite(p3503.uc_str(), p3503.length());
	}
	if (!p3511.empty())
	{
		pw->fileWrite((uint8_t*) &pm, sizeof(uint32_t));
		if (addSdfx3511)
		{
			// no pings writen to file yet, add bathy sdfx to first 3503 record in file
			addSdfx3511 = false;
			pw->addBathySdfx(p3511);
		}
		pw->fileWrite(p3511.uc_str(), p3511.length());
	}
	if (!p3502.empty())
	{
		pw->fileWrite((uint8_t*) &pm, sizeof(uint32_t));
		pw->fileWrite(p3502.uc_str(), p3502.length());
	}

	pw->_numPings++;
	return;
}
//-------------------------------------------------------------------------------------
// helper functions
namespace klein
{
	std::ostream& operator << (std::ostream& out, const PageWriter::Policy& p)
	{
		for (auto const& pp : *(p._queue))
		{
			out << pp;
		}

		return out;
	}
	std::ostream& operator << (std::ostream& out, const PageWriter::Policy::Ping& p)
	{
		out << "Ping: " << p.pingNum
			<< ", Mask: 0x" << std::hex << std::setfill('0') << std::setw(8) << p.mask
			<< ", Written? " << std::boolalpha << p.written;
		return out;
	}
}
// operator << DiskRecordingStatus
std::ostream& operator <<(std::ostream& out, const DiskRecordingStatus& d)
{
	out << "DiskRecordingStatus:" << std::endl << "\tVersion " << d.nVersion
			<< std::endl << "\tHardDiskPercent " << d.nHardDiskPercent
			<< std::endl << "\tHardDiskFull " << d.nHardDiskFull << std::endl
			<< "\tszFileName " << d.szFileName;
	return out;
}
// operator << DiskRecordingSettings
inline std::ostream& operator <<(std::ostream& out,
		const DiskRecordingSettings& d)
{
	out << "DiskRecordingSettings:" << std::endl << "\tVersion " << d.nVersion
			<< std::endl << "\tRecordMode " << d.nRecordMode << std::endl
			<< "\tFileFormat " << d.nFileFormat << std::endl
			<< "\tPingsPerFile " << d.nPingsPerFile << std::endl << "\tNewFile "
			<< d.nNewFile << std::endl << "\tTpuDiagLevel " << d.nTpuDiagLevel
			<< std::endl << "\tFilePrefix " << d.szFilePrefix << std::endl
			<< "\tPathAction " << d.nPathAction << std::endl << "\tFilePath "
			<< d.szFilePath;
	return out;
}

