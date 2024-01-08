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


static constexpr const uint8_t STATUS_NOTEON = 0b10010000;
static constexpr const uint8_t STATUS_NOTEOFF = 0b10000000;
static constexpr const uint8_t STATUS_CC = 0b10110000;
static constexpr const uint8_t STATUS_POLYAFTERTOUCH = 0b10100000;
static constexpr const uint8_t STATUS_AFTERTOUCH = 0b11010000;
static constexpr const uint8_t STATUS_PROGRAMCHANGE = 0b11000000;
static constexpr const uint8_t STATUS_PITCHBEND = 0b11100000;


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
          }
          s += std::to_string(0xF7);
          csound->message(s);

      }
      return OK;
  }
};

struct sysex_print_hex : csnd::InPlug<2>
{
  int init()
  {
      return kperf();
  }

  int kperf()
  {
      if(args[1] > 0 && args.myfltvec_data(0)[0] == 0xF0){
          csound->message("SYSEX MESSAGE HEX:");
          for(size_t i = 0; args.myfltvec_data(0)[i] < 0xF7; ++i)
          {
              //s += (std::to_string(args.myfltvec_data(0)[i]) + ", ");
              std::cout << std::hex << (int)args.myfltvec_data(0)[i] << " ";
          }
          std::cout << std::hex << 0xF7 << std::dec << std::endl;
      }
      return OK;
  }
};
/*
 *  RawMIDI
 *
 *
*/

constexpr static const size_t DEFAULT_IN_BUFFER_SIZE = 2048;
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

/**
 * RtMidiInDispatcher
 * This class implements a Midi in callback and handles the input queue.
 * It is instanciated by rawmidi_in_open
 * Each suscriber (midi in opcode using this handle) will read from the queue.
**/
class RtMidiInDispatcher
{
public:
    // Physical port
    RtMidiInDispatcher(csnd::Csound * cs, size_t port, size_t api)
        : buf(1024)
    {
        midiin = new RtMidiIn(get_api_from_index(api));
        if(port >= midiin->getPortCount())
            cs->init_error("MidiIn Port is not valid");

        midiin->openPort(port);
        midiin->ignoreTypes(false, true, false);
        midiin->setBufferSize(DEFAULT_IN_BUFFER_SIZE*4, DEFAULT_IN_BUFFER_COUNT*4);


        queue.ringSize = 100;
        queue.ring = new MidiInApi::MidiMessage[ queue.ringSize ];
        void * uData = (void *)&queue;
        midiin->setCallback([](double stamp, std::vector<unsigned char> *vec, void* udata) {
            MidiInApi::MidiQueue *queue = (MidiInApi::MidiQueue *)udata;
            MidiInApi::MidiMessage msg;
            msg.bytes = *vec;
            msg.timeStamp = stamp;
            queue->push(msg);
        }, uData);
    }
    // Virtual Port
    RtMidiInDispatcher(csnd::Csound * cs, std::string port, size_t api)
        :buf(1024)
    {
        midiin = new RtMidiIn(get_api_from_index(api));
        if(midiin->getPortCount() == 0) {
            cs->init_error("No port found");
        }
        midiin->openVirtualPort(port);
        midiin->ignoreTypes(false, true, false);
        midiin->setBufferSize(DEFAULT_IN_BUFFER_SIZE*4, DEFAULT_IN_BUFFER_COUNT*4);

        queue.ringSize = 100;
        queue.ring = new MidiInApi::MidiMessage[ queue.ringSize ];
        void * uData = (void *)&queue;
        midiin->setCallback([](double stamp, std::vector<unsigned char> *vec, void* udata) {
            MidiInApi::MidiQueue *queue = (MidiInApi::MidiQueue *)udata;
            MidiInApi::MidiMessage msg;
            msg.bytes = *vec;
            msg.timeStamp = stamp;
            queue->push(msg);
        }, uData);
    }

    MidiInApi::MidiQueue &get_queue() {return queue;}

