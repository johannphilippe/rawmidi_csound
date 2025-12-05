#include <iostream>
#include<plugin.h>
#include<modload.h>
#include<OpcodeBase.hpp>
#include"rtmidi/RtMidi.h"
#include "rtmidi/rtmidi_c.h"

using namespace std;

#include<array>

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
*/


static constexpr const uint8_t STATUS_NOTEON = 0b10010000; // 144
static constexpr const uint8_t STATUS_NOTEOFF = 0b10000000; //128
static constexpr const uint8_t STATUS_CC = 0b10110000; // 176
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
 *  & RtMidi Adapters
 *
*/

constexpr static const size_t DEFAULT_IN_BUFFER_SIZE = 128;
constexpr static const size_t DEFAULT_IN_BUFFER_COUNT = 4;

static RtMidiApi get_api_from_index(int index)
{

    switch (index) {
        case 0:
            return RtMidiApi::RTMIDI_API_UNSPECIFIED;
        case 1:
            return RtMidiApi::RTMIDI_API_MACOSX_CORE;
        case 2:
            return RtMidiApi::RTMIDI_API_LINUX_ALSA;
        case 3:
            return RtMidiApi::RTMIDI_API_UNIX_JACK;
        case 4:
            return RtMidiApi::RTMIDI_API_WINDOWS_MM;
        case 5:
            return RtMidiApi::RTMIDI_API_RTMIDI_DUMMY;
        case 6:
            return RtMidiApi::RTMIDI_API_NUM;
        default: 
            return RtMidiApi::RTMIDI_API_UNSPECIFIED;
    }
}

struct  rawmidi_list_devices : csnd::Plugin<2, 1>
{
    int init() {
        buf.allocate(csound, 512);

        csnd::Vector<STRINGDAT> &in_ports = outargs.vector_data<STRINGDAT>(0);
        csnd::Vector<STRINGDAT> &out_ports = outargs.vector_data<STRINGDAT>(1);

        int api_nbr = (int)inargs[0];
        RtMidiInPtr midiin = rtmidi_in_create(get_api_from_index(api_nbr), "Client Name", 1024);
        int nports = rtmidi_get_port_count(midiin); //midiin.getPortCount();

        in_ports.init(csound, nports);
        csound->message(">>> Input ports : ");
        int len;
        for(int i = 0; i < nports; ++i)
        {
            rtmidi_get_port_name(midiin, i, buf.data(), &len);
            in_ports[i].data = csound->strdup((char *)buf.data());
            in_ports[i].size = len;
            csound->message("\tPort " + std::to_string(i) + " : " + in_ports[i].data);
        }
        RtMidiOutPtr midiout = rtmidi_out_create(get_api_from_index(api_nbr), "Client Name");
        nports = rtmidi_get_port_count(midiout);
        out_ports.init(csound, nports);
        csound->message(">>> Output ports : ");
        for(int i = 0; i < nports; ++i) {

            rtmidi_get_port_name(midiin, i, buf.data(), &len);
            out_ports[i].data = csound->strdup((char *)buf.data());
            out_ports[i].size = len;
            csound->message("\tPort " + std::to_string(i) + " : " + out_ports[i].data);
        }
        return OK;
    }

    csnd::AuxMem<char> buf; 
};

/*
    Csound AuxMem based Queue

*/
 
template<typename T>
struct cs_queue 
{
    cs_queue() {}

    cs_queue(csnd::Csound *cs, size_t bsize)
        : front(0)
        , back(0)
        , ringsize(0)
    {
        _data.allocate(cs, bsize);
    }

    void prealloc(csnd::Csound *cs, size_t len, T* mem)
    {
        front = 0;
        back = 0;
        _data = mem;
        ringsize = len;
    }

    void allocate(csnd::Csound *cs, size_t bsize)
    {
        front = 0;
        back = 0;
        ringsize = bsize;
        _data.allocate(cs, bsize);
    }

    size_t size(size_t *__back, size_t *__front)
    {
        size_t _back = back, _front = front, _size;
        if(back >= _front)
            _size = _back - _front;
        else 
            _size = ringsize - _front + _back;
        
        if(__back) * __back = _back;
        if(__front) *__front = _front;
        return _size;

    }

    bool push(const T *buf)
    {
        size_t _back, _front, _size; 
        _size = size(&_back, &_front);
        if(_size < ringsize - 1)
        {
            _data[_back] = *buf;
            back = (back + 1) % ringsize;
            return true;
        }

        return false;
    }
    bool pop(T *buf, double *timestamp) 
    {
        size_t _back, _front, _size;
        _size = size(&_back, &_front);
        if(_size == 0)
            return false;
        
        *buf = _data[_front];
        front = (front + 1) % ringsize;
        return true;

    }

    size_t front, back, ringsize;
    csnd::AuxMem<T> _data;
};

struct cs_midi_msg
{
    const unsigned char* data;
    size_t size;
    double timestamp;
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
    RtMidiInDispatcher(csnd::Csound *cs, size_t port, size_t api, csnd::AuxMem<unsigned char> *_buf, cs_queue<cs_midi_msg> *_q)
    {
        buf = _buf;
        queue = _q;
        queue->ringsize = 100;
        midiin = rtmidi_in_create(get_api_from_index(api), "Client Name", 1024);
        if(rtmidi_get_port_count(midiin) == 0) {
            cs->init_error("No port found");
        }

        rtmidi_open_port(midiin, port, "Input Port");
        rtmidi_in_ignore_types(midiin, false, true, false);

        void * uData = (void *)queue;
        rtmidi_in_set_callback(midiin, [](double timestamp, const unsigned char *msg, size_t msg_size, void *userData) {
            cs_queue<cs_midi_msg> *q = (cs_queue<cs_midi_msg>*)userData;
            cs_midi_msg _msg{msg, msg_size, timestamp};
            q->push(&_msg);
            
        }, uData);        
    }

    /*    
    // Physical port
    RtMidiInDispatcher(csnd::Csound * cs, size_t port, size_t api)
        : queue(cs, 100)
    {
        buf.allocate(cs, 1024);

        queue.ringsize = 100;

        std::cout << "Allocated" << std::endl;
        midiin = rtmidi_in_create(get_api_from_index(api), "Client Name", 1024);
        std::cout << "Created" << std::endl;

        if(rtmidi_get_port_count(midiin) == 0) {
            cs->init_error("No port found");
        }

        rtmidi_open_port(midiin, port, "Input Port");
        rtmidi_in_ignore_types(midiin, false, true, false);

        queue.ringsize = 100;
        void * uData = (void *)&queue;
        rtmidi_in_set_callback(midiin, [](double timestamp, const unsigned char *msg, size_t msg_size, void *userData) {
            cs_queue<cs_midi_msg> *q = (cs_queue<cs_midi_msg>*)userData;
            cs_midi_msg _msg{msg, msg_size, timestamp};
            q->push(&_msg);
            
        }, uData);        
    }
    */

    // Virtual Port
    RtMidiInDispatcher(csnd::Csound * cs, std::string port, size_t api, csnd::AuxMem<unsigned char> *_buf, cs_queue<cs_midi_msg> *_q)
    {
        buf = _buf;
        queue = _q;
        queue->ringsize = 100;
        midiin = rtmidi_in_create(get_api_from_index(api), "Client Name", 1024);
        if(rtmidi_get_port_count(midiin) == 0) {
            cs->init_error("No port found");
        }

        rtmidi_open_virtual_port(midiin, port.c_str());
        rtmidi_in_ignore_types(midiin, false, true, false);

        void * uData = (void *)queue;
        rtmidi_in_set_callback(midiin, [](double timestamp, const unsigned char *msg, size_t msg_size, void *userData) {
            cs_queue<cs_midi_msg> *q = (cs_queue<cs_midi_msg>*)userData;
            cs_midi_msg _msg{msg, msg_size, timestamp};
            q->push(&_msg);
            
        }, uData);        
    }

