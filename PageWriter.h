#ifndef _KLEIN_PAGE_WRITER_H_
#define _KLEIN_PAGE_WRITER_H_

// 
// $Id: //TPU-4XXX-Stream/2.13/Recorder/PageWriter.h#1 $
//

#include <ostream>
#include <stdint.h>
#include <stdio.h> // FILE*

#include <memory>
#include <boost/circular_buffer.hpp>

#include "UcBuffer.h"

#include "KleinSonar.h"
#include "Recorder.h"


namespace klein
{

// 'final' class - otherwise change dtor to virtual
class PageWriter
{

	public:

	class Policy
	{
		public:
		// this policy implements an algorithm where the pages for
		// each ping are accumulated. Every time a new page is added
		// the ping it was added to is checked to see if it is ready to
		// write.
		//
		// what happens when a page is lost in the ether? well, the policy
		// will write any prior pings that have not been written when a ping
		// is ready to write
			class Ping
			{
				public:
					Ping(uint32_t p, uint32_t m);
					Ping(const Ping& rhs);
					Ping& operator = (const Ping& rhs);
					Ping(Ping&& rhs);
					Ping& operator = (Ping&& rhs);

					~Ping() {}

					void write(PageWriter* pw);

					uint32_t pingNum;
					uint32_t mask;
					bool written;

					klein::UcBuffer p3501;
					klein::UcBuffer p3502;
					klein::UcBuffer p3503;
					klein::UcBuffer p3511;
			};

			Policy(PageWriter* pw);
			~Policy() {};

			void writePage(const uint8_t* p, const size_t n);

		private:
			// pointer to parent for callbacks
			PageWriter* _pw;

			typedef boost::circular_buffer<PageWriter::Policy::Ping> PingQueue_t;
			std::unique_ptr<PingQueue_t> _queue;

			bool pingReadyForWrite(const Ping& p);
			void writePings(const uint32_t pingNum);

		public:

			inline PingQueue_t::iterator findPing(const uint32_t n)
			{
				return std::find_if(_queue->begin(), _queue->end(),
						[n] (const Ping& p) -> bool
						{ return p.pingNum == n; });
			}

		friend std::ostream& operator << (std::ostream& out, const Policy& p);
		friend std::ostream& operator << (std::ostream& out, const Policy::Ping& p);

	};

		PageWriter(Recorder& r);
		~PageWriter();
	
		void writePage(const uint8_t* p, const size_t n);
		void update();
		void flush();
		inline bool record() { return _settings.nRecordMode; }
		uint32_t getFramingMode();

	private:
		// no copy or operator = ctors
		PageWriter(const PageWriter& rhs);
		PageWriter operator = (const PageWriter& rhs);

		float getDiskUsedPercent();

		void openNewDataFile(const CKleinType3Header* h, const U32 ff = 0);
		void openDataFile();
		void closeDataFile();
		void fileWrite(const uint8_t* p, const size_t n);
		void fileWriteForReal();

		// this probably should be done by the Bathy process...
		void addBathySdfx(klein::UcBuffer& p);

		// data structures from the Klein SDK
		DiskRecordingStatus _status;
		DiskRecordingSettings _settings;

		Recorder& recorder;
		FILE* _fp;
		uint32_t _cacheBytes;
		uint8_t* _cache;
		uint8_t* _cachePtr;
		uint32_t _fileSize;
		uint32_t _numPings;

		char _filename[PATH_MAX+128];

		// Policy _policy;
		std::unique_ptr<Policy> _policy;

		static const int pingWriteInterval;
		static const int cacheSize;

	friend std::ostream& operator << (std::ostream& out, const PageWriter& p);
	friend void PageWriter::Policy::Ping::write(PageWriter* pw);
};

} // namespace klein

extern std::ostream& operator << (std::ostream& out, 
		const DiskRecordingStatus& p);
extern std::ostream& operator << (std::ostream& out,
		const DiskRecordingSettings& p);

#endif //_KLEIN_PAGE_WRITER_H_ 