    void use() {
        use_count = (use_count + 1) % suscribed;
        if(use_count == 0) {
            while(queue.pop(&buf, &stamp)) {}
        }
    }
    void suscribe() {suscribed++;}
    void unsuscribe() {if(suscribed > 0) suscribed--;}

private:
    std::vector<unsigned char> buf;
    double stamp;
    size_t use_count = 0;
    size_t suscribed = 0;
    RtMidiIn *midiin;
    MidiInApi::MidiQueue queue;
};


struct rawmidi_in_open : csnd::Plugin<1, 2>
{
    int init() {
        midiin = new RtMidiInDispatcher(csound, (size_t)inargs[0], (size_t)inargs[1]);
        intptr_t ptr = reinterpret_cast<intptr_t>(midiin);
        outargs[0] = ptr;
        buf.resize(1024);
        return OK;
    }

    int kperf() {
        //while(midiin->get_queue().pop(&buf, &stamp)) {
            // Do nothing, just empty the queue
            // This is a critical spot, since this has to happen AFTER the queue has been consumed by input opcodes...
            // Let's hope it works this way
        //}
        return OK;
    }

    double stamp;
    std::vector<unsigned char> buf;
    RtMidiInDispatcher *midiin = nullptr;
};


struct rawmidi_virtual_in_open : csnd::Plugin<1, 2>
{
    int init() {
        midiin = new RtMidiInDispatcher(csound, inargs.str_data(0).data, (size_t)inargs[1]);
        intptr_t ptr = reinterpret_cast<intptr_t>(midiin);
        outargs[0] = ptr;
        buf.resize(1024);
        return OK;
    }
    int kperf() {
       // while(midiin->get_queue().pop(&buf, &stamp)) {
            // Do nothing, just empty the queue
            // This is a critical spot, since this has to happen AFTER the queue has been consumed by input opcodes...
            // Let's hope it keeps working this way
       // }

        return OK;
    }
    double stamp;
    std::vector<unsigned char> buf;
    RtMidiInDispatcher *midiin = nullptr;
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


struct rawmidi_in : csnd::Plugin<3, 1>
{
    int init() {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)inargs[0]);
        midiin = reinterpret_cast<RtMidiInDispatcher *>(ptr);
        if(midiin == nullptr) {
            csound->init_error("Rawmidi -> Midi device is not found, make sure to pass a valid handle initialized with rawmidi_open____");
            return NOTOK;
        }
        midiin->suscribe();

        queue.ringSize = 100;
        queue.ring = new MidiInApi::MidiMessage[ queue.ringSize ];
        outargs.myfltvec_data(2).init(csound, DEFAULT_IN_BUFFER_SIZE * DEFAULT_IN_BUFFER_COUNT);
        buf.resize(DEFAULT_IN_BUFFER_SIZE * DEFAULT_IN_BUFFER_COUNT);
        tmp_buf.resize(DEFAULT_IN_BUFFER_SIZE * DEFAULT_IN_BUFFER_COUNT);
        return kperf();
    }

    int kperf() {
        MidiInApi::MidiQueue &receiver_queue = midiin->get_queue();
        MidiInApi::MidiQueue copy(receiver_queue);
        midiin->use();
        copy.front = front;

        while(copy.pop(&tmp_buf, &stamp)) {
            MidiInApi::MidiMessage msg;
            msg.bytes = tmp_buf;
            msg.timeStamp = stamp;
            queue.push(msg);
        }
        front = copy.front;

        if(!queue.pop(&buf, &stamp)) {
            outargs[0] = 0; // no change
            return OK;
        }

        outargs[0] = 1; // changed // new message
        outargs[1] = buf.size();
        for(size_t i = 0; i < buf.size(); ++i)
        {
            outargs.myfltvec_data(2).data_array()[i] = buf[i];
        }
        return OK;
    }
    double stamp;
    std::vector<unsigned char> buf, tmp_buf;
    RtMidiInDispatcher *midiin;
    MidiInApi::MidiQueue queue;
    unsigned int front;
};

