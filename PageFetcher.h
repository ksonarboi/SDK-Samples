#ifndef _KLEIN_PAGEFETCHER_H_
#define _KLEIN_PAGEFETCHER_H_

//
// $Id: //TPU-4XXX-Stream/2.13/Recorder/PageFetcher.h#1 $
//

#include <ostream>

#include "Recorder.h"

namespace klein
{

class PageFetcher
{
public:

	PageFetcher(Recorder& r, const int pt) :  
		recorder(r), pageType(pt), lastPingNum(-1), buffer(NULL), bufSize(0) { }

	virtual ~PageFetcher();

	bool fetchPage();

	void writePage();

	inline void reset() { lastPingNum = -1; }

private:
	Recorder& recorder;
	int pageType;
	int lastPingNum;
	unsigned char* buffer;
	size_t bufSize;

	friend std::ostream& operator << (std::ostream& out, const PageFetcher& pf);

};

} // namespace klein
#endif // _KLEIN_PAGEFETCHER_H_
