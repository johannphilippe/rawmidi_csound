#include <iostream>
#include<plugin.h>
#include<modload.h>
#include<OpcodeBase.hpp>
#include"rtmidi/RtMidi.h"

using namespace std;

#include<cstdint>
#include<cstddef>


/**
* @brief Get size of the resulting 7 bits bytes array obtained when using the bitize7checksum function
*/
constexpr size_t bitized7size(size_t len)
{
    return len / 7 * 8 + (len % 7 ? 1 + len % 7 : 0);
}
/**
* @brief Get size of the resulting 8 bits bytes array obtained when using the unbitize7 function
*/
constexpr size_t unbitized7size(size_t len)
{
    return len / 8 * 7 + (len % 8 ? len % 8 - 1 : 0);
}

/**
* @brief 7-bitize an array of bytes and get the resulting checksum
*
* @param in Input array of 8 bits bytes
* @param inlen Length in bytes of the input array of 8 bits bytes
* @param out An output array of bytes that will receive the 7-bitized bytes
* @return the output 7-bitized bytes XOR checksum
*/
constexpr uint8_t bitize7chksum(const uint8_t* in, size_t inlen, uint8_t* out)
{
    uint8_t chksum = 0;
    for (size_t i{0}, outsize{0}; i < inlen; i += 7, outsize += 8)
    {
        out[outsize] = 0;
        for (size_t j = 0; (j < 7) && (i + j < inlen); ++j)
        {
            out[outsize] |= (in[i + j] & 0x80) >> (j + 1);
            out[outsize + j + 1] = in[i + j] & 0x7F;
            chksum ^= out[outsize + j + 1];
        }
        chksum ^= out[outsize];
    }
    return chksum;
}

/**
* @brief 7-unbitize an array of bytes and get the incoming checksum
*
* @param in Input array of 7 bits bytes
* @param inlen Length in bytes of the input array of 7 bits bytes
* @param out An output array of bytes that will receive the 7-unbitized bytes
* @return the input 7-bitized bytes XOR checksum
*/
constexpr uint8_t unbitize7chksum(const uint8_t* in, size_t inlen, uint8_t* out)
{
    uint8_t chksum = 0;
    for (size_t i{0}, outsize{0}; i < inlen; i += 8, outsize += 7)
    {
        chksum ^= in[i];
        for (size_t j = 0; (j < 7) && (j + 1 + i < inlen); ++j)
        {
            out[outsize + j] = ((in[i] << (j + 1)) & 0x80) | in[i + j + 1];
            chksum ^= in[i + j + 1];
        }
    }
    return chksum;
}

struct sysex_array : csnd::Plugin<1, 128>
{
  int init()
  {
      data.allocate(csound, in_count());
      for(size_t i = 0; i < in_count(); ++i)
      {
          data[i] = inargs[i];
          data[i] = data[i] & 0b01111111;

      }
      data[0] = 0xF0;
      data[data.len() - 1] = 0xF7;

      outargs.myfltvec_data(0).init(csound, in_count()+2);
      for(size_t i = 1; i < in_count(); ++i)
          outargs.myfltvec_data(0)[i] = data[i];
      outargs.myfltvec_data(0)[0] = 0xF0;
      outargs.myfltvec_data(0)[in_count()] = 0xF7;


      return OK;
  }

  csnd::AuxMem<uint8_t> data;
};


struct sysex_print : csnd::InPlug<2>
{
  int init()
  {
      return kperf();
  }

  int kperf()
  {
      if(args[1] > 0 && args.myfltvec_data(0)[0] == 0xF0){
          csound->message("SYSEX MESSAGE :");
          for(size_t i = 0; args.myfltvec_data(0)[i] < 0xF7; ++i)
          {
              std::string s(std::to_string(args.myfltvec_data(0)[i]) + ", ");
              csound->get_csound()->Message(csound->get_csound(), csound->get_csound()->LocalizeString(s.c_str()) );
          }
          csound->message(std::to_string(0xF7));

      }
      return OK;
  }
};

/*
 *  RawMIDI
 *
 *
*/

static RtMidi::Api get_api_from_index(int index)
{

    switch (index) {
    case 0:
        return RtMidi::Api::UNSPECIFIED;
    case 1:
        return RtMidi::Api::MACOSX_CORE;
    case 2:
        return RtMidi::Api::LINUX_ALSA;
    case 3:
        return RtMidi::Api::UNIX_JACK;
    case 4:
        return RtMidi::Api::WINDOWS_MM;
    case 5:
        return RtMidi::Api::RTMIDI_DUMMY;
    case 6:
        return RtMidi::Api::WEB_MIDI_API;
    case 7:
        return RtMidi::Api::NUM_APIS;
    default:
        return RtMidi::Api::UNSPECIFIED;
    }

    /*
    0=UNSPECIFIED,
    1=MACOSX_CORE,
    2=LINUX_ALSA,
    3=UNIX_JACK,
    4=WINDOWS_MM,
    5=RTMIDI_DUMMY,
    6=WEB_MIDI_API,
    7=NUM_APIS
    */
    // return static_cast<RtMidi::Api>(index);
}