/*
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
*/

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
            if(i > 0 && i < (args[1]-1))
                buf[i] = buf[i] & 0b01111111;
        }

        midiout->sendMessage(&buf);
        return OK;
    }


    std::vector<unsigned char> buf;
    RtMidiOut *midiout;
    double stamp;
};

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
        midiin = reinterpret_cast<RtMidiInDispatcher *>(ptr);
        if(midiin == nullptr) {
            csound->init_error("Rawmidi -> Midi device is not found, make sure to pass a valid handle initialized with rawmidi_open____");
            return NOTOK;
        }

        midiin->suscribe();

        requested_channel = 255;
        requested_ident = 255;

        buf.resize(1024);
        tmp_buf.resize(1024);
        queue.ringSize = 100;
        queue.ring = new MidiInApi::MidiMessage[ queue.ringSize ];

        if(in_count() > 1)
        {
            requested_channel = (uint8_t(inargs[1]) & 0b00001111);
        }
        if(in_count() > 2)
        {
            requested_ident = uint8_t(inargs[2]) & 0b01111111;
        }


        return kperf();
    }

    int kperf()
    {
        MidiInApi::MidiQueue &receiver_queue = midiin->get_queue();
        MidiInApi::MidiQueue copy(receiver_queue);
        midiin->use();
        copy.front = front;
        while(copy.pop(&tmp_buf, &stamp)) {
            MidiInApi::MidiMessage msg;
            msg.bytes = tmp_buf;
            msg.timeStamp = stamp;
            queue.push(msg);
        }
        front = copy.front;

        if(!queue.pop(&buf, &stamp)) {
            return OK;
        }
        if( (buf[0] & status) == status) {
            uint8_t channel = (uint8_t(buf[0]) & 0x0F) + 1;
            uint8_t byte1 = uint8_t(buf[1]) & 0b01111111;
            uint8_t byte2 = uint8_t(buf[2]) & 0b01111111;

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
    std::vector<unsigned char> buf, tmp_buf;
    RtMidiInDispatcher *midiin;
    MidiInApi::MidiQueue queue;
    unsigned int front;
};


template<uint8_t status>
struct rawmidi_2bytes_in : csnd::Plugin<2,2>
{
    int init()
    {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)inargs[0]);
        midiin = reinterpret_cast<RtMidiInDispatcher *>(ptr);
        if(midiin == nullptr) {
            csound->init_error("Rawmidi -> Midi device is not found, make sure to pass a valid handle initialized with rawmidi_open_ in or out");
            return NOTOK;
        }

        requested_channel = 255;
        requested_ident = 255;

        buf.resize(1024);
        tmp_buf.resize(1024);
        queue.ringSize = 100;
        queue.ring = new MidiInApi::MidiMessage[ queue.ringSize ];

        if(in_count() > 1)
        {
            requested_channel = (uint8_t(inargs[1]) & 0b00001111);
        }
        if(in_count() > 2)
        {
            requested_ident = uint8_t(inargs[2]) & 0b01111111;
        }
        return kperf();
    }

    int kperf()
    {
        MidiInApi::MidiQueue &receiver_queue = midiin->get_queue();
        MidiInApi::MidiQueue copy(receiver_queue);
        copy.front = front;
        while(copy.pop(&tmp_buf, &stamp)) {
            MidiInApi::MidiMessage msg;
            msg.bytes = tmp_buf;
            msg.timeStamp = stamp;
            queue.push(msg);
        }
        front = copy.front;

        if(!queue.pop(&buf, &stamp)) {
            return OK;
        }
        if( (buf[0] & status) == status) {
            uint8_t channel = (uint8_t(buf[0]) & 0x0F) + 1;
            uint8_t byte1 = uint8_t(buf[1]) & 0b01111111;

            if(requested_channel < 17 && (channel != requested_channel))
                return OK;
            if(requested_ident < 128 && (byte1 != requested_ident))
                return OK;
            outargs[0] = channel;
            outargs[1] = byte1;
        }
        return OK;
    }

    uint8_t requested_channel, requested_ident;
    double stamp;
    std::vector<unsigned char> buf, tmp_buf;
    RtMidiInDispatcher *midiin;
    MidiInApi::MidiQueue queue;
    unsigned int front;
};