    cs_queue<cs_midi_msg> &get_queue() {return (cs_queue<cs_midi_msg>&) *queue;}

    void use() {
        cs_midi_msg _msg{buf->data(), buf->len(), 0};
        use_count = (use_count + 1) % suscribed;
        if(use_count == 0) {
            while(queue->pop(&_msg, &stamp)) {}
        }
    }
    void suscribe() {suscribed++;}
    void unsuscribe() {if(suscribed > 0) suscribed--;}

private:
    double stamp;
    size_t use_count = 0;
    size_t suscribed = 0;

    RtMidiInPtr midiin;
    csnd::AuxMem<unsigned char> *buf;
    cs_queue<cs_midi_msg> *queue;
};

struct rawmidi_in_open : csnd::Plugin<1, 2>
{
    int init() {
        buf.allocate(csound, 1024);
        dispatcher_buf.allocate(csound, 1024);
        dispatcher_queue.allocate(csound, 100);
        midiin = new RtMidiInDispatcher(csound, (size_t)inargs[0], (size_t)inargs[1], &dispatcher_buf, &dispatcher_queue);
        intptr_t ptr = reinterpret_cast<intptr_t>(midiin);
        outargs[0] = ptr;
        return OK;
    }

    double stamp;
    csnd::AuxMem<unsigned char> buf;

    csnd::AuxMem<unsigned char> dispatcher_buf;
    cs_queue<cs_midi_msg> dispatcher_queue;
    RtMidiInDispatcher *midiin = nullptr;
};


struct rawmidi_virtual_in_open : csnd::Plugin<1, 2>
{
    int init() {
        buf.allocate(csound, 1024);
        dispatcher_buf.allocate(csound, 1024);
        dispatcher_queue.allocate(csound, 100);
        midiin = new RtMidiInDispatcher(csound, inargs.str_data(0).data, (size_t)inargs[1], &dispatcher_buf, &dispatcher_queue);
        intptr_t ptr = reinterpret_cast<intptr_t>(midiin);
        outargs[0] = ptr;
        return OK;
    }
    double stamp;
    csnd::AuxMem<unsigned char> buf;

    csnd::AuxMem<unsigned char> dispatcher_buf;
    cs_queue<cs_midi_msg> dispatcher_queue;
    RtMidiInDispatcher *midiin = nullptr;
};

struct rawmidi_virtual_out_open : csnd::Plugin<1, 2>
{
    int init() {
        if(in_count() > 1)
            midiout = rtmidi_out_create(get_api_from_index(static_cast<int>(inargs[1])), "Client Name"); //new RtMidiOut(get_api_from_index(static_cast<int>(inargs[1])));
        else
            midiout = rtmidi_out_create_default();

        unsigned int nports = rtmidi_get_port_count(midiout); //midiout->getPortCount();
        if(nports == 0) {
           csound->init_error("No ports available");
           return NOTOK;
        }
        
        rtmidi_open_virtual_port(midiout, inargs.str_data(0).data);
        //midiout->openVirtualPort(inargs.str_data(0).data);
        outargs[0] = reinterpret_cast<intptr_t>(midiout);
        return OK;
    }

    RtMidiOutPtr midiout;
    //RtMidiOut *midiout = nullptr;
};

struct rawmidi_out_open : csnd::Plugin<1, 2>
{
    int init() {
        if(in_count() > 1)
            midiout = rtmidi_out_create(get_api_from_index(static_cast<int>(inargs[1])), "Client Name"); //new RtMidiOut(get_api_from_index(static_cast<int>(inargs[1])));
        else
            midiout = rtmidi_out_create_default();

        unsigned int nports = rtmidi_get_port_count(midiout); //midiout->getPortCount();
        if(nports == 0) {
           csound->init_error("No ports available");
           return NOTOK;
        }
        size_t port_num = (int)inargs[0];
        if(port_num >= nports) {
            csound->init_error("Port not available" );
            return NOTOK;
        }

        rtmidi_open_port(midiout, port_num, "Outport Name");
        outargs[0] = reinterpret_cast<intptr_t>(midiout);
        return OK;
    }

    RtMidiOutPtr midiout;
};

template<typename T>
inline bool is_power_of_two(T n)
{
    if(n <= 0) return false;
    T i = n & (n - 1);
    return (i == 0) ? true : false;
}

struct rawmidi_in : csnd::Plugin<3, 2>
{
    int init() {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)inargs[0]);
        midiin = reinterpret_cast<RtMidiInDispatcher *>(ptr);
        if(midiin == nullptr) {
            csound->init_error("Rawmidi -> Midi device is not found, make sure to pass a valid handle initialized with rawmidi_open____");
            return NOTOK;
        }
        midiin->suscribe();
        csound->plugin_deinit(this);

        queue.allocate(csound, 100);

        size_t bufsize = DEFAULT_IN_BUFFER_SIZE * DEFAULT_IN_BUFFER_COUNT;
        if(in_count() > 1) 
            bufsize = (int)inargs[1];
        
        outargs.myfltvec_data(2).init(csound, bufsize);
        buf.allocate(csound, DEFAULT_IN_BUFFER_SIZE * DEFAULT_IN_BUFFER_COUNT);
        tmp_buf.allocate(csound, DEFAULT_IN_BUFFER_SIZE * DEFAULT_IN_BUFFER_COUNT);
        return kperf();
    }

    int deinit()
    {
        midiin->unsuscribe();
        return OK;
    }

    int kperf() {
        cs_queue<cs_midi_msg>& receiver_queue = midiin->get_queue(); 
        midiin->use();
        receiver_queue.front = front;
        cs_midi_msg _msg{tmp_buf.data(), tmp_buf.len(), 0};
        while(receiver_queue.pop(&_msg, &stamp)) {
            queue.push(&_msg);
        }
        front = receiver_queue.front;
        if(!queue.pop(&_msg, &stamp)) 
        {
            outargs[0] = 0;
            std::cout << "nothing" << std::endl;
            return OK;
        }
        outargs[0] = 1;
        outargs[1] = _msg.size;
        std::cout << "msg size " << _msg.size << std::endl;
        for(size_t i = 0; i < _msg.size; ++i)
            outargs.myfltvec_data(2).data_array()[i] = _msg.data[i];
        return OK;
    }

    csnd::AuxMem<unsigned char> buf, tmp_buf;
    cs_queue<cs_midi_msg> queue;
    double stamp;
    RtMidiInDispatcher *midiin;
    unsigned int front;
};

// Get the last 7 bits of a byte in a buffer
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
        buf.allocate(csound, 1024);
        intptr_t ptr = reinterpret_cast<intptr_t>((long)args[0]);
        midiout = reinterpret_cast<RtMidiOutPtr>(ptr);
        if(midiout == nullptr)
        {
            csound->init_error("Pointer to midi device is null");
            return NOTOK;
        }
        return kperf();
    }

    int kperf()
    {
        const int msg_len = args[1]; 
        for(size_t i = 0; i < msg_len; ++i)
        {
            buf[i] = args.myfltvec_data(2)[i];
            if(i > 0 && i < (args[1]-1))
                buf[i] = buf[i] & 0b01111111;
        }

        rtmidi_out_send_message(midiout, buf.data(), msg_len);
        return OK;
    }


    csnd::AuxMem<unsigned char>buf;
    RtMidiOutPtr midiout;
    double stamp;
};

