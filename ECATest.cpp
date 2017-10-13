// sample program that demonstrates how a UUV3500 might be controlled
// The purpose is to set up the Sonar Aquisition, Bathy Processing, and
// Data Recording for the UUV3500 CM3-BT4 Series Card Stack.
//
// some C++11 may have creeped in
//
const char* const _id = "$Id: ECATest.cpp,v 1.6 2017/03/29 18:04:05 klein Exp $";

#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>

#include <cstdlib>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h> // fopen
#include <string.h>
#include <unistd.h>

#include <sys/time.h> // gettimeofday

// from google code
#include <getoptpp/getopt_pp.h>

// Klein SDK
#include "KleinSonar.h"
#include "sdfx_types.h"

// set when signalled
static bool shutdown = false;

// a function to print the curent time to ostream
static std::ostream& printTime(std::ostream& os)
{
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

// print klein errror to ostream
static std::ostream& printError(TPU_HANDLE tpu, std::ostream& os)
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

// dump a bathy cal file
std::ostream& operator << (std::ostream& out, const KLEIN_BATHY_CAL_1& b)
{
  out << "Bathy Cal 1: " << std::endl
    // << "  Record Header: " << b.recordHeader << std::endl
    << "  System identification value: " << b.systemId << std::endl            
    << "  Port Transducer ID: " << b.transducerPortId << std::endl        
    << "  Starboard Transducer ID: " << b.transducerStbdId << std::endl        
    << "  The number of Port bathy transducers: " << b.transducerCountPort << std::endl      
    << "  The number of Stbd bathy transducers: " << b.transducerCountStbd << std::endl      
    << "  Enumerated value defining transducer layout, e.g., vertical.  Reserved for future use: " 
      << b.transducerOrientation << std::endl      
    << "  Port bathy transducer 1 distance from bathy channel 1 (wavelength): " << b.transducerSpacingPort1 << std::endl      
    << "  Port bathy transducer 2 distance from bathy channel 1 (wavelength): " << b.transducerSpacingPort2 << std::endl      
    << "  Port bathy transducer 3 distance from bathy channel 1 (wavelength): " << b.transducerSpacingPort3 << std::endl      
    << "  Port bathy transducer 4 distance from bathy channel 1 (wavelength): " << b.transducerSpacingPort4 << std::endl      
    << "  Starboard bathy transducer 1 distance from bathy channel 1 (wavelength): " << b.transducerSpacingStbd1 << std::endl      
    << "  Starboard bathy transducer 2 distance from bathy channel 1 (wavelength): " << b.transducerSpacingStbd2 << std::endl      
    << "  Starboard bathy transducer 3 distance from bathy channel 1 (wavelength): " << b.transducerSpacingStbd3 << std::endl      
    << "  Starboard bathy transducer 4 distance from bathy channel 1 (wavelength): " << b.transducerSpacingStbd4 << std::endl      
    << "  Port channel 1 electronix/transducer phase correction (radians): " << b.phaseCorrectionPort1 << std::endl      
    << "  Port channel 2 electronix/transducer phase correction (radians): " << b.phaseCorrectionPort2 << std::endl      
    << "  Port channel 3 electronix/transducer phase correction (radians): " << b.phaseCorrectionPort3 << std::endl      
    << "  Port channel 4 electronix/transducer phase correction (radians): " << b.phaseCorrectionPort4 << std::endl      
    << "  Starboard channel 1 electronix/transducer phase correction (radians): " << b.phaseCorrectionStbd1 << std::endl      
    << "  Starboard channel 2 electronix/transducer phase correction (radians): " << b.phaseCorrectionStbd2 << std::endl      
    << "  Starboard channel 3 electronix/transducer phase correction (radians): " << b.phaseCorrectionStbd3 << std::endl      
    << "  Starboard channel 4 electronix/transducer phase correction (radians): " << b.phaseCorrectionStbd4 << std::endl      
    << "  Port Array depression angle (from horizontal, degrees): " << b.depressionAnglePort << std::endl      
    << "  Starboard Array depression angle (from horizontal, degrees): " << b.depressionAngleStbd << std::endl;
  return out;
}

// dump bathy engineering settings
std::ostream& operator << (std::ostream& out, const KLEIN_BATHY_ENG_SETTINGS_1& b)
{
  out << "Bathy Eng 1: " << std::endl
    // << "  Record Header: " << b.recordHeader << std::endl
    << "  dcBiasAlpha: " << b.dcBiasAlpha << std::endl
    << "  dcBiasMeterSat: " << b.dcBiasMeterSat << std::endl
    << "  dcBiasMeterSbb: " << b.dcBiasMeterSbb << std::endl
    << "  gainAlpha: " << b.gainAlpha << std::endl
    << "  gainTpWin: " << b.gainTpWin << std::endl
    << "  boxcarMaxTap: " << b.boxcarMaxTap << std::endl
    << "  boxcarSlope: " << b.boxcarSlope << std::endl
    << "  boxcarOffset: " << b.boxcarOffset << std::endl
    << "  phasefiltMaxTap: " << b.phasefiltMaxTap << std::endl
    << "  phasefiltSlope: " << b.phasefiltSlope << std::endl
    << "  phasefiltOffset: " << b.phasefiltOffset << std::endl
    << "  minphaseangAngle: " << b.minphaseangAngle << std::endl
    << "  rangegateGateWidth: " << b.rangegateGateWidth << std::endl;
  return out;
}

// dump bathy process settings
std::ostream& operator << (std::ostream& out, const KLEIN_BATHY_PROC_SETTINGS_3& b)
{
  out << "Bathy Processed 3: " << std::endl
    // << "  Record Header: " << b.recordHeader << std::endl
    << "\tDivide bathy intensity vectors by this value to get intensity (magnitude): " << b.bathyScaleIntensity << std::endl    
    << "\tDivide bathy Angle vectors by this value to get radians: " << b.bathyScaleAngle << std::endl      
    << "\tDivide bathy Quality1 vector values by this value to get value between 0 and 1: " << b.bathyScaleQuality1 << std::endl
    << "\tDivide bathy Quality2 vector values by this value to get value between 0 and 1: " << b.bathyScaleQuality2 << std::endl      
    << "\tDivide Uncertainty vectors by this value to get uncertainty: " << b.bathyScaleUncertainty << std::endl    
    << "\tDivide SNR vector values by this value to get SNR in dB: " << b.bathyScaleSNR << std::endl    
    << "\tDivide bathy X, Y and Z values by this value to get meters: " << b.bathyScaleXyz << std::endl        
    << "\tDivide bathy roll vector by this value to get radians: " << b.bathyScaleRoll << std::endl        
    << "\tDivide bathy pitch vector by this value to get radians: " << b.bathyScalePitch << std::endl      
    << "\tDivide bathy heave vector by this value to get meters: " << b.bathyScaleHeave << std::endl      
    << "\tMultiply bathy sound speed correction vectors by this value to get meters: " << 
      b.bathyScaleSoundSpeedCorrection << std::endl    
    << "\tWind speed for uncertainty calculations: " << b.uncWindSpeedSel << std::endl
    << "\tGrain size for uncertainty calculations: " << b.uncGrainSizeSel << std::endl
    << "\tType of motion correction applied to bathy output: " << b.bathyMotionType;

  switch (b.bathyMotionType)
  {
    case 0:
    default:
      out << " None" << std::endl;
      break;
    case 1:
      out << " Towfish compass" << std::endl;
      break;
    case 2:
      out << " Auxiliary sensor" << std::endl;
      break;
    case 3:
      out << " Klein Motion Sensor" << std::endl;
      break;
    case 4:
      out << " POS MV" << std::endl;
      break;
    case 5:
      out << " F180" << std::endl;
      break;
    case 6:
      out << " DMS" << std::endl;
      break;
    case 7:
      out << " Octans" << std::endl;
      break;
  }

  out << "\tType of sound speed correction applied: " <<  b.bathySoundSpeedType;    
  switch (b.bathySoundSpeedType)
  {
    case 0:
    default:
      out << " None" << std::endl;
      break;
    case 1:
      out << " Array Sound Speed" << std::endl;
    case 2:
      out << " Sound Velocity Profile" << std::endl;
    case 3:
      out << " Array Sound Speed and Sound Velocity Profile" << std::endl;
  }

  out << "\tProcessing optimization utilized: " <<  b.bathyProcessingOptimizations;  
  if (b.bathyProcessingOptimizations)
    out << " Normal(Fast) Search" << std::endl;
  else 
    out << " Exhaustive(Extended) Search" << std::endl;
  return out;
}

// usage()
static void usage(const std::string& name)
{
  std::cerr << "Usage: " << name
    << "[-h --host hostname]"
    << "[-c --calfile filename]" 
    << std::endl;
  std::cerr << "\tdefault: -h 192.168.0.81 --calfile towfish.cal" << std::endl;
}

//
// The SPUStatusInterface, of which there is one, will wait until the TPU accepts its connection attempt
// and then will setup the sonar aquisition, bathy processing, and data recorder, then exit.
// It is constructed from main with arguments of the server and bathy cal file.
//
class SPUStatusInterface 
{

public:

  SPUStatusInterface(const std::string& spu, const std::string& cf) : 
    m_tpuHandle(NULL), m_spuIP(spu), m_calFile(cf)
  {}

  virtual ~SPUStatusInterface(void)
  {
    if (m_tpuHandle);
    {
      DllCloseTheTpu(m_tpuHandle);
    }
  }

  const int execute()
  {
    // connect - can block
    connectToTPU();

    // make sure we are standby
    standby(true);

    // set sonar up - needs to be master
    if (!setupSonar())
    {
        printTime(std::cerr);
        std::cerr << "- Sonar setup failed" << std::endl;
    }

    // set bathy up - needs to be master
    if (!setupBathy())
    {
        printTime(std::cerr);
        std::cerr << "- Bathy setup failed" << std::endl;
    }

    // set recorder up - needs to be master
    if (!setupRecorder())
    {
        printTime(std::cerr);
        std::cerr << "- Recorder setup failed" << std::endl;
    }

    // take out of standby
    standby(false);

    // done
    disconnectFromTPU();

    return 0;
  }

  inline TPU_HANDLE tpuHandle() { return m_tpuHandle; }

private:

  void connectToTPU()
  {
    DLLErrorCode errorCode = NGS_NO_CONNECTION_WITH_TPU;

    while (errorCode != NGS_NO_ERROR && errorCode != NGS_ALREADY_CONNECTED && !shutdown)
    {
      U32 protocolVersion = 0;

      // to use 'set' methods we need to be master, only 1 master per tpu
      U32 config = S5KCONF_MASTER;
      // otherwise slave
      // U32 config = 0;

      // cast away const :(
      m_tpuHandle = DllOpenTheTpu(config, (char *)m_spuIP.c_str(), &protocolVersion);

      DllGetLastError(m_tpuHandle, &errorCode);

      switch(errorCode)
      {
        case NGS_NO_ERROR:
        case NGS_ALREADY_CONNECTED:
          // break while loop and lower alarm
          goto lowerAlarm;
          break;
        default:
          {
            std::ostringstream os;
            printTime(os); os << " - OpenTheTpu() - "; printError(m_tpuHandle, os);

            std::cerr << os.str() << std::endl;
          }
          // need to free the TPUHandle on failure
          disconnectFromTPU();
          break;
      }

      { printTime(std::cerr); std::cerr << " - Alarm raised " << std::endl; usleep(1e6); }
    } // while 

lowerAlarm:
    { printTime(std::cerr); std::cerr << " - Alarm lowered " << std::endl; usleep(1e6); }

  }

  void disconnectFromTPU()
  {
    if (m_tpuHandle == NULL)  //Nothing to do, already disconnected
      return;

    try
    {
      DllCloseTheTpu(m_tpuHandle);
    }
    catch (...)
    {
      std::cerr << "Connection to the TPU was not properly closed." << std::endl;
    }

    m_tpuHandle = NULL;  
  }

  bool setupSonar()
  {
    bool pause = false;

    // get standby state as it will need to be turned off and restored
    {
      U32 mode = 0; // 0=on, 1=off

      if ((NGS_SUCCESS != DllGetTheTpuStandby(m_tpuHandle, &mode)))
      {
        std::ostringstream os; printTime(os); os << " - Could not GetTheTpuStandby() - " ; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      pause = (mode == 1) ? true : false;

      // if not paused, then standby
      if (!pause) standby(true);

    }
    // set the clock to our (host) time
    {
      if (NGS_SUCCESS != DllSetTheTpuClock(m_tpuHandle))
      {
        std::ostringstream os; 
        printTime(os); os << " - Could not SetTheTpuClock() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }
      else
      {
        std::ostringstream os; 
        printTime(os); os << " - SetTheTpuClock() - successful";
      }
    }
    // get framing mode, set framing mode
    {
      // get
      U32 mode = 0; 
      if ((NGS_SUCCESS != DllGetTheTpuFramingMode(m_tpuHandle, &mode)))
      {
        std::ostringstream os; printTime(os); os << " - Could not GetTheTpuFramingMode() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      // change mode to HF, LF and Bathy
      const uint32_t newMode = 7;

      // set
      if ((NGS_SUCCESS != DllSetTheTpuFramingMode(m_tpuHandle, newMode)))
      {
        std::ostringstream os; printTime(os); os << " - Could not SetTheTpuFramingMode() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      // debug
      {
        std::ostringstream os; printTime(os); os << " - SetTheTpuFramingMode() to " << newMode << " from " << mode;
        std::cout << os.str() << std::endl;
      }
    }
    // get range, set range
    {
      // get
      U32 range = 0; 
      if ((NGS_SUCCESS != DllGetTheTpuRange(m_tpuHandle, &range)))
      {
        std::ostringstream os; printTime(os); os << " - Could not GetTheTpuRange() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      // change range no matter what
      // const U32 newRange = 30; // default pl is lf=2ms, hf=1ms
      // const U32 newRange = 50; // default pl is lf=2ms, hf=2ms
      const U32 newRange = 75; // default pl is lf=2ms, hf=4ms // best HF range?
      // --- 75m maybe change pl to match?
      // const U32 newRange = 100; // default pl is lf=2ms, hf=4ms
      // --- 100m maybe change pl to match?
      // const U32 newRange = 125; // default pl is lf=8ms, hf=8ms // best Bathy range?
      // const U32 newRange = 150; // default pw is lf=8ms, hf=8ms // best LF range?
      // const U32 newRange = 200; // default pl is lf=8ms, hf=8ms

      // set
      if ((NGS_SUCCESS != DllSetTheTpuRange(m_tpuHandle, newRange)))
      {
        std::ostringstream os; printTime(os); os << " - Could not SetTheTpuRange() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      // debug
      {
        std::ostringstream os; printTime(os); os << " - SetTheTpuRange() to "
          << newRange << " meters from " << range << " meters";
        std::cout << os.str() << std::endl;
      }
    }
    // get pulse length, set pulse length (tx on)
    {
      // pulse length is really bit field of useful info, lets call it a waveform 
      // defined as  
      //     bit 15 - HFTX
      //     bits 14-8 - HF Pulse Length Table Index
      //     bit 7 - LFTX
      //     bits 6-0 - LF Pulse Length Table Index
      //
      // PL Table for UUV3500 is [1, 2, 4, 8];

      // get waveform
      U32  waveform = 0; 
      if ((NGS_SUCCESS != DllGetTheTpuPulseLength(m_tpuHandle, &waveform)))
      {
        std::ostringstream os; printTime(os); os << " - Could not GetTheTpuPulseLength() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      // decode waveform 
      const bool hftx = (waveform & 0x8000);
      const bool lftx = (waveform & 0x0080);

      const uint32_t hfwf = ((waveform & 0x7700) >> 8);
      const uint32_t lfwf = (waveform & 0x0077);

      // debug
      {
        std::ostringstream os;
        printTime(os);
        os << " - Waveform: 0x" << std::setfill('0') << std::setw(4) << std::hex << waveform
          << ", HF TX: " << std::boolalpha << hftx
          << ", HF WF: 0x" << std::setfill('0') << std::setw(4) << std::hex << hfwf
          << ", LF TX: " << std::boolalpha << lftx
          << ", LF WF: 0x" << std::setfill('0') << std::setw(4) << std::hex << lfwf
          << std::dec;
        std::cout << os.str() << std::endl;
      }

      // keep wf, enable tx for both
      if (!hftx) waveform |= 0x8000;
      if (!lftx) waveform |= 0x0080;

      if ((NGS_SUCCESS != DllSetTheTpuPulseLength(m_tpuHandle, waveform)))
      {
        std::ostringstream os; printTime(os); 
        os << " - Could not SetTheTpuPulseLength() to 0x" 
          << std::setfill('0') << std::setw(4) << std::hex << waveform
          << " - "; 
        printError(m_tpuHandle, os);

        throw os.str().c_str();
      }

      {
        std::ostringstream os; printTime(os); 
        os << " - SetTheTpuPulseLength() to 0x" 
          << std::setfill('0') << std::setw(4) << std::hex << waveform;
        std::cout << os.str() << std::endl;
      }

      // dump if changed
      if (!hftx || !lftx)
      {
        std::ostringstream os;
        printTime(os);
        os << " - New Waveform: 0x" << std::setfill('0') << std::setw(4) << std::hex << waveform
          << ", HF TX: " << std::boolalpha << (waveform & 0x8000)
          << ", HF WF: 0x" << std::setfill('0') << std::setw(4) << std::hex << hfwf
          << ", LF TX: " << std::boolalpha << (waveform * 0x0080)
          << ", LF WF: 0x" << std::setfill('0') << std::setw(4) << std::hex << lfwf
          << std::dec;
        std::cout << os.str() << std::endl;
      }
    }
    // make sure low-res - always when bathy
    {
      // get
      U32 resolution = 0; 
      if ((NGS_SUCCESS != DllGetTheTpuResolutionMode(m_tpuHandle, &resolution)))
      {
        std::ostringstream os; printTime(os); os << " - Could not GetTheTpuResolutionMode() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      // change resolution to 0
      const U32 newResolution = 0;

      if (newResolution != resolution)
      {
        // set
        if ((NGS_SUCCESS != DllSetTheTpuResolutionMode(m_tpuHandle, newResolution)))
        {
          std::ostringstream os; printTime(os); os << " - Could not SetTheTpuResolutionMode() - "; printError(m_tpuHandle, os);
          throw os.str().c_str();
        }

        // debug
        {
          std::ostringstream os; printTime(os); os << " - SetTheTpuResolutionMode() to "
            << newResolution << ", from " << resolution;
          std::cout << os.str() << std::endl;
        }
      }
    }
    // set RX gain on
    {
      // get
      U32 gain = 0; 
      if ((NGS_SUCCESS != DllGetTheTpuGainOffset(m_tpuHandle, &gain)))
      {
        std::ostringstream os; printTime(os); os << " - Could not GetTheTpuGainOffset() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      // make sure gain is on
      const U32 newGain = 1;

      if (newGain != gain)
      {
        // set
        if ((NGS_SUCCESS != DllSetTheTpuGainOffset(m_tpuHandle, newGain)))
        {
          std::ostringstream os; printTime(os); os << " - Could not SetTheTpuGainOffset() - "; printError(m_tpuHandle, os);
          throw os.str().c_str();
        }

        // debug
        {
          std::ostringstream os; printTime(os); os << " - SetTheTpuGainOffset() to " 
            << newGain << ", from " << gain;
          std::cout << os.str() << std::endl;
        }
      }
    }

    // turn standby off (if it was on)
    if (!pause) standby(false);

    return true;
  }

  bool setupBathy()
  {
    if (!setupBathyCal()) return false;

    if (!setupBathyProc()) return false;

    if (!setupBathyEng()) return false;

    return true;
  }

  bool setupBathyCal()
  {
    // cal file
    // change settings as necessary
      // figure out a buffer size for the cal file to be read into
    U32 size = 0;
    if (NGS_SUCCESS != DllGetTheSdfxRecordSize(m_tpuHandle, SDFX_RECORD_ID_BATHY_CAL_1, &size))
    {
      std::ostringstream os; printTime(os); 
      os << " - Could not GetTheSdfxRecordSize(BATHY_CAL) - "; printError(m_tpuHandle, os);
      throw os.str().c_str();
    }

    // read cal file into buffer of size and set
    {
      uint8_t buffer[size];
      memset(buffer, 0, size);

      // read from the cal file
      {
        FILE* fp = fopen(m_calFile.c_str(), "rb");
        if (!fp)
        {
          std::ostringstream os; 
          printTime(os); 
          os << " - Error on fopen(" << m_calFile << ")"
            << strerror(errno);
          throw os.str();
        }
        
        const size_t n = fread(buffer, 1, size, fp);
        if (n != size)
        {
          std::ostringstream os;
          printTime(os); 
          os << " - Error on fread(" << n << ")"
              << " doesn't match expected size: " << size;
          fclose(fp);
          throw os.str();
        }

        fclose(fp);

        // debug dump
        if (0)
        {
          KLEIN_BATHY_CAL_1* p = reinterpret_cast<KLEIN_BATHY_CAL_1*>(buffer);;
          std::cout << *p << std::endl;
        }

        // write the record back
        if (NGS_SUCCESS != DllSetTheSdfxRecord(m_tpuHandle, buffer))
        {
          std::ostringstream os; printTime(os); 
          os << " - Could not SetTheSdfxRecord(BATHY_CAL) - "; printError(m_tpuHandle, os);
          throw os.str().c_str();
        }

        {
          std::ostringstream os;
          printTime(os); os << " - SetTheSdfxRecord(BATHY_CAL) - successful";
          std::cout << os.str() << std::endl;
        }

        // debug
        // std::cout << "New " << *p << std::endl;

        return true;
      }
    }
    return false;
  }

  bool setupBathyProc()
  {
    // tweak proc settings
    U32 size = 0;
    if (NGS_SUCCESS != DllGetTheSdfxRecordSize(m_tpuHandle, 
          SDFX_RECORD_ID_BATHY_PROC_SETTINGS_1, &size))
    {
      std::ostringstream os; printTime(os); 
      os << " - Could not GetTheSdfxRecordSize(BATHY_PROC_SETTINGS) - "; 
      printError(m_tpuHandle, os);
      throw os.str().c_str();
    }

    // allocate a buffer, get, tweak, send
    {
     uint8_t buffer[size];
     memset(buffer, 0, size);

     if (NGS_SUCCESS != DllGetTheSdfxRecord(m_tpuHandle, 
           SDFX_RECORD_ID_BATHY_PROC_SETTINGS_1, buffer, size))
     {
        std::ostringstream os; printTime(os); 
        os << " - Could not GetTheSdfxRecord(BATHY_PROC_SETTINGS) - "; 
        printError(m_tpuHandle, os);
        throw os.str().c_str();
     }
      
     {
        KLEIN_BATHY_PROC_SETTINGS_3* p = reinterpret_cast<KLEIN_BATHY_PROC_SETTINGS_3*>(buffer);

        // std::cout << *p << std::endl;

        // these values are not set by default and need to be set by the client

        // mod settings
        p->bathyScaleIntensity = 1.0;
        p->bathyScaleAngle = 10000.0;
        p->bathyScaleQuality1 = 10000.0;
        p->bathyScaleQuality2 = 10000.0;
        p->bathyScaleUncertainty = 100000.0;
        p->bathyScaleSNR = 100.0;
        p->bathyScaleXyz = 100.0;
        p->bathyScaleRoll = 10000.0;
        p->bathyScalePitch = 10000.0;
        p->bathyScaleHeave = 100.0;
        p->bathyScaleSoundSpeedCorrection = 0.0;
        // p->reserved1;          /* Reserved for future use */
        // p->uncWindSpeedSel = 1;
        p->uncWindSpeedSel = 0;
        p->uncGrainSizeSel = 0;
        // p->bathyMotionType = 3;
        p->bathyMotionType = 0;
        p->bathySoundSpeedType = 0;
        p->bathyProcessingOptimizations = 1;

        // write the record back
        if (NGS_SUCCESS != DllSetTheSdfxRecord(m_tpuHandle, buffer))
        {
          std::ostringstream os; printTime(os); 
          os << " - Could not SetTheSdfxRecord(BATHY_PROC_SETTINGS) - "; 
          printError(m_tpuHandle, os);
          throw os.str().c_str();
        }

        // std::cout << "New " << *p << std::endl;

        {
          std::ostringstream os;
          printTime(os); os << " - SetTheSdfxRecord(BATHY_PROC_SETTINGS) - successful";
          std::cout << os.str() << std::endl;
        }
        return true;
      }
    }
    return false;
     
  }

  bool setupBathyEng()
  {
    // tweak eng settings
    U32 size = 0;
    if (NGS_SUCCESS != DllGetTheSdfxRecordSize(m_tpuHandle, SDFX_RECORD_ID_BATHY_ENG_SETTINGS_1, &size))
    {
      std::ostringstream os; printTime(os); 
      os << " - Could not GetTheSdfxRecordSize(BATHY_ENG_SETTINGS) - "; 
      printError(m_tpuHandle, os);
      throw os.str().c_str();
    }

    // allocate a buffer, get, tweak, send
    {
      uint8_t buffer[size];
      memset(buffer, 0, size);

      if (NGS_SUCCESS != DllGetTheSdfxRecord(m_tpuHandle, SDFX_RECORD_ID_BATHY_ENG_SETTINGS_1, buffer, size))
      {
        std::ostringstream os; printTime(os); 
        os << " - Could not GetTheSdfxRecord(BATHY_ENG_SETTINGS) - "; 
        printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      {
        KLEIN_BATHY_ENG_SETTINGS_1* p = reinterpret_cast<KLEIN_BATHY_ENG_SETTINGS_1*>(buffer);;
        // std::cout << *p << std::endl;

        // these values are not set by default and need to be set by the client
        //
        // make mods
        p->dcBiasAlpha = 0.1;
        p->dcBiasMeterSat = 3.3;
        p->dcBiasMeterSbb = 1.0;
        p->gainAlpha = 0.1;
        p->gainTpWin = 256.0;
        p->boxcarMaxTap = 100.0;
        p->boxcarSlope = 0.023;
        p->boxcarOffset = 7.0;
        p->phasefiltMaxTap = 0.0;
        p->phasefiltSlope = 0.0;
        p->phasefiltOffset = 0.0;
        p->minphaseangAngle = 30.0;
        p->rangegateGateWidth = 1.0;

        // write the record back
        if (NGS_SUCCESS != DllSetTheSdfxRecord(m_tpuHandle, buffer))
        {
          std::ostringstream os; printTime(os); 
          os << " - Could not SetTheSdfxRecord(BATHY_ENG_SETTINGS) - "; 
          printError(m_tpuHandle, os);
          throw os.str().c_str();
        }
        {
          std::ostringstream os;
          printTime(os); os << " - SetTheSdfxRecord(BATHY_ENG_SETTINGS) - successful";
          std::cout << os.str() << std::endl;
        }      
        // std::cout << "New " << *p << std::endl;

        // only successful return
        return true;
      }
    }
    return false;
  }

  bool setupRecorder()
  {
    bool active = false;

    // get recorder mode as it will need to be turned off and restored
    {
      U32 mode = 0; // 1=on, ^1=off

      if ((NGS_SUCCESS != DllGetTheTpuRecordingMode(m_tpuHandle, &mode)))
      {
        std::ostringstream os; printTime(os); os << " - Could not GetTheTpuRecordingMode() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      active =  (mode == 1) ? true : false;

      if (active)
      {
        mode = 0;

        if ((NGS_SUCCESS != DllSetTheTpuRecordingMode(m_tpuHandle, mode)))
        {
          std::ostringstream os; printTime(os); os << " - Could not SetTheTpuRecordingMode() - " ; printError(m_tpuHandle, os); 
          throw os.str().c_str();
        }
      }
    }

    // set recording format to sdf
    {
      const uint32_t format = 1; // 1=sdf, 2=xtf
      if ((NGS_SUCCESS != DllSetTheTpuRecordingFileFormat(m_tpuHandle, format)))
      {
        std::ostringstream os; printTime(os); os << " - Could not SetTheTpuRecordingFileFormat() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }
    }
    // set pings(really pings (not pages)!) per file
    {
      const uint32_t pingsPerFile = 500;
      if ((NGS_SUCCESS != DllSetTheTpuRecordingPingsPerFile(m_tpuHandle, pingsPerFile)))
      {
        std::ostringstream os; printTime(os); os << " - Could not SetTheTpuRecordingPingsPerFile() - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }
      // debug
      {
        std::ostringstream os; printTime(os); 
        os << " - SetTheTpuRecordingPingsPerFile() " << pingsPerFile;
        std::cout << os.str() << std::endl;
      }
    }
    // set file prefix
    {
      const std::string prefix("eca_");
      
      // cast away const char*
      if ((NGS_SUCCESS != DllSetTheTpuRecordingFilePrefix(m_tpuHandle, 
              (char*)prefix.c_str(), prefix.length()+1)))
      {
        std::ostringstream os; printTime(os); os << " - Could not SetTheTpuRecordingFilePrefix() " << prefix << " - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }
      // debug
      {
        std::ostringstream os; printTime(os); 
        os << " - SetTheTpuRecordingFilePrefix() " << prefix;
        std::cout << os.str() << std::endl;
      }

    }
    // set file path
    {
      // const std::string path("/mnt/sdb1/sonarData");
      const std::string path("/mnt/sda1/sonarData");
      uint32_t action = 1; // 1=set, 2=creat, 3=del

      // cast away const char*
      if ((NGS_SUCCESS != DllSetTheTpuRecordingFilePath(m_tpuHandle, 
              (char*)path.c_str(), path.length()+1, action)))
      {
        std::ostringstream os; printTime(os); os << " - Could not SetTheTpuRecordingFilePath() " << path << ", action: " << action << " - "; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }
      
      // debug
      {
        std::ostringstream os; printTime(os); 
        os << " - SetTheTpuRecordingFilePath() " << path << ", action: " << action;
        std::cout << os.str() << std::endl;
      }
    }

    // turn recorder on
    // if (active)
    {
      const uint32_t mode = 1; // 1=on, ^1=off

      if ((NGS_SUCCESS != DllSetTheTpuRecordingMode(m_tpuHandle, mode)))
      {
        std::ostringstream os; printTime(os); os << " - Could not SetTheTpuRecordingMode() - " ; printError(m_tpuHandle, os);
        throw os.str().c_str();
      }

      // debug
      {
        std::ostringstream os; printTime(os); 
        os << " - SetTheTpuRecordingMode() " << mode;
        std::cout << os.str() << std::endl;
      }
    }

    return true;
  }

  void standby(const bool by)
  {
    // 1=standby, 0=run
    const uint32_t mode = (by) ? 1 : 0; 

    if ((NGS_SUCCESS != DllSetTheTpuStandby(m_tpuHandle, mode)))
    {
      std::ostringstream os; printTime(os); os << " - Could not SetTheTpuStandby() - "; printError(m_tpuHandle, os);
      throw os.str().c_str();
    }

    // debug
    {
      std::ostringstream os; printTime(os); os << " - SetTheTpuStandby() - "; 
      os << std::boolalpha << by;
      std::cout << os.str() << std::endl;
    }
  }

  TPU_HANDLE m_tpuHandle;
  const std::string m_spuIP;
  const std::string m_calFile;

};

// ^C handler
static void terminate(const int param)
{
  // only give one shot to shutdown
  if (shutdown)
  {
    std::cerr << "shutdown failed" << std::endl;
    exit(-1);
  }
  shutdown = true;
}

int main(const int ac, const char* const av[])
{
  // map ^C handler
   (void) signal(SIGINT, terminate);

  GetOpt::GetOpt_pp ops(ac, av);

  ops.exceptions(std::ios::failbit | std::ios::eofbit);

  std::string hostname("192.168.0.81");
  std::string calFile("towfish.cal");

  try
  {
    ops >> GetOpt::Option('h', "host", hostname, hostname)
      >> GetOpt::Option('c', "calfile", calFile, calFile);
  }
  catch (const GetOpt::GetOptEx& ex)
  {
    std::cerr << "caught: " << ex.what() << std::endl;
    usage(av[0]);
    return -1;
  }

  try
  {
    std::cout << "SPU: " <<  hostname << ", and bathy cal file: " << calFile << std::endl;

    SPUStatusInterface ECATest(hostname, calFile);

    std::cout << "ECATest.execute returns: " << ECATest.execute() << std::endl;

  }
  catch (const std::string& e)
  {
    std::cerr << "caught: " << e << std::endl;
    return -2;
  }
  catch (const char*& e)
  {
    std::cerr << "caught: " << e << std::endl;
    return -3;
  }

  return 0;
}


