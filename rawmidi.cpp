#include <iostream>
#include<plugin.h>
#include<modload.h>
#include<OpcodeBase.hpp>
#include"rtmidi/RtMidi.h"

using namespace std;

#include<cstdint>
#include<cstddef>

#include<bitset>

/*
 *  A flexible MIDI library for Csound using RTMidi.
 *  Features :
 *  * Device oriented MIDI communication > ideal for setups with a lot of devices
 * 	* Not limited to MIDI channel messages (Sysex is on the table)
 *  * Raw access to MIDI streams
 *  * I and K rate for channel messages
 *
 *
 * 	* TODO
 * 	  * Filter for receiver : sysex only, note messages, cc's etc
*/



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
          std::string s;
          for(size_t i = 0; args.myfltvec_data(0)[i] < 0xF7; ++i)
          {
              s += (std::to_string(args.myfltvec_data(0)[i]) + ", ");
              //csound->get_csound()->Message(csound->get_csound(), csound->get_csound()->LocalizeString(s.c_str()) );
          }
          s += std::to_string(0xF7);
          csound->message(s);

      }
      return OK;
  }
};

/*
 *  RawMIDI
 *
 *
*/

constexpr static const size_t DEFAULT_IN_BUFFER_SIZE = 16384;
constexpr static const size_t DEFAULT_IN_BUFFER_COUNT = 4;

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

struct  rawmidi_list_devices : csnd::Plugin<2, 1>
{
    int init() {

        csnd::Vector<STRINGDAT> &in_ports = outargs.vector_data<STRINGDAT>(0);
        csnd::Vector<STRINGDAT> &out_ports = outargs.vector_data<STRINGDAT>(1);

        int api_nbr = (int)inargs[0];
        RtMidiIn midiin(get_api_from_index(api_nbr));
        int nports = midiin.getPortCount();

        in_ports.init(csound, nports);
        csound->message(">>> Input ports : ");
        for(int i = 0; i < nports; ++i)
        {
            in_ports[i].data = csound->strdup((char *) midiin.getPortName(i).c_str());
            in_ports[i].size = midiin.getPortName(i).size();
            csound->message("\tPort " + std::to_string(i) + " : " + midiin.getPortName(i));
        }

        RtMidiOut midiout(get_api_from_index(api_nbr));
        nports = midiout.getPortCount();

        out_ports.init(csound, nports);
        csound->message(">>> Output ports : ");
        for(int i = 0; i < nports; ++i) {
            out_ports[i].data = csound->strdup((char *) midiout.getPortName(i).c_str());
            out_ports[i].size = midiout.getPortName(i).size();
            csound->message("\tPort " + std::to_string(i) + " : " + midiout.getPortName(i));
        }
        return OK;
    }

};

struct rawmidi_virtual_in_open : csnd::Plugin<1, 2>
{
    int init() {
        if(in_count() > 1)
        {
           midiin = new RtMidiIn(get_api_from_index(inargs[1]));
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


        midiin->openVirtualPort(inargs.str_data(0).data);
        midiin->ignoreTypes(false, true, false);
        midiin->setBufferSize(16384, 4);
        outargs[0] = reinterpret_cast<intptr_t>(midiin);
        return OK;
    }
    RtMidiIn *midiin = nullptr;
};

struct rawmidi_in_open : csnd::Plugin<1, 2>
{
    int init() {
        if(in_count() > 1)
        {
           midiin = new RtMidiIn(get_api_from_index(inargs[1]));
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
        midiin->ignoreTypes(false, true, false);
        midiin->setBufferSize(DEFAULT_IN_BUFFER_SIZE*4, DEFAULT_IN_BUFFER_COUNT*4);

        outargs[0] = reinterpret_cast<intptr_t>(midiin);


        return OK;
    }
    RtMidiIn *midiin = nullptr;
};

struct rawmidi_virtual_out_open : csnd::Plugin<1, 2>
{
    int init() {
        if(in_count() > 1)
            midiout = new RtMidiOut(get_api_from_index(static_cast<int>(inargs[1])));
        else
            midiout = new RtMidiOut();

        unsigned int nports = midiout->getPortCount();
        if(nports == 0) {
           csound->init_error("No ports available");
           return NOTOK;
        }
        midiout->openVirtualPort(inargs.str_data(0).data);
        outargs[0] = reinterpret_cast<intptr_t>(midiout);
        return OK;
    }

