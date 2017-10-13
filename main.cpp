
#include <iostream>
#include <iomanip>
// #include <sstream>
#include <cstdlib> // exit
// #include <unistd.h>
#include <signal.h>
// #include <memory>


#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdio.h> // fopen
#include <sys/time.h> // gettimeofday

// options processing code
#include <getoptpp/getopt_pp.h>

#include "Recorder.h"

const char* const _id = "$Id: //TPU-4XXX-Stream/2.13/Recorder/main.cpp#3 $";



namespace klein
{

bool shutdown = false;

// klein::printTime()
std::ostream& printTime(std::ostream& os)
{
	// a function to print the curent time to ostream
	static char buf[255];
	static const char format[] = "%X";
	struct timeval tv;
	struct timezone tz;

	// ignore return value
	(void) gettimeofday(&tv, &tz);

	const struct tm* const tmp = localtime(&tv.tv_sec);
	
	if (tmp == NULL)
	{
		throw strerror(errno);
	} 
	if (strftime(buf, sizeof(buf), format, tmp) == 0)
		throw "strftime failure";

	os << buf << "." << std::setw(3) << std::setfill('0') << (int)(tv.tv_usec/ 1e3);
	return os;
}

// klein::printError()
std::ostream& printError(TPU_HANDLE tpu, std::ostream& os)
{
	DLLErrorCode ec = NGS_NO_ERROR;

	DllGetLastError(tpu, &ec);

	switch(ec)
	{
		case NGS_NO_ERROR:
			os << "No Error";
			break;
		case NGS_NO_NETWORK_SOCKET_OBJECT:
			os << "No Socket";
			break;
		case NGS_NO_CONNECTION_WITH_TPU:
			os << "No Connection";
			break;
		case NGS_ALREADY_CONNECTED:
			os << "Already Connected";
			break;
		case NGS_INVALID_IP_ADDRESS:
			os << "Invalid IpAddr";
			break;
		case NGS_REQUIRES_A_MASTER_CONNECTION:
			os << "Requres Master";
			break;
		case NGS_MASTER_ALREADY_CONNECTED:
			os << "Already Master Connected";
			break;
		case NGS_GETHOSTBYNAME_ERROR:
			os << "GetHostByName Error";
			break;
		case NGS_COMMAND_HANDSHAKE_ERROR:
			os << "Command Handshake Error";
			break;
		case NGS_COMMAND_NOT_SUPPORTTED_BY_CURRENT_PROTOCOL:
			os << "Command Not Supported";
			break;
		case NGS_SEND_COMMAND_FAILURE:
			os << "Send Failure";
			break;
		case NGS_RECEIVE_COMMAND_FAILURE:
			os << "Receive Failure";
			break;
		case NGS_TPU_REPORTS_COMMAND_FAILED:
			os << "TPU Reports Failure";
			break;
		case NGS_UNKNOWN_DATA_PAGE_VERSION:
			os << "Unknown Data Page Version";
			break;
		case NGS_SDFX_RECORD_TYPE_UNKNOWN:
			os << "Unknown SDFX Type";
			break;
		case NGS_SDFX_RECEIVE_BUFFER_TOO_SMALL:
			os << "SDRX Receive Buffer To Small";
			break;
		case NGS_SDFX_HEADER_VERSION_UNKNOWN:
			os << "SDFX Header Version Unknown";
			break;
		case NGS_SDFX_RECORD_VERSION_UNKNOWN:
			os << "SDFX Record Version Unknown";
			break;
		default:
			os << "Unknown Error Code: " << ec;
			break;
	}
	return os;
}

bool operator == (const DiskRecordingSettings& lhs, const DiskRecordingSettings& rhs)
{
	bool ret = false;
	if (lhs.nVersion != rhs.nVersion) return ret;
	if (lhs.nRecordMode != rhs.nRecordMode) return ret;
	if (lhs.nFileFormat != rhs.nFileFormat) return ret;
	if (lhs.nPingsPerFile != rhs.nPingsPerFile) return ret;
	if (lhs.nNewFile != rhs.nNewFile) return ret;
	if (lhs.nTpuDiagLevel != rhs.nTpuDiagLevel) return ret;
	if (lhs.nPathAction != rhs.nPathAction) return ret;

	if (strcmp(lhs.szFilePrefix, rhs.szFilePrefix)) return ret;
	if (strcmp(lhs.szFilePath, rhs.szFilePath)) return ret;
	
	return true;
}

} // namespace klein

// ^C handler
static void terminate(const int param)
{
	if (klein::shutdown)
	{
		std::cerr << "shutdown failed" << std::endl;
		exit(-1);
	}
	klein::shutdown = true;
}

// usage()
static void usage(const std::string& name)
{
	std::cerr << "Usage: " << name
		<< "[-h --host hostname]"
		<< "[-n --noblocking | -b --blocking]" 
		<< std::endl;
	std::cerr << "\tdefault: -h 127.0.0.1 --blocking" << std::endl;
}

// the main()
int main(const int ac, const char* const av[])
{
	// map ^C handler
 	(void) signal(SIGINT, terminate);

	GetOpt::GetOpt_pp ops(ac, av);

	ops.exceptions(std::ios::failbit | std::ios::eofbit);

	// std::string hostname("192.168.0.81");
	std::string hostname("127.0.0.1");
	bool useBlocking = true;
	bool useNoBlocking = !useBlocking;

	try
	{
		ops >> GetOpt::Option('h', "host", hostname, hostname)
			>> GetOpt::OptionPresent('n', "noblocking", useNoBlocking)
			>> GetOpt::OptionPresent('b', "blocking", useBlocking);

		// both set is error
		if (useNoBlocking && useBlocking)
		{	
			usage(av[0]);
			return -1;
		}

		// if neither was set, reset to default
		if (!useNoBlocking && !useBlocking) useBlocking = true;
	}
	catch (const GetOpt::GetOptEx& ex)
	{
		std::cerr << "caught: " << ex.what() << std::endl;
		usage(av[0]);
		return -1;
	}

	std::cout << "Using blocking sockets: " << std::boolalpha << useBlocking
		<< " with SPU: " <<  hostname << std::endl;

	klein::Recorder recorder(hostname, useBlocking);

	std::cout << "recorder.execute returns: " << recorder.execute() << std::endl;

	return 0;
}