struct  rawmidi_list_devices : csnd::InPlug<1>
{
    int init() {
        int api_nbr = (int)args[0];
        RtMidiIn midiin(get_api_from_index(api_nbr));
        int nports = midiin.getPortCount();
        csound->message(">>> Input ports : ");
        for(int i = 0; i < nports; ++i)
            csound->message("\tPort " + std::to_string(i) + " : " +midiin.getPortName(i));

        RtMidiOut midiout(get_api_from_index(api_nbr));
        nports = midiout.getPortCount();
        csound->message(">>> Output ports : ");
        for(int i = 0; i < nports; ++i)
            csound->message("\tPort " + std::to_string(i) + " : " + midiout.getPortName(i));

        return OK;
    }
};

struct rawmidi_in : csnd::Plugin<2, 2>
{
    int init() {

        std::cout << "in count " << in_count() << std::endl;

        if(in_count() > 1)
        {
           std::cout << "inargs : " << *inargs.data(0) << " " << *inargs.data(1) << std::endl;
           std::cout << "arg " << inargs[1] <<  " &  api number : " << get_api_from_index(inargs[1]) << std::endl;
           midiin = new RtMidiIn(get_api_from_index(inargs[1]));
           //midiin = new RtMidiIn(RtMidi::Api::LINUX_ALSA);
        }
        else
        {
           midiin = new RtMidiIn();
        }

        unsigned int nports = midiin->getPortCount();
        if(nports == 0) {
            csound->init_error("no ports available");
            return NOTOK;
        }


        size_t port_num = static_cast<size_t>(inargs[0]);
        if(port_num >= nports) {
            csound->init_error("Port num is not available");
            return NOTOK;
        }

        midiin->openPort(port_num);
        midiin->ignoreTypes(false, false, false);
        midiin->setBufferSize(4096, 4);
        outargs.myfltvec_data(1).init(csound, 4096);
        vbuf.resize(4096);
        return kperf();
    }

    int kperf() {
        stamp = midiin->getMessage(&vbuf);
        outargs[0] = vbuf.size();
        for(size_t i = 0; i < vbuf.size(); ++i)
        {
            if(i >= vbuf.size()) {
                break;
            }
            outargs.myfltvec_data(1).data_array()[i] = vbuf[i];
        }
        return OK;
    }



    RtMidiIn *midiin;
    std::vector<unsigned char> vbuf;
    double stamp;
};

inline void bit7(std::vector<unsigned char> &buf)
{
    for(size_t i = 1; i < buf.size() - 1; ++i)
    {
        buf[i] = buf[i] & 0b01111111;
    }
}

struct rawmidi_out : csnd::InPlug<4>
{
    int init() {
        if(in_count() > 3)
            midiout = new RtMidiOut(get_api_from_index(static_cast<int>(args[3])));
        else
            midiout = new RtMidiOut();

        unsigned int nports = midiout->getPortCount();
        if(nports == 0) {
           csound->init_error("No ports available");
           return NOTOK;
        }
        size_t port_num = (int)args[0];
        if(port_num >= nports) {
            csound->init_error("Port not available" );
            return NOTOK;
        }

        midiout->openPort(port_num);
        //buf.resize(4096);

        return kperf();
    }

    int kperf()
    {

        buf.clear();
        for(size_t i = 0; i < args[1]; ++i)
        {
            buf.push_back(static_cast<unsigned char>(args.myfltvec_data(2)[i]));
        }

        midiout->sendMessage(&buf);
        return OK;
    }


    std::vector<unsigned char> buf;
    RtMidiOut *midiout;
    double stamp;
};

void csnd::on_load(Csound *csound) {
    std::cout << "RtMidi RawMIDI for Csound " << std::endl;
    std::cout << "Supported API's are : \n"
            <<   "\t0=UNSPECIFIED, \n"
            <<   "\t1=MACOSX_CORE, \n"
            <<   "\t2=LINUX_ALSA, \n"
            <<   "\t3=UNIX_JACK, \n"
            <<   "\t4=WINDOWS_MM, \n"
            <<   "\t5=RTMIDI_DUMMY, \n"
            <<   "\t6=WEB_MIDI_API, \n"
            <<   "\t7=NUM_APIS"
              << std::endl;


    csnd::plugin<rawmidi_list_devices>(csound, "rawmidi_list_devices", "", "i", csnd::thread::ik);

    // ksize, kdata[] rawmidi_in iport_num, [iApi_index]
    csnd::plugin<rawmidi_in>(csound, "rawmidi_in", "kk[]", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_in>(csound, "rawmidi_in", "ii[]", "im", csnd::thread::i);

    // rawmidi_out iport_num, ksize, kdata[], [iApi_index]
    csnd::plugin<rawmidi_out>(csound, "rawmidi_out", "", "ikk[]m", csnd::thread::ik);
    csnd::plugin<rawmidi_out>(csound, "rawmidi_out", "", "iii[]m", csnd::thread::i);

    csnd::plugin<sysex_array>(csound, "sysex_arr", "i[]", "m", csnd::thread::i);

    csnd::plugin<sysex_print>(csound, "sysex_print", "", "k[]k", csnd::thread::ik );
    csnd::plugin<sysex_print>(csound, "sysex_print", "", "i[]i", csnd::thread::i );

}