    RtMidiOut *midiout = nullptr;
};

struct rawmidi_out_open : csnd::Plugin<1, 2>
{
    int init() {
        if(in_count() > 1)
            midiout = new RtMidiOut(get_api_from_index(static_cast<int>(inargs[1])));
        else
            midiout = new RtMidiOut();

        unsigned int nports = midiout->getPortCount();
        if(nports == 0) {
           csound->init_error("No ports available");
           return NOTOK;
        }
        size_t port_num = (int)inargs[0];
        if(port_num >= nports) {
            csound->init_error("Port not available" );
            return NOTOK;
        }

        midiout->openPort(port_num);
        outargs[0] = reinterpret_cast<intptr_t>(midiout);
        return OK;
    }

    RtMidiOut *midiout = nullptr;
};

template<typename T>
inline bool is_power_of_two(T n)
{
    if(n <= 0) return false;
    T i = n & (n - 1);
    return (i == 0) ? true : false;
}

struct rawmidi_in_set_buffersize : csnd::InPlug<2>
{

    int init() {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)args[0]);
        midiin = reinterpret_cast<RtMidiIn *>(ptr);
        if(midiin == nullptr)
        {
            csound->init_error("Pointer to midi device is null");
            return NOTOK;
        }

        if(!is_power_of_two((int)args[1]))
        {
            csound->init_error("New buffer size needs to be power of 2");
            return NOTOK;
        }

        midiin->setBufferSize(size_t(args[1]), size_t(args[2]));

        return OK;
    }
    RtMidiIn *midiin;
};

struct rawmidi_in : csnd::Plugin<2, 1>
{
    int init() {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)inargs[0]);
        midiin = reinterpret_cast<RtMidiIn *>(ptr);
        if(midiin == nullptr)
        {
            csound->init_error("Pointer to midi device is null");
            return NOTOK;
        }

        outargs.myfltvec_data(1).init(csound, DEFAULT_IN_BUFFER_SIZE * DEFAULT_IN_BUFFER_COUNT);
        vbuf.resize(DEFAULT_IN_BUFFER_SIZE * DEFAULT_IN_BUFFER_COUNT);
        return kperf();
    }

    int kperf() {
        stamp = midiin->getMessage(&vbuf);
        outargs[0] = vbuf.size();
        for(size_t i = 0; i < vbuf.size(); ++i)
        {
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

struct rawmidi_out : csnd::InPlug<3>
{
    int init() {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)args[0]);
        midiout = reinterpret_cast<RtMidiOut *>(ptr);
        if(midiout == nullptr)
        {
            csound->init_error("Pointer to midi device is null");
            return NOTOK;
        }

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

static constexpr const uint8_t STATUS_NOTEON = 0b10010000;
static constexpr const uint8_t STATUS_NOTEOFF = 0b10000000;
static constexpr const uint8_t STATUS_CC = 0b10110000;
static constexpr const uint8_t STATUS_POLYAFTERTOUCH = 0b10100000;
static constexpr const uint8_t STATUS_AFTERTOUCH = 0b11010000;
static constexpr const uint8_t STATUS_PROGRAMCHANGE = 0b11000000;
static constexpr const uint8_t STATUS_PITCHBEND = 0b11100000;

template<uint8_t status>
struct rawmidi_3bytes_out : csnd::InPlug<4>
{
    int init() {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)args[0]);
        midiout = reinterpret_cast<RtMidiOut *>(ptr);
        if(midiout == nullptr) {
            csound->init_error("Rawmidi -> Midi device is not found, make sure to pass a valid handle initialized with rawmidi_open____");
            return NOTOK;
        }
        return kperf();
    }

    int kperf() {
        // if no change, do nothing (avoid sending same message
        if(channel == args[1] && args[2] == byte2 && args[3] == byte3)
            return OK;

        channel = (uint8_t(args[1]) & 0x000F) - 1;
        status_byte = status | (channel & 0x0F);
        msg[0] = status_byte;
        byte2 = ((unsigned char)(uint8_t)args[2]) & 0b01111111;
        byte3 = ((unsigned char)(uint8_t)args[3]) & 0b01111111;
        msg[1] = byte2;
        msg[2] = byte3;
        midiout->sendMessage(msg, 3);
        return OK;
    }


    uint8_t status_byte;
    // To 255 for equality check >
    uint8_t channel = 255;
    uint8_t byte2 = 255, byte3 = 255;
    RtMidiOut *midiout;
    unsigned char msg[3];
};

