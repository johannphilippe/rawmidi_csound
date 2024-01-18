# rawmidi_csound

 A flexible MIDI library for Csound using RTMidi.

 Features :
 * Device oriented MIDI communication > ideal for setups with a lot of physical or virtual devices) : you can benefit from 16 IO channels for each device
 * Not limited to MIDI channel messages (Sysex is on the table)
 * Raw access to MIDI streams
 * I and K rate for channel messages

For example : 
```
ihandle = rawmidi_open_in(2, 2) ; Second device 2, with API 2 (Alsa Linux)
kch, knote, kvel rawmidi_noteon_in ihandle
```

# Reference

Supported API's are : 
    0=UNSPECIFIED
    1=MACOSX_CORE
    2=LINUX_ALSA
    3=UNIX_JACK
    4=WINDOWS_MM
	5=RTMIDI_DUMMY
    6=WEB_MIDI_API
    7=NUM_APIS

List input and output devices : 
`Sins[], Souts[] rawmidi_list_devices iAPI_index`

Open a port (in or out) to a physical or virtual device

`ihandle rawmidi_in_open iport_num, iAPI_index`
`ihandle rawmidi_out_open iport_num, iAPI_index`
`ihandle rawmidi_virtual_in_open Sport_name, iAPI_index`
`ihandle rawmidi_virtual_out_open Sport_name, iAPI_index`

The returned handle correspond to a connection to the device. It will be used to send or retrieve MIDI stream.


Receive or send raw MIDI stream : 

`kchanged, ksize, kdata[] rawmidi_in ihandle`
`kchanged, isize, idata[] rawmidi_in ihandle`

`rawmidi_out ihandle, ksize, kdata[]`
`rawmidi_out ihandle, isize, idata[]`

Facilities for channel messages outputs : 

`rawmidi_noteon_out ihandle, ichan, ikey, ivel`
`rawmidi_noteon_out ihandle, kchan, kkey, kvel`

`rawmidi_noteoff_out ihandle, ichan, ikey, ivel`
`rawmidi_noteoff_out ihandle, kchan, kkey, kvel`

`rawmidi_cc_out ihandle, ichan, ictl, ival`
`rawmidi_cc_out ihandle, kchan, kctl, kval`

`rawmidi_pitchbend_out ihandle, ichan, ictl, ival`
`rawmidi_pitchbend_out ihandle, kchan, kctl, kval`

`rawmidi_aftertouch_out ihandle, ichan, ival`
`rawmidi_aftertouch_out ihandle, kchan, kval`

`rawmidi_polyaftertouch_out ihandle, ikey, ival`
`rawmidi_polyaftertouch_out ihandle, kkey, kval`

`rawmidi_programchange_out ihandle, ival`
`rawmidi_programchange_out ihandle, kval`

Facilities for channel messages inputs : 
These opcodes return k-rate only, for design reasons.

`kchan, kkey, kvel rawmidi_noteon_in ihandle`
`kchan, kkey, kvel rawmidi_noteoff_in ihandle`
`kchan, kctl, kval rawmidi_cc_in ihandle`
`kchan, kval1, kval2 rawmidi_pitchbend_in ihandle`
`kchan, kval rawmidi_aftertouch_in ihandle`
`kchan, kkey, kval rawmidi_polyaftertouch_in ihandle`
`kchan, kval  rawmidi_programchange_in ihandle`

Print a sysex message as Integers : 
`sysex_print isize, iArr[]`
`sysex_print ksize, kArr[]`

Print a sysex message in hexadecimal format :
`sysex_print_hex isize, iArr[]`
`sysex_print_hex ksize, kArr[]`

# Build and install

``` bash
git clone https://github.com/johannphilippe/rawmidi_csound.git --recurse-submodules
cd rawmidi_csound
mkdir build && cd build
cmake ..
make
sudo cp librawmidi.so /your/path/to/csound/plugins
```


# Erae sysex engine 

For Embodme Erae touch sysex API.
Two engines : one widget per zone (wpz), and the other will have an actual refresh rate
Now only the widget per zone is implemented with one widget.

## WPZ - Widget per zone (one per zone)

* Try games
	- Pong 
	- Cellular automata
	- A ball we can launch agains walls (bouncing game) 
		- Bouncing ball with gravity (one wall must be gravity)
		- A 4 wall structure with energy decrease (trajectory system)


# Todo 

* Unsuscribe methods (destructors, unload) for inputs 
* Interpolate axis in wpz (z particularly) for better graphical representation
* framerate engine for games and so.