struct rawmidi_noteon_in : rawmidi_3bytes_in<STATUS_NOTEON> {};
struct rawmidi_noteoff_in : rawmidi_3bytes_in<STATUS_NOTEOFF> {};
struct rawmidi_cc_in : rawmidi_3bytes_in<STATUS_CC> {};
struct rawmidi_polyaftertouch_in : rawmidi_3bytes_in<STATUS_POLYAFTERTOUCH> {};
struct rawmidi_pitchbend_in : rawmidi_3bytes_in<STATUS_PITCHBEND> {};
struct rawmidi_programchange_in : rawmidi_2bytes_in<STATUS_PROGRAMCHANGE> {};
struct rawmidi_aftertouch_in : rawmidi_2bytes_in<STATUS_AFTERTOUCH> {};


struct decode_5bytes_float : csnd::Plugin<1, 3>
{
    int init() {
        size_t iinsize = inargs[2];
        static_assert (sizeof(float) == sizeof(uint32_t), "float is not 32 bits" );
        size_t len8 = unbitized7size(iinsize);
        std::cout << "unbitized size " << len8 << std::endl;
        outbytes.allocate(csound, len8);
        inbytes.allocate(csound, iinsize);
        outargs.myfltvec_data(0).init(csound, 3);
        return OK;
    }

    MYFLT convert(uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4) {
        uint32_t temp = 0;
        temp = ((v1 << 24) |
                (v2 << 16) |
                (v3 <<  8) |
                 v4);
        return *((float *) &temp);
    }