template<uint8_t status>
struct rawmidi_2bytes_out : csnd::InPlug<3>
{
    int init() {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)args[0]);
        midiout = reinterpret_cast<RtMidiOut *>(ptr);
        if(midiout == nullptr) {
            csound->init_error("Rawmidi -> Midi device is not found, make sure to pass a valid handle initialized with rawmidi_open____");
            return NOTOK;
        }
        return kperf();
    }

    int kperf() {
        // if no change, do nothing (avoid sending same message
        if(channel == args[1] && args[2] == byte2)
            return OK;

        channel = (uint8_t(args[1]) & 0x0F) - 1;
        status_byte = status | (channel & 0x0F);
        msg[0] = status_byte;
        byte2 = ((unsigned char)(uint8_t)args[2]) & 0b01111111;
        msg[1] = byte2;
        midiout->sendMessage(msg, 2);
        return OK;
    }

    uint8_t status_byte;
    // To 255 for equality check >
    uint8_t channel = 255;
    uint8_t byte2 = 255;
    RtMidiOut *midiout;
    unsigned char msg[2];
};

struct rawmidi_noteon_out : public rawmidi_3bytes_out<STATUS_NOTEON> {};
struct rawmidi_noteoff_out : public rawmidi_3bytes_out<STATUS_NOTEOFF> {};
struct rawmidi_cc_out : public rawmidi_3bytes_out<STATUS_CC> {};
struct rawmidi_pitchbend_out : public rawmidi_3bytes_out<STATUS_PITCHBEND> {};
struct rawmidi_polyaftertouch_out : public rawmidi_3bytes_out<STATUS_POLYAFTERTOUCH> {};
struct rawmidi_programchange_out : public rawmidi_2bytes_out<STATUS_PROGRAMCHANGE> {};
struct rawmidi_aftertouch_out : public rawmidi_2bytes_out<STATUS_AFTERTOUCH> {};

template<uint8_t status>
struct rawmidi_3bytes_in : csnd::Plugin<3,3>
{
    int init()
    {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)inargs[0]);
        midiin_src = reinterpret_cast<RtMidiIn *>(ptr);
        if(midiin_src == nullptr) {
            csound->init_error("Rawmidi -> Midi device is not found, make sure to pass a valid handle initialized with rawmidi_open____");
            return NOTOK;
        }
        midiin = midiin_src;

        requested_channel = 255;
        requested_ident = 255;

        if(in_count() > 1)
        {
            requested_channel = (uint8_t(inargs[1]) & 0b00001111);
        }
        if(in_count() > 2)
        {
            requested_ident = uint8_t(inargs[2]) & 0b01111111;
        }

        vbuf.resize(3);
        return kperf();
    }

    int kperf()
    {
        stamp = midiin->getMessage(&vbuf);
        if( (vbuf[0] & status) == status) {
            uint8_t channel = (uint8_t(vbuf[0]) & 0x0F) + 1;
            uint8_t byte1 = uint8_t(vbuf[1]) & 0b01111111;
            uint8_t byte2 = uint8_t(vbuf[2]) & 0b01111111;

            if(requested_channel < 17 && (channel != requested_channel))
                return OK;
            if(requested_ident < 128 && (byte1 != requested_ident))
                return OK;
            outargs[0] = channel;
            outargs[1] = byte1;
            outargs[2] = byte2;
        }
        return OK;
    }

    uint8_t requested_channel, requested_ident;
    double stamp;
    std::vector<unsigned char> vbuf;
    RtMidiIn *midiin_src, *midiin;
};