template<uint8_t status>
struct rawmidi_3bytes_out : csnd::InPlug<4>
{
    int init() {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)args[0]);
        midiout = reinterpret_cast<RtMidiOutPtr>(ptr);
        msg.allocate(csound, 3);
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
        rtmidi_out_send_message(midiout, msg.data(), 3);
        return OK;
    }


    uint8_t status_byte;
    // To 255 for equality check >
    uint8_t channel = 255;
    uint8_t byte2 = 255, byte3 = 255;
    RtMidiOutPtr midiout;
    csnd::AuxMem<unsigned char> msg;
};

template<uint8_t status>
struct rawmidi_2bytes_out : csnd::InPlug<3>
{
    int init() {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)args[0]);
        midiout = reinterpret_cast<RtMidiOutPtr>(ptr);
        msg.allocate(csound, 2);
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
        rtmidi_out_send_message(midiout, msg.data(), 2);
        return OK;
    }

    uint8_t status_byte;
    // To 255 for equality check >
    uint8_t channel = 255;
    uint8_t byte2 = 255;
    RtMidiOutPtr midiout;
    csnd::AuxMem<unsigned char> msg;
};

struct rawmidi_noteon_out : public rawmidi_3bytes_out<STATUS_NOTEON> {};
struct rawmidi_noteoff_out : public rawmidi_3bytes_out<STATUS_NOTEOFF> {};
struct rawmidi_cc_out : public rawmidi_3bytes_out<STATUS_CC> {};
struct rawmidi_pitchbend_out : public rawmidi_3bytes_out<STATUS_PITCHBEND> {};
struct rawmidi_polyaftertouch_out : public rawmidi_3bytes_out<STATUS_POLYAFTERTOUCH> {};
struct rawmidi_programchange_out : public rawmidi_2bytes_out<STATUS_PROGRAMCHANGE> {};
struct rawmidi_aftertouch_out : public rawmidi_2bytes_out<STATUS_AFTERTOUCH> {};

template<uint8_t status>
struct rawmidi_3bytes_in : csnd::Plugin<4,3>
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
        csound->plugin_deinit(this);

        requested_channel = 255;
        requested_ident = 255;

        buf.allocate(csound, 128);
        tmp_buf.allocate(csound, 128);

        queue.allocate(csound, 100);
        queue.ringsize = 100;

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

    int deinit()
    {
        midiin->unsuscribe();
        return OK;
    }

    int kperf()
    {
        cs_queue<cs_midi_msg> &receiver_queue = midiin->get_queue();

        midiin->use();
        receiver_queue.front = front;
        cs_midi_msg _msg{tmp_buf.data(), tmp_buf.len(), 0};

        while(receiver_queue.pop(&_msg, &stamp)) {
            _msg.timestamp = stamp;
            //msg.bytes = tmp_buf;
            queue.push(&_msg);
        }
        front = receiver_queue.front;

        // Check for first relevant message in the queue
        while(queue.pop(&_msg, &stamp)) {
            if( (_msg.data[0] & 0xF0) == status) {
                uint8_t channel = (uint8_t(_msg.data[0]) & 0b00001111) + 1;
                uint8_t byte1 = uint8_t(_msg.data[1]) & 0b01111111;
                uint8_t byte2 = uint8_t(_msg.data[2]) & 0b01111111;

                if(requested_channel < 17 && (channel != requested_channel))
                    continue;
                if(requested_ident < 128 && (byte1 != requested_ident))
                    continue;
                outargs[0] = 1; // changed
                outargs[1] = (MYFLT)channel;
                outargs[2] = (MYFLT)byte1;
                outargs[3] = (MYFLT)byte2;
                return OK;
            }
        }
        outargs[0] = 0; // No change until found one 
        return OK;
    }

    uint8_t requested_channel, requested_ident;
    double stamp;
    
    csnd::AuxMem<unsigned char> buf, tmp_buf;
    
    RtMidiInDispatcher *midiin;
    cs_queue<cs_midi_msg> queue;
    
    unsigned int front;
};


template<uint8_t status>
struct rawmidi_2bytes_in : csnd::Plugin<3,2>
{
    int init()
    {
        intptr_t ptr = reinterpret_cast<intptr_t>((long)inargs[0]);
        midiin = reinterpret_cast<RtMidiInDispatcher *>(ptr);
        if(midiin == nullptr) {
            csound->init_error("Rawmidi -> Midi device is not found, make sure to pass a valid handle initialized with rawmidi_open_ in or out");
            return NOTOK;
        }

        midiin->suscribe();
        csound->plugin_deinit(this);

        requested_channel = 255;
        requested_ident = 255;

        buf.allocate(csound, 128);
        tmp_buf.allocate(csound, 128);
        queue.allocate(csound, 100);
        queue.ringsize = 100;

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

    int deinit()
    {
        midiin->unsuscribe();
        return OK;
    }

    int kperf()
    {
        cs_queue<cs_midi_msg> &receiver_queue = midiin->get_queue();

        midiin->use();
        receiver_queue.front = front;
        cs_midi_msg _msg{tmp_buf.data(), tmp_buf.len(), 0};
        while(receiver_queue.pop(&_msg, &stamp)) {
            _msg.timestamp = stamp;
            queue.push(&_msg);
        }
        front = receiver_queue.front;

        // Check for first relevant message in the queue
        while(queue.pop(&_msg, &stamp)) {
            if( (_msg.data[0] & 0xF0) == status) {
                uint8_t channel = (uint8_t(_msg.data[0]) & 0x0F) + 1;
                uint8_t byte1 = uint8_t(_msg.data[1]) & 0b01111111;

                if(requested_channel < 17 && (channel != requested_channel))
                    continue;
                if(requested_ident < 128 && (byte1 != requested_ident))
                    continue;
                outargs[0] = 1;
                outargs[1] = channel;
                outargs[2] = byte1;
                return OK;
            }
        }
        outargs[0] = 0;
        return OK;

        return OK;
    }

    uint8_t requested_channel, requested_ident;
    double stamp;
    csnd::AuxMem<unsigned char> buf, tmp_buf;
    RtMidiInDispatcher *midiin;
    cs_queue<cs_midi_msg> queue;
    unsigned int front;
};

struct rawmidi_noteon_in : rawmidi_3bytes_in<STATUS_NOTEON> {};
struct rawmidi_noteoff_in : rawmidi_3bytes_in<STATUS_NOTEOFF> {};
struct rawmidi_cc_in : rawmidi_3bytes_in<STATUS_CC> {};
struct rawmidi_polyaftertouch_in : rawmidi_3bytes_in<STATUS_POLYAFTERTOUCH> {};
struct rawmidi_pitchbend_in : rawmidi_3bytes_in<STATUS_PITCHBEND> {};
struct rawmidi_programchange_in : rawmidi_2bytes_in<STATUS_PROGRAMCHANGE> {};
struct rawmidi_aftertouch_in : rawmidi_2bytes_in<STATUS_AFTERTOUCH> {};


/**
 *
 *  Embodme - ERAE Touch Functions
 *
 *	* Engine contains list of zone, and is the threaded process that gets transfer
 *  * Zone is a representation of an API zone and its internal components
 *
 *
 *  * Decision to make :
 * 		- Use different zones for different widgets ?
 * 		- Use one zone per view ?
 * 		- 128 zones is enough... so a zone could only be one widget
**/

// For later, if implementing a "one zone UI api"
struct erae_sysex_zone
{

};

#include<thread>
struct erae_sysex_engine
{
    erae_sysex_engine(RtMidiIn *in, RtMidiOut *out, size_t _fps)
        : midiin(in)
        , midiout(out)
        , fps(_fps)
        , sleep_time(1000 / fps)
    {}