    int kperf() {
        csnd::Vector<MYFLT> &in_bytes = inargs.vector_data<MYFLT>(0);
        for(size_t i = 0; i < size_t(inargs[2]); ++i) {
            inbytes[i] = static_cast<uint8_t>( static_cast<unsigned int>(in_bytes[i]));
        }

        uint8_t chksum = unbitize7chksum(inbytes.data(), size_t(inargs[2]), outbytes.data());
        if(chksum != static_cast<uint8_t>(inargs[1])) {
            csound->message("Checksum is different : " +  std::to_string(chksum) + " - " + std::to_string((uint8_t)inargs[1]));
        }
        float *xyz = (float *)outbytes.data();
        outargs.myfltvec_data(0)[0] = xyz[0];
        outargs.myfltvec_data(0)[1] = xyz[1];
        outargs.myfltvec_data(0)[2] = xyz[2];
        return OK;
    }
    csnd::AuxMem<uint8_t> inbytes, outbytes;
};

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

    csnd::plugin<decode_5bytes_float>(csound, "decode_floats", "k[]", "k[]ki", csnd::thread::ik);

    // Print and returns lists of input and output devices corresponding to the specified API
    csnd::plugin<rawmidi_list_devices>(csound, "rawmidi_list_devices", "S[]S[]", "i", csnd::thread::ik);

    /** Opening a device **/
    // ihandle rawmidi_in_open iport_num, [iapi_number]
    csnd::plugin<rawmidi_in_open>(csound, "rawmidi_open_in.ik", "i", "ii", csnd::thread::ik);
    // ihandle rawmidi_out_open iport_num, [iapi_number]
    csnd::plugin<rawmidi_out_open>(csound, "rawmidi_open_out.ii", "i", "ii", csnd::thread::i);
    // ihandle rawmidi_in_open Sport_name, [iapi_number]
    csnd::plugin<rawmidi_virtual_in_open>(csound, "rawmidi_open_virtual_in.ik", "i", "Si", csnd::thread::ik);
    // ihandle rawmidi_out_open Sport_name, [iapi_number]
    csnd::plugin<rawmidi_virtual_out_open>(csound, "rawmidi_open_virtual_out.ii", "i", "Si", csnd::thread::i);

    /** Input **/
    // ksize, kdata[] rawmidi_in in_handle, [iApi_index]
    csnd::plugin<rawmidi_in>(csound, "rawmidi_in", "kkk[]", "i", csnd::thread::ik);
    csnd::plugin<rawmidi_in>(csound, "rawmidi_in", "iii[]", "i", csnd::thread::i);

    /** Output **/
    // rawmidi_out iout_handle, ksize, kdata[], [iApi_index]
    csnd::plugin<rawmidi_out>(csound, "rawmidi_out", "", "ikk[]", csnd::thread::ik);
    csnd::plugin<rawmidi_out>(csound, "rawmidi_out", "", "iii[]", csnd::thread::i);

    // Facility for sysex handling
    csnd::plugin<sysex_print>(csound, "sysex_print", "", "k[]k", csnd::thread::ik );
    csnd::plugin<sysex_print>(csound, "sysex_print", "", "i[]i", csnd::thread::i );
    csnd::plugin<sysex_print_hex>(csound, "sysex_print_hex", "", "k[]k", csnd::thread::ik );
    csnd::plugin<sysex_print_hex>(csound, "sysex_print_hex", "", "i[]i", csnd::thread::i );

    /** Channel messages out **/
    // rawmidi_noteon_out ihandle, ichannel, inote, ivel
    csnd::plugin<rawmidi_noteon_out>(csound, "rawmidi_noteon_out.ii", "", "iiii", csnd::thread::i);
    csnd::plugin<rawmidi_noteoff_out>(csound, "rawmidi_noteoff_out.ii", "", "iiii", csnd::thread::i);
    csnd::plugin<rawmidi_cc_out>(csound, "rawmidi_cc_out.ii", "", "iiii", csnd::thread::i);
    csnd::plugin<rawmidi_pitchbend_out>(csound, "rawmidi_pitchbend_out.ii", "", "iiii", csnd::thread::i);
    csnd::plugin<rawmidi_polyaftertouch_out>(csound, "rawmidi_polyaftertouch_out.ii", "", "iiii", csnd::thread::i);
    csnd::plugin<rawmidi_aftertouch_out>(csound, "rawmidi_aftertouch_out.ii", "", "iii", csnd::thread::i);
    csnd::plugin<rawmidi_programchange_out>(csound, "rawmidi_programchange_out.ii", "", "iii", csnd::thread::i);

    // rawmidi_noteon_out ihandle, kchannel, knote, kvel
    csnd::plugin<rawmidi_noteon_out>(csound, "rawmidi_noteon_out.ik", "", "ikkk", csnd::thread::ik);
    csnd::plugin<rawmidi_noteoff_out>(csound, "rawmidi_noteoff_out.ik", "", "ikkk", csnd::thread::ik);
    csnd::plugin<rawmidi_cc_out>(csound, "rawmidi_cc_out.ik", "", "ikkk", csnd::thread::ik);
    csnd::plugin<rawmidi_pitchbend_out>(csound, "rawmidi_pitchbend_out.ik", "", "ikkk", csnd::thread::ik);
    csnd::plugin<rawmidi_polyaftertouch_out>(csound, "rawmidi_polyaftertouch_out.ik", "", "ikkk", csnd::thread::ik);
    csnd::plugin<rawmidi_aftertouch_out>(csound, "rawmidi_aftertouch_out.ik", "", "ikk", csnd::thread::ik);
    csnd::plugin<rawmidi_programchange_out>(csound, "rawmidi_programchange_out.ik", "", "ikk", csnd::thread::ik);

    /** Channel messages in **/
    /** Side note : Do not share handles for input **/
    /** Due to this impossibility to share, a consumer count is kept in the handler structure to consume when each child consumed **/
    // kchannel, knote, kvel rawmidi_noteon_in ihandle
    csnd::plugin<rawmidi_noteon_in>(csound, "rawmidi_noteon_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_noteoff_in>(csound, "rawmidi_noteon_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_cc_in>(csound, "rawmidi_cc_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_pitchbend_in>(csound, "rawmidi_pitchbend_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_polyaftertouch_in>(csound, "rawmidi_polyaftertouch_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_aftertouch_in>(csound, "rawmidi_aftertouch_in.ik", "kk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_programchange_in>(csound, "rawmidi_programchange_in.ik", "kk", "im", csnd::thread::ik);

}