template<uint8_t status>
struct rawmidi_2bytes_in : csnd::Plugin<2,2>
{

    int init()
    {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)inargs[0]);
        midiin = reinterpret_cast<RtMidiIn *>(ptr);
        if(midiin == nullptr) {
            csound->init_error("Rawmidi -> Midi device is not found, make sure to pass a valid handle initialized with rawmidi_open____");
            return NOTOK;
        }

        requested_channel = 255;
        if(in_count() > 1)
            requested_channel = uint8_t(inargs[1]) & 0b00001111;

        vbuf.resize(2);
        return kperf();
    }

    int kperf()
    {
        stamp = midiin->getMessage(&vbuf);
        if( (vbuf[0] & status) == status) {
            uint8_t channel = (uint8_t(vbuf[0]) & 0b00001111) + 1 ;
            uint8_t byte1 = uint8_t(vbuf[1]) & 0b01111111;
            if(requested_channel < 17 && (channel != requested_channel))
                return OK;
            outargs[0] = channel;
            outargs[1] = byte1;
        }
        return OK;
    }

    uint8_t requested_channel;
    double stamp;
    std::vector<unsigned char> vbuf;
    RtMidiIn *midiin;
};

struct rawmidi_noteon_in : rawmidi_3bytes_in<STATUS_NOTEON> {};
struct rawmidi_noteoff_in : rawmidi_3bytes_in<STATUS_NOTEOFF> {};
struct rawmidi_cc_in : rawmidi_3bytes_in<STATUS_CC> {};
struct rawmidi_polyaftertouch_in : rawmidi_3bytes_in<STATUS_POLYAFTERTOUCH> {};
struct rawmidi_pitchbend_in : rawmidi_3bytes_in<STATUS_PITCHBEND> {};
struct rawmidi_programchange_in : rawmidi_2bytes_in<STATUS_PROGRAMCHANGE> {};
struct rawmidi_aftertouch_in : rawmidi_2bytes_in<STATUS_AFTERTOUCH> {};