    RtMidiIn *midiin;
    RtMidiOut *midiout;
    size_t fps;
    size_t sleep_time; // ms
    std::thread t;
};

struct color {


    uint8_t r, g, b;
    constexpr static const color white() {return {127, 127, 127};}
    constexpr static const color black() {return {0, 0, 0};}
    constexpr static const color red() {return {127, 0, 0};}
    constexpr static const color green() {return {0, 127, 0};}
    constexpr static const color blue() {return {0, 0, 127};}

    void mult(float m) {
        r = (uint8_t) float(r) * m;
        g = (uint8_t) float(g) * m;
        b = (uint8_t) float(b) * m;
    }

    void inv() {
        r = 127 - r;
        g = 127 - g;
        b = 127 - b;
    }
};

struct fingerstream
{
    enum action_t {
        click = 0,
        slide = 1,
        release = 2,
        undefined = 3,
    };

    fingerstream()
        :action(action_t::undefined)
    {}

    action_t action = action_t::undefined;
    uint8_t finger;
    size_t zone;
    float x, y, z;
    uint8_t chksum;
};

struct erae_messages {

    static inline bool is_proper_boundary_reply(csnd::AuxMem<uint8_t> &vec, uint8_t zone)
    {
        std::cout << "Boundary ? " << std::endl;
        if(vec[8] != 0x7F) return false;
        std::cout << "not 8 " << std::endl;
        if(vec[9] != 0x01) return false; 
        std::cout << "not 9 " << std::endl;
        if(vec[10] != zone) return false;
        std::cout << "not 10 " << std::endl;
        return true;
    }
    static inline std::pair<uint8_t, uint8_t> get_boundaries(csnd::AuxMem<uint8_t> &vec)
    {
        return {vec[7], vec[8]};
    }

    static inline bool is_proper_fingerstream(csnd::AuxMem<uint8_t> &vec, uint8_t zone)
    {
        std::cout << "Fingerstream ? " << std::endl;
        std::cout << "vec 8 : " << vec[8] << std::endl;
        if(vec[8] == 0x7F) return false;
        std::cout << "8 ok ? " << std::endl;
        if(vec[9] != zone) return false;
        std::cout << "9 ok , fingersteram ! " << std::endl;
        return true;
    }

    static inline void retrieve_action(uint8_t val, fingerstream &stream) {
        uint8_t action = (val & 0b01110000) >> 4;
        uint8_t finger = (val & 0b00001111);
        if(action == 0b0000)
            stream.action = fingerstream::action_t::click;
        else if(action == 0b0001)
            stream.action = fingerstream::action_t::slide;
        else if(action == 0b0010)
            stream.action = fingerstream::action_t::release;
        else
            stream.action = fingerstream::action_t::undefined;
        stream.finger = finger;
    }
    static inline void get_fingerstream( csnd::AuxMem<uint8_t> &vec, uint8_t width, uint8_t height, fingerstream &stream, csnd::AuxMem<uint8_t> &out_buf)
    {
        stream.chksum = vec[20];
        retrieve_action(vec[4], stream);
        stream.zone = vec[5];
        uint8_t chksum = unbitize7chksum(vec.data() + 6, 14, out_buf.data());
        if(chksum != stream.chksum)
        {
            // Error ? Or not ?
        }

        float * xyz = (float *)out_buf.data();
        stream.x = xyz[0] / (float)width;
        stream.y = xyz[1] / (float)height;
        stream.z = xyz[2];
    }

    static inline void get_fingerstream_not_normalized( csnd::AuxMem<uint8_t> &vec, uint8_t width, uint8_t height, fingerstream &stream, csnd::AuxMem<uint8_t> &out_buf)
    {
        stream.chksum = vec[20];
        retrieve_action(vec[4], stream);
        stream.zone = vec[5];
        uint8_t chksum = unbitize7chksum(vec.data() + 6, 14, out_buf.data());
        if(chksum != stream.chksum)
        {
            // Error ? Or not ?
        }

        float * xyz = (float *)out_buf.data();
        stream.x = xyz[0];
        stream.y = xyz[1];
        stream.z = xyz[2];
    }

    template <typename T, typename... U>
    static constexpr auto make_array(U&&... u) {
        return std::array<T, sizeof...(U)>{ { static_cast<T>(u)... } };
    }
    

    static inline size_t open_sysex(csnd::AuxMem<uint8_t> *data_ptr, uint8_t receiver_bytes)
    {
        auto open_sysex_msg = erae_messages::make_array<uint8_t>(0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x02, 0x01, 0x01, 0x04, 0x01, receiver_bytes, 0xF7);
        std::memcpy(data_ptr->data(), &open_sysex_msg, open_sysex_msg.size());
        return open_sysex_msg.size();
    }

    static inline size_t close_sysex(csnd::AuxMem<uint8_t> *data_ptr)
    {
        auto close_sysex_msg = erae_messages::make_array<uint8_t>(0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x02, 0x01, 0x01, 0x04, 0x02, 0xF7);
        std::memcpy(data_ptr->data(), &close_sysex_msg, close_sysex_msg.size());
        return close_sysex_msg.size();
    }

    static inline size_t boundary_request(csnd::Csound *cs, csnd::AuxMem<uint8_t> *data_ptr, uint8_t zone)
    {
        //F0 00 21 50 00 01 00 01 01 01 04 10 ZONE F7
        auto boundary_request_msg = erae_messages::make_array<uint8_t>(0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x02, 0x01, 0x01, 0x04, 0x10, zone, 0xF7);
        if(data_ptr->len() < boundary_request_msg.size())
            data_ptr->allocate(cs, boundary_request_msg.size());
        std::memset(data_ptr->data(), 0, data_ptr->len());
        std::memcpy(data_ptr->data(), &boundary_request_msg, boundary_request_msg.size() * sizeof(uint8_t));
        return boundary_request_msg.size();
    }

    static inline size_t draw_rectangle(csnd::Csound *cs, csnd::AuxMem<uint8_t> *data_ptr, uint8_t zone, uint8_t x, uint8_t y, uint8_t w, uint8_t h, color c)
    {
       //F0 00 21 50 00 01 00 01 01 01 04 22 ZONE XPOS YPOS WIDTH HEIGHT RED GREEN BLUE F7
        auto draw_rect_msg = erae_messages::make_array<uint8_t>(0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x02, 0x01, 0x01, 0x04, 0x22, zone, x, y, w, h, c.r, c.g, c.b, 0xF7);
        if(data_ptr->len() < draw_rect_msg.size())
            data_ptr->allocate(cs, draw_rect_msg.size());
        std::memset(data_ptr->data(), 0, data_ptr->len());
        std::memcpy(data_ptr->data(), &draw_rect_msg, draw_rect_msg.size());
        return draw_rect_msg.size();
    }

