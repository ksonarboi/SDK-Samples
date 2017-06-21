#ifndef _KLEIN_RECORDER_H
#define _KLEIN_RECORDER_H

//
// $Id: //TPU-4XXX-Stream/2.13/Recorder/Recorder.h#1 $
//

#include <ostream>
#include <string>

#include "KleinSonar.h"


namespace klein
{

class PageWriter;

class Recorder 
{

public:

	Recorder(const std::string& spu, const bool& bs) : 
		_tpuHandle(NULL), _spuIP(spu), _useBlockingSockets(bs),
		_startFetchTime_nsec(0), _pageWriter(NULL) {}

	virtual ~Recorder(void);

	const int execute();

	inline TPU_HANDLE tpuHandle() { return _tpuHandle; }

	void checkStatus(const BoolStat status);

	void writePage(const uint8_t* p, const size_t n);

private:

	void connectToTPU();
	void disconnectFromTPU();
	void setStartTime();
	void nap();

	TPU_HANDLE _tpuHandle;
	const std::string _spuIP;
	const bool _useBlockingSockets;
	long _startFetchTime_nsec;

	klein::PageWriter* _pageWriter;

	friend std::ostream& operator << (std::ostream& out, const Recorder& s);
};

} // namespace klein
#endif // _KLEIN_RECORDER_H