void csnd::on_load(Csound *csound) {
    std::cout << "RawMIDI : RtMidi for Csound " << std::endl;
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

    // Print and returns lists of input and output devices corresponding to the specified API
    csnd::plugin<rawmidi_list_devices>(csound, "rawmidi_list_devices", "S[]S[]", "i", csnd::thread::ik);

    /** Opening a device **/
    // ihandle rawmidi_in_open iport_num, [iapi_number]
    csnd::plugin<rawmidi_in_open>(csound, "rawmidi_open_in", "i", "ii", csnd::thread::i);
    // ihandle rawmidi_out_open iport_num, [iapi_number]
    csnd::plugin<rawmidi_out_open>(csound, "rawmidi_open_out", "i", "ii", csnd::thread::i);
    // ihandle rawmidi_in_open Sport_name, [iapi_number]
    csnd::plugin<rawmidi_virtual_in_open>(csound, "rawmidi_open_virtual_in", "i", "Si", csnd::thread::i);
    // ihandle rawmidi_out_open Sport_name, [iapi_number]
    csnd::plugin<rawmidi_virtual_out_open>(csound, "rawmidi_open_virtual_out", "i", "Si", csnd::thread::i);

    /** Input **/
    // ksize, kdata[] rawmidi_in in_handle, [iApi_index]
    csnd::plugin<rawmidi_in>(csound, "rawmidi_in", "kk[]", "i", csnd::thread::ik);
    csnd::plugin<rawmidi_in>(csound, "rawmidi_in", "ii[]", "i", csnd::thread::i);

    /** Output **/
    // rawmidi_out iout_handle, ksize, kdata[], [iApi_index]
    csnd::plugin<rawmidi_out>(csound, "rawmidi_out", "", "ikk[]", csnd::thread::ik);
    csnd::plugin<rawmidi_out>(csound, "rawmidi_out", "", "iii[]", csnd::thread::i);

    // Facility for sysex handling
    csnd::plugin<sysex_print>(csound, "sysex_print", "", "k[]k", csnd::thread::ik );
    csnd::plugin<sysex_print>(csound, "sysex_print", "", "i[]i", csnd::thread::i );

    /** Channel messages out **/
    // rawmidi_noteon_out ihandle, ichannel, inote, ivel
    csnd::plugin<rawmidi_noteon_out>(csound, "rawmidi_noteon_out.ii", "", "iiii", csnd::thread::i);
    csnd::plugin<rawmidi_noteoff_out>(csound, "rawmidi_noteoff_out.ii", "", "iiii", csnd::thread::i);
    csnd::plugin<rawmidi_cc_out>(csound, "rawmidi_cc_out.ii", "", "iiii", csnd::thread::i);
    csnd::plugin<rawmidi_pitchbend_out>(csound, "rawmidi_pitchbend_out.ii", "", "iiii", csnd::thread::i);
    csnd::plugin<rawmidi_polyaftertouch_out>(csound, "rawmidi_noteoff_out.ii", "", "iiii", csnd::thread::i);
    csnd::plugin<rawmidi_aftertouch_out>(csound, "rawmidi_aftertouch_out.ii", "", "iii", csnd::thread::i);
    csnd::plugin<rawmidi_programchange_out>(csound, "rawmidi_programchange_out.ii", "", "iii", csnd::thread::i);

    // rawmidi_noteon_out ihandle, kchannel, knote, kvel
    csnd::plugin<rawmidi_noteon_out>(csound, "rawmidi_noteon_out.ik", "", "ikkk", csnd::thread::ik);
    csnd::plugin<rawmidi_noteoff_out>(csound, "rawmidi_noteoff_out.ik", "", "ikkk", csnd::thread::ik);
    csnd::plugin<rawmidi_cc_out>(csound, "rawmidi_cc_out.ik", "", "ikkk", csnd::thread::ik);
    csnd::plugin<rawmidi_pitchbend_out>(csound, "rawmidi_pitchbend_out.ik", "", "ikkk", csnd::thread::ik);
    csnd::plugin<rawmidi_polyaftertouch_out>(csound, "rawmidi_noteoff_out.ik", "", "ikkk", csnd::thread::ik);
    csnd::plugin<rawmidi_aftertouch_out>(csound, "rawmidi_aftertouch_out.ik", "", "ikk", csnd::thread::ik);
    csnd::plugin<rawmidi_programchange_out>(csound, "rawmidi_programchange_out.ik", "", "ikk", csnd::thread::ik);

    /** Channel messages in **/
    /** Side note : Do not share handles for input **/
    // kchannel, knote, kvel rawmidi_noteon_in ihandle
    csnd::plugin<rawmidi_noteon_in>(csound, "rawmidi_noteon_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_noteoff_in>(csound, "rawmidi_noteon_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_cc_in>(csound, "rawmidi_cc_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_pitchbend_in>(csound, "rawmidi_pitchbend_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_polyaftertouch_in>(csound, "rawmidi_polyaftertouch_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_aftertouch_in>(csound, "rawmidi_aftertouch_in.ik", "kk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_programchange_in>(csound, "rawmidi_programchange_in.ik", "kk", "im", csnd::thread::ik);
}