    static inline size_t clear_zone(csnd::AuxMem<uint8_t> *data_ptr, uint8_t zone)
    {
       //F0 00 21 50 00 01 00 01 01 01 04 20 ZONE F7
        auto clear_msg = erae_messages::make_array<uint8_t>(0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0X02, 0x01, 0x01, 0x04, 0x20, zone, 0xF7);
        std::memcpy(data_ptr->data(), &clear_msg, clear_msg.size());
        return clear_msg.size();
    }

    static inline size_t draw_pixel(csnd::AuxMem<uint8_t> *data_ptr, uint8_t zone, uint8_t x, uint8_t y, color c)
    {
        //F0 00 21 50 00 01 00 01 01 01 04 21 ZONE XPOS YPOS RED GREEN BLUE F7
        //F0 00 21 50 00 01 00 01 01 01 04 21 ZONE XPOS YPOS RED GREEN BLUE F7
        auto draw_pix_msg  = erae_messages::make_array<uint8_t>(0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x02, 0x01, 0x01, 0x04, 0x21, zone, x, y, c.r, c.g, c.b, 0xF7);
        std::memcpy(data_ptr->data(), &draw_pix_msg, draw_pix_msg.size());
        return draw_pix_msg.size();
    }

    static inline size_t draw_circle(csnd::AuxMem<uint8_t> *data_ptr, uint8_t zone, uint8_t x, uint8_t y, uint8_t rad, color c)
    {
        size_t total = 0;
        size_t npoints = rad * 5;
        size_t angle_increment = 360 / npoints;
        for(size_t i = 0; i < npoints; ++i)
        {
            size_t angle = angle_increment * i;
            uint8_t xp = x + rad * std::cos(angle);
            uint8_t yp = y +rad * std::sin(angle);
            auto draw_pix_msg  = erae_messages::make_array<uint8_t>(0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x02, 0x01, 0x01, 0x04, 0x21, zone, xp, yp, c.r, c.g, c.b, 0xF7);
            ::memcpy(data_ptr->data() + (i*19), &draw_pix_msg, draw_pix_msg.size());
            //size_t s = draw_pixel(data_ptr->data() + (i*19) , zone, xp, yp, c);
            total += draw_pix_msg.size();
        }
        return total;
    }

};


// WPZ = Widget per zone, as "one widget per zone", each zone contains max 1 widget that expands inside
struct erae_wpz_base
{
    void prepare(csnd::Csound *cs, size_t insize)
    {
        std::cout << "XYZ base  : alloc queue " << std::endl;
        queue.allocate(cs, 100);
        //queue.ringSize = 100;
        //queue.ring = new MidiInApi::MidiMessage[ queue.ringSize ];
        std::cout << "XYZ base  : alloc buf " << std::endl;
        buf.allocate(cs, 1024);
        std::cout << "XYZ base  : alloc tmp " << std::endl;
        tmp_buf.allocate(cs, 1024);
        size_t s = unbitized7size(insize);
        std::cout << "XYZ base  : alloc out " << std::endl;
        out_buf.allocate(cs, s);
        std::cout << "XYZ base  : midi suscribe " << std::endl;
        midiin->suscribe();
        std::cout << "XYZ base  : deinit set " << std::endl;
        //cs->plugin_deinit(this);

        fps = 30;
        samples_to_refresh = cs->sr() / fps;
        std::cout << "XYZ base  : prepare ok " << std::endl;
    }


    void set_fps(csnd::Csound *cs, uint8_t _fps)
    {
        fps = _fps;
        samples_to_refresh = cs->sr() / fps;
    }

    void get_data() {
        cs_queue<cs_midi_msg> &receiver_queue = midiin->get_queue();
        //MidiInApi::MidiQueue &receiver_queue = midiin->get_queue();
        //MidiInApi::MidiQueue copy(receiver_queue);

        midiin->use();
        receiver_queue.front = front;
        cs_midi_msg _msg{buf.data(), buf.len(), 0};
        while(receiver_queue.pop(&_msg, &stamp)) {
            _msg.timestamp = stamp;
            queue.push(&_msg);
            std::cout << "push to internal queue "  << std::endl;
        }
        front = receiver_queue.front;
    }

    RtMidiInDispatcher *midiin;
    //RtMidiOut *midiout;
    RtMidiOutPtr midiout;
    cs_queue<cs_midi_msg> queue;
    
    //MidiInApi::MidiQueue queue;
    uint8_t zone;

    size_t front;
    double stamp;
    csnd::AuxMem<unsigned char> buf, tmp_buf;

    //std::vector<unsigned char> buf, tmp_buf;
    size_t width, height;
    fingerstream stream_data;
    csnd::AuxMem<uint8_t> out_buf;


    uint8_t fps = 30;
    size_t samples_to_refresh;
    size_t increment = 0;
};


/**
 * @brief The erae_wpz_xyz struct is a simple xyz pad
 * @arg midiin
 * @arg midiout
 * @arg zone
 * @arg optional color (rgb array)
 *
 * @return changed (0 or 1)
 * @return x
 * @return y
 * @return z
 * @return action
 * @return finger
 *
**/
struct erae_wpz_xyz : csnd::Plugin<6, 4>, erae_wpz_base
{
    // Draw rectangle F0 00 21 50 00 01 00 01 01 01 04 22 ZONE XPOS YPOS WIDTH HEIGHT RED GREEN BLUE F7
    // Request boundary in init
    int init()
    {
        std::cout << "XYZ Init " << std::endl;
        midiin = reinterpret_cast<RtMidiInDispatcher *>((intptr_t)inargs[0]);
        midiout = rtmidi_out_create(get_api_from_index(static_cast<int>(inargs[1])), "Client Name");

        std::cout << "XYZ Created,  " << std::endl;
        zone = inargs[2];
        std::cout << "XYZ checking color " << std::endl;
        if(in_count() > 3) {
            csnd::Vector<MYFLT> vec = inargs.vector_data<MYFLT>(3);
            c = color{uint8_t(vec[0]), uint8_t(vec[1]), uint8_t(vec[2])};
            c.mult(0.7);
        }
        inv_color = color{uint8_t(127-c.r), uint8_t(127-c.g), uint8_t(127-c.b) };
        std::cout << "XYZ Prepare : " << std::endl;
        prepare(csound, 14);

        // Need to take care of that with a pointer or something
        //std::vector<uint8_t> msg = erae_messages::boundary_request(zone);
        std::cout << "XYZ Check boundaries " << std::endl;
        size_t s = erae_messages::boundary_request(csound, &boundary, zone);
        std::cout << "XYZ sending boundary request " << std::endl;
        rtmidi_out_send_message(midiout, boundary.data(), s);
        //midiout->sendMessage(&msg);

        std::cout << "XYZ screen alloc " << std::endl;
        screen.allocate(csound, 1024);

        for(size_t i = 0; i < 10; ++i)
            fingerlist[i].action = fingerstream::action_t::undefined;

        std::cout << "XYZ Init OK" << std::endl;

        csound->plugin_deinit(this);
        return OK;
    }

    int deinit() {
        midiin->unsuscribe();
        return OK;
    }

    void redraw() {
        std::cout << "redraw " << std::endl;
        //screen.clear();
        std::memset(screen.data(), 0, screen.len());
        
        size_t total = erae_messages::draw_rectangle(csound, &screen, zone, 0, 0, width, height, c);
        std::cout << "\n\nStart loop" << std::endl;
        for(size_t i = 0; i < 10; ++i)
        {
            std::cout << i << " finger : " << (int)fingerlist[i].finger << " - action " << (int)fingerlist[i].action << std::endl;
            if(fingerlist[i].action == fingerstream::action_t::click || fingerlist[i].action == fingerstream::action_t::slide) {
                color cross_c = inv_color; //color::white();
                cross_c.mult(stream_data.z);

                size_t s = erae_messages::draw_rectangle(csound, &rect, zone, fingerlist[i].x * width,  0, 1, height, cross_c);
                for(size_t i = 0; i < s; ++i)
                    screen[i+total] = rect[i];
                total += s;
                s = erae_messages::draw_rectangle(csound, &rect, zone, 0, fingerlist[i].y * height, width, 1, cross_c);
                for(size_t i = 0; i < s; ++i)
                    screen[i+total] = rect[i];
            }
        }
        rtmidi_out_send_message(midiout, screen.data(), screen.len());
    }

    int kperf()
    {
        outargs[0] = 0;
        get_data();
        cs_midi_msg _msg{buf.data(), buf.len(), 0};
        if(!queue.pop(&_msg, &stamp)) {
            return OK;
        }
        std::cout << "has message " << buf[0]  << std::endl;
        if(erae_messages::is_proper_boundary_reply(buf, zone)) {
            std::cout << "boundary"  << std::endl;
            //std::pair<uint8_t, uint8_t> boundaries = erae_messages::get_boundaries(buf);
            width = buf[11];
            height = buf[12];
            // Draw
            erae_messages::draw_rectangle(csound, &rect, zone, 0, 0, width, height, c);
            //midiout->sendMessage(&rect);
            rtmidi_out_send_message(midiout, rect.data(), rect.len());
        } else if(erae_messages::is_proper_fingerstream(buf, zone)) {
            std::cout << "fingerstream" << std::endl;
            erae_messages::get_fingerstream( buf, width, height, stream_data, out_buf);
            outargs[0] = 1;
            outargs[1] = stream_data.x;
            outargs[2] = stream_data.y;
            outargs[3] = stream_data.z;
            outargs[4] = static_cast<int>(stream_data.action);
            outargs[5] = static_cast<int>(stream_data.finger);

            fingerstream &cur = fingerlist[stream_data.finger];
            ::memcpy(&cur, &stream_data, sizeof(fingerstream));
            if(increment > samples_to_refresh)
            {
                redraw();
                increment = 0;
             }
            increment += csound->get_csound()->GetKsmps(csound->get_csound());
        }
        return OK;
    }

    color c = color::blue();
    color inv_color;
    fingerstream fingerlist[10];
    //std::vector<uint8_t> screen;

    csnd::AuxMem<uint8_t> screen, rect, boundary;
};

/**
 * @brief The erae_wpz_matrix struct is a matrix for Erae touch
 * @arg midiin
 * @arg midiout
 * @arg zone
 * @arg nrows
 * @arg ncols
 *
 * @return changed : 1 or 0
 * @return Matrix as array of arrays
 * @return row
 * @return col
 * @return index (in array)
**/
// Configurable matrix

/*
struct erae_wpz_matrix : csnd::Plugin<5, 8>, erae_wpz_base
{

    struct cell_state {
        bool activated;
        float value;
        double time;
        bool is_double;
    };

    int init()
    {
        midiin = reinterpret_cast<RtMidiInDispatcher *>((intptr_t)inargs[0]);
        midiout = reinterpret_cast<RtMidiOut *>((intptr_t)inargs[1]);
        zone = inargs[2];
        nrows = static_cast<uint8_t>(inargs[3]);
        ncols = static_cast<uint8_t>(inargs[4]);

        csnd::Vector<MYFLT> &outs = outargs.vector_data<MYFLT>(0);
        outs.init(csound, nrows * ncols);

        rectsize = static_cast<uint8_t>(inargs[7]);

        csnd::Vector<MYFLT> basecol_vec = inargs.vector_data<MYFLT>(5);
        csnd::Vector<MYFLT> selcol_vec = inargs.vector_data<MYFLT>(6);
        basecolor = color{uint8_t(basecol_vec[0]), uint8_t(basecol_vec[1]), uint8_t(basecol_vec[2])};
        selcolor = color{uint8_t(selcol_vec[0]), uint8_t(selcol_vec[1]), uint8_t(selcol_vec[2])};

        prepare(csound, 14);

        // Request boundaries :
        std::vector<uint8_t> msg = erae_messages::boundary_request(zone);
        midiout->sendMessage(&msg);

        screen.resize(1024);
        selection_list.allocate(csound, nrows * ncols);

        return kperf();
    }

    void redraw()
    {
        screen.clear();
        color c = basecolor;
        uint8_t posx = 0, posy = 0;
        for(size_t row = 0; row < nrows; ++row) {
            posy = row * 2;
            for(size_t col = 0; col < ncols; ++col) {
                c = basecolor;
                if((col+row)%2 != 0)
                    c.mult(0.2);

                size_t index = (ncols * row) + col;
                if(selection_list[index].activated) {
                    c = selcolor;
                    c.mult(selection_list[index].value);
                }

                posx = col * 2;
                std::vector<uint8_t> rect = erae_messages::draw_rectangle(zone, posx, posy, rectsize, rectsize, c);
                for(size_t i = 0; i < rect.size(); ++i)
                    screen.push_back(rect[i]);
            }
        }
        midiout->sendMessage(&screen);
    }

    inline bool is_short_click(double t1, double t2)
    {
        constexpr static const double short_click_time = 200.0;
        return (t2 - t1) * 1000 < short_click_time;
    }

    int kperf()
    {
        get_data();
        if(!queue.pop(&buf, &stamp)) {
            outargs[0] = 0;
            return OK;
        }
        if(erae_messages::is_proper_boundary_reply(buf, zone)) {
            std::pair<uint8_t, uint8_t> boundaries = erae_messages::get_boundaries(buf);
            width = boundaries.first;
            height = boundaries.second;
            // Draw
            redraw();
        } else if(erae_messages::is_proper_fingerstream(buf, zone)) {
            // Find which square is picked

            erae_messages::get_fingerstream_not_normalized( buf, width, height, stream_data, out_buf);

            size_t col = std::floor(stream_data.x / rectsize);
            size_t row = std::floor(stream_data.y / rectsize);
            if(col >= ncols || row >= nrows)
                return OK;
            size_t index = (ncols * row) + col;

            switch (stream_data.action) {
            case fingerstream::action_t::click:
            {
                // if not short click : activate
                // If double click : set value to 1
                selection_list[index].activated = true;
                selection_list[index].value = stream_data.z;
                selection_list[index].time = csound->current_time_seconds();
                break;
            }
            case fingerstream::action_t::slide:
            {
                selection_list[index].value = stream_data.z;
                break;
            }
            case fingerstream::action_t::release:
            {
                if(is_short_click(selection_list[index].time, csound->current_time_seconds()))
                    selection_list[index].activated = false;
                break;
            }
            case fingerstream::action_t::undefined:
            {break;}
            }

            if(!selection_list[index].activated)
                selection_list[index].value = 0;

            csnd::Vector<MYFLT> &outs = outargs.vector_data<MYFLT>(1);
            outs[index] = selection_list[index].value;
            outargs[0] = 1;
            outargs[2] = row;
            outargs[3] = col;
            outargs[4] = index;

            if(increment > samples_to_refresh)
            {
                redraw();
                increment = 0;
             }
            increment += csound->get_csound()->GetKsmps(csound->get_csound());
        } else {
            outargs[0] = 0;
        }
        return OK;
    }

    uint8_t nrows, ncols;
    color basecolor, selcolor;
    uint8_t rectsize;
    std::vector<uint8_t> screen;
    csnd::AuxMem<cell_state> selection_list;
};


struct erae_wpz_matrix_dyn : csnd::Plugin<5, 9>, erae_wpz_base
{

    struct cell_state {
        bool activated;
        float value;
        double time;
        bool is_double;
    };

    int init()
    {
        midiin = reinterpret_cast<RtMidiInDispatcher *>((intptr_t)inargs[0]);
        midiout = reinterpret_cast<RtMidiOut *>((intptr_t)inargs[1]);
        zone = inargs[2];
        nrows = static_cast<uint8_t>(inargs[3]);
        ncols = static_cast<uint8_t>(inargs[4]);

        csnd::Vector<MYFLT> &outs = outargs.vector_data<MYFLT>(0);
        outs.init(csound, nrows * ncols);

        rectsize = static_cast<uint8_t>(inargs[7]);

        csnd::Vector<MYFLT> basecol_vec = inargs.vector_data<MYFLT>(5);
        csnd::Vector<MYFLT> selcol_vec = inargs.vector_data<MYFLT>(6);
        basecolor = color{uint8_t(basecol_vec[0]), uint8_t(basecol_vec[1]), uint8_t(basecol_vec[2])};
        selcolor = color{uint8_t(selcol_vec[0]), uint8_t(selcol_vec[1]), uint8_t(selcol_vec[2])};

        prepare(csound, 14);

        // Request boundaries :
        std::vector<uint8_t> msg = erae_messages::boundary_request(zone);
        midiout->sendMessage(&msg);

        screen.resize(1024);
        selection_list.allocate(csound, nrows * ncols);

        set_fps(csound, 50);

        return kperf();
    }

    void redraw()
    {
        csnd::Vector<MYFLT> &ctrls = inargs.vector_data<MYFLT>(8);
        screen.clear();
        color c = basecolor;
        uint8_t posx = 0, posy = 0;
        for(size_t row = 0; row < nrows; ++row) {
            double ctrl_val = std::abs(ctrls[row]);
            ctrl_val = (ctrl_val > 1) ? 1 : (ctrl_val < 0) ? 0 : ctrl_val;
            ctrl_val *= 0.9;
            ctrl_val += 0.1;

            posy = row * 2;
            for(size_t col = 0; col < ncols; ++col) {
                c = basecolor;
                if((col+row)%2 != 0)
                    c.mult(0.2);


                size_t index = (ncols * row) + col;
                if(selection_list[index].activated) {
                    c = selcolor;
                    c.mult(selection_list[index].value);
                }
                c.mult(ctrl_val);

                posx = col * 2;
                std::vector<uint8_t> rect = erae_messages::draw_rectangle(zone, posx, posy, rectsize, rectsize, c);
                for(size_t i = 0; i < rect.size(); ++i)
                    screen.push_back(rect[i]);
            }
        }
        midiout->sendMessage(&screen);
    }

    inline bool is_short_click(double t1, double t2)
    {
        constexpr static const double short_click_time = 200.0;
        return (t2 - t1) * 1000 < short_click_time;
    }

    int kperf()
    {
        bool newmessage = true;
        get_data();
        if(!queue.pop(&buf, &stamp)) {
            outargs[0] = 0;
            newmessage = false;
        }
        if(newmessage && erae_messages::is_proper_boundary_reply(buf, zone)) {
            std::pair<uint8_t, uint8_t> boundaries = erae_messages::get_boundaries(buf);
            width = boundaries.first;
            height = boundaries.second;
            // Draw
            redraw();
        } else if(newmessage && erae_messages::is_proper_fingerstream(buf, zone)) {
            // Find which square is picked

            erae_messages::get_fingerstream_not_normalized( buf, width, height, stream_data, out_buf);

            size_t col = std::floor(stream_data.x / rectsize);
            size_t row = std::floor(stream_data.y / rectsize);
            if(col >= ncols || row >= nrows)
                return OK;
            size_t index = (ncols * row) + col;

            switch (stream_data.action) {
            case fingerstream::action_t::click:
            {
                // if not short click : activate
                // If double click : set value to 1
                selection_list[index].activated = true;
                selection_list[index].value = stream_data.z;
                selection_list[index].time = csound->current_time_seconds();
                break;
            }
            case fingerstream::action_t::slide:
            {
                selection_list[index].value = stream_data.z;
                break;
            }
            case fingerstream::action_t::release:
            {
                if(is_short_click(selection_list[index].time, csound->current_time_seconds()))
                    selection_list[index].activated = false;
                break;
            }
            case fingerstream::action_t::undefined:
            {break;}
            }

            if(!selection_list[index].activated)
                selection_list[index].value = 0;

            csnd::Vector<MYFLT> &outs = outargs.vector_data<MYFLT>(1);
            outs[index] = selection_list[index].value;
            outargs[0] = 1;
            outargs[2] = row;
            outargs[3] = col;
            outargs[4] = index;

        } else {
            outargs[0] = 0;
        }
        if(increment > samples_to_refresh)
        {
            redraw();
            increment = 0;
         }
        increment += csound->get_csound()->GetKsmps(csound->get_csound());
        return OK;
    }

    uint8_t nrows, ncols;
    color basecolor, selcolor;
    uint8_t rectsize;
    std::vector<uint8_t> screen;
    csnd::AuxMem<cell_state> selection_list;
};
*/

/**
 * @brief Retrieve from matrix array
 *
 * @arg array
 * @arg ncols
 * @arg row
 * @arg col
 *
 * @return value
**/

/*
struct erae_wpz_matrix_getvalue : csnd::Plugin<1, 4>
{
    int init() {return OK;}

    int kperf()
    {
        size_t index = (inargs[1] * inargs[2]) + inargs[3];
        csnd::Vector<MYFLT> v = inargs.vector_data<MYFLT>(0);
        outargs[0] = v[index];
        return OK;
    }
};


struct erae_sysex_open : csnd::InPlug<4>
{
    int init()
    {
        RtMidiOut *midiout = reinterpret_cast<RtMidiOut *>(intptr_t(args[0]));
        if(midiout == nullptr)
        {
            csound->init_error("Midi Out is null");
            return NOTOK;
        }

        uint8_t receiver, prefix, bytes;
        receiver = static_cast<uint8_t>(args[1]);
        prefix = static_cast<uint8_t>(args[2]);
        bytes = static_cast<uint8_t>(args[3]);

        std::vector<uint8_t> v = erae_messages::open_sysex(receiver, prefix, bytes);
        midiout->sendMessage(&v);

        return OK;
    }

};

struct erae_sysex_close : csnd::InPlug<1>
{
    int init()
    {
        RtMidiOut *midiout = reinterpret_cast<RtMidiOut *>(intptr_t(args[0]));
        if(midiout == nullptr)
        {
            csound->init_error("Midi Out is null");
            return NOTOK;
        }

        std::vector<uint8_t> v = erae_messages::close_sysex();
        midiout->sendMessage(&v);

        return OK;
    }
};

struct erae_sysex_clear_zone : csnd::InPlug<2>
{

    int init()
    {
        RtMidiOut *midiout = reinterpret_cast<RtMidiOut *>(intptr_t(args[0]));
        if(midiout == nullptr)
        {
            csound->init_error("Midi Out is null");
            return NOTOK;
        }
        uint8_t zone = static_cast<uint8_t>(args[1]);

        std::vector<uint8_t> v = erae_messages::clear_zone(zone);
        midiout->sendMessage(&v);

        return OK;
    }
};
*/

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

struct sysex_text_to_arr : csnd::Plugin<2, 1>
{
    int init() {
        csnd::Vector<MYFLT> &vec = outargs.vector_data<MYFLT>(1);
        std::string s(inargs.str_data(0).data);
        vec.init(csound, s.size());
        outargs[0] = s.size();
        for(size_t i = 0; i < s.size(); ++i)
            vec[i] = MYFLT(s[i] & 0x7F);
        return OK;
    }
};

struct array_prepend : csnd::Plugin<2, 2>
{
    int init() {
        csnd::Vector<MYFLT> &invec = inargs.vector_data<MYFLT>(1);
        size_t size = invec.len() + 1;
        outargs[0] = size;
        csnd::Vector<MYFLT> &outvec = outargs.vector_data<MYFLT>(1);
        outvec.init(csound, size);
        outvec[0] = inargs[0];
        for(size_t i = 0; i < invec.len(); ++i)
            outvec[i+1] = invec[i];
        return OK;
    }
};
struct array_append : csnd::Plugin<2, 2>
{
    int init() {
        csnd::Vector<MYFLT> &invec = inargs.vector_data<MYFLT>(0);
        size_t size = invec.len() + 1;
        outargs[0] = size;
        csnd::Vector<MYFLT> &outvec = outargs.vector_data<MYFLT>(1);
        outvec.init(csound, size);
        for(size_t i = 0; i < invec.len(); ++i)
            outvec[i] = invec[i];
        outvec[size-1] = inargs[1];
        return OK;
    }

};
struct array_concat : csnd::Plugin<2, 2>
{
    int init() {
        csnd::Vector<MYFLT> &invec1 = inargs.vector_data<MYFLT>(0);
        csnd::Vector<MYFLT> &invec2 = inargs.vector_data<MYFLT>(1);
        
        size_t size = invec1.len() + invec2.len();
        outargs[0] = size;
        csnd::Vector<MYFLT> &outvec = outargs.vector_data<MYFLT>(1);
        outvec.init(csound, size);
        size_t cnt = 0;
        for(; cnt < invec1.len(); ++cnt)
            outvec[cnt] = invec1[cnt];
        for(size_t i = 0; i < invec2.len(); ++i, ++cnt)
            outvec[cnt] = invec2[i];
        return OK;
    }

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
    // Print and returns lists of input and output devices corresponding to the specified API
    csnd::plugin<rawmidi_list_devices>(csound, "rawmidi_list_devices", "S[]S[]", "i", csnd::thread::ik);

    /** Opening a device **/
    // ihandle rawmidi_in_open iport_num, [iapi_number]
    csnd::plugin<rawmidi_in_open>(csound, "rawmidi_open_in.ii", "i", "ii", csnd::thread::i);
    // ihandle rawmidi_out_open iport_num, [iapi_number]
    csnd::plugin<rawmidi_out_open>(csound, "rawmidi_open_out.ii", "i", "ii", csnd::thread::i);
    // ihandle rawmidi_in_open Sport_name, [iapi_number]
    csnd::plugin<rawmidi_virtual_in_open>(csound, "rawmidi_open_virtual_in.ii", "i", "Si", csnd::thread::i);
    // ihandle rawmidi_out_open Sport_name, [iapi_number]
    csnd::plugin<rawmidi_virtual_out_open>(csound, "rawmidi_open_virtual_out.ii", "i", "Si", csnd::thread::i);

    /** Input **/
    // ksize, kdata[] rawmidi_in in_handle, [iApi_index]
    csnd::plugin<rawmidi_in>(csound, "rawmidi_in", "kkk[]", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_in>(csound, "rawmidi_in", "iii[]", "im", csnd::thread::i);

    /** Output **/
    // rawmidi_out iout_handle, ksize, kdata[], [iApi_index]
    csnd::plugin<rawmidi_out>(csound, "rawmidi_out", "", "ikk[]", csnd::thread::ik);
    csnd::plugin<rawmidi_out>(csound, "rawmidi_out", "", "iii[]", csnd::thread::i);

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
    csnd::plugin<rawmidi_noteon_in>(csound, "rawmidi_noteon_in.ik", "kkkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_noteoff_in>(csound, "rawmidi_noteoff_in.ik", "kkkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_cc_in>(csound, "rawmidi_cc_in.ik", "kkkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_pitchbend_in>(csound, "rawmidi_pitchbend_in.ik", "kkkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_polyaftertouch_in>(csound, "rawmidi_polyaftertouch_in.ik", "kkkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_aftertouch_in>(csound, "rawmidi_aftertouch_in.ik", "kkk", "im", csnd::thread::ik);
    csnd::plugin<rawmidi_programchange_in>(csound, "rawmidi_programchange_in.ik", "kkk", "im", csnd::thread::ik);

    /**
    Erae Touch utilities and widgets 
    **/
    // Basinc ERAE Touch sysex API functions
    /*
    csnd::plugin<erae_sysex_open>(csound, "erae_sysex_open", "", "iiii", csnd::thread::i);
    csnd::plugin<erae_sysex_close>(csound, "erae_sysex_close", "", "i", csnd::thread::i);
    csnd::plugin<erae_sysex_clear_zone>(csound, "erae_sysex_clear_zone", "", "ii", csnd::thread::i);
    */

    // WPZ is widget per zone, a set of widgets using one full API zone
    csnd::plugin<erae_wpz_xyz>(csound, "erae_wpz_xyz", "kkkkkk", "iiii[]", csnd::thread::ik);
    /*
    csnd::plugin<erae_wpz_matrix>(csound, "erae_wpz_matrix", "k[]kkkk", "iiiiii[]i[]i", csnd::thread::ik);
    csnd::plugin<erae_wpz_matrix_getvalue>(csound, "erae_wpz_matrix_getvalue", "k", "k[]kkk", csnd::thread::ik);
    csnd::plugin<erae_wpz_matrix_dyn>(csound, "erae_wpz_matrix_dyn", "k[]kkkk", "iiiiii[]i[]ik[]", csnd::thread::ik);
    */

    // Floats decoder for 7bitized float stream
    csnd::plugin<decode_5bytes_float>(csound, "decode_floats", "k[]", "k[]ki", csnd::thread::ik);
    /** SYSEX Utilities **/
    csnd::plugin<sysex_text_to_arr>(csound, "sysex_text_to_bytes", "ii[]", "S", csnd::thread::i);
    //csnd::plugin<sysex_compose>(csound, "sysex_compose", "ii[]", "M", csnd::thread::i);

    // Facility for sysex handling
    csnd::plugin<sysex_print>(csound, "sysex_print", "", "k[]k", csnd::thread::ik );
    csnd::plugin<sysex_print>(csound, "sysex_print", "", "i[]i", csnd::thread::i );
    csnd::plugin<sysex_print_hex>(csound, "sysex_print_hex", "", "k[]k", csnd::thread::ik );
    csnd::plugin<sysex_print_hex>(csound, "sysex_print_hex", "", "i[]i", csnd::thread::i );

    /** Array utilities for sysex **/
    csnd::plugin<array_append>(csound, "array_append", "ii[]", "i[]i", csnd::thread::i);
    csnd::plugin<array_prepend>(csound, "array_prepend", "ii[]", "ii[]", csnd::thread::i);
    csnd::plugin<array_concat>(csound, "array_concat", "ii[]", "i[]i[]", csnd::thread::i);
}

