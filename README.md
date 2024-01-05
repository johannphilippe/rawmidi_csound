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

# Documentation

There is no "documentation" but you can find the reference by looking at `rawmidi.cpp` : at the end of the file, inside the `on_load` function. 

# Usage

`ihandle rawmidi_open_in iport_num, [iAPI_index]`
`ihandle rawmidi_open_out iport_num, [iAPI_index]`
`ihandle rawmidi_open_virtual_in Sport_name, [iAPI_index]`
`ihandle rawmidi_open_virtual_out Sport_name, [iAPI_index]`

`ksize, kdata[] rawmidi_in ihandle`
`isize, idata[] rawmidi_in ihandle`

`rawmidi_out ihandle_num, ksize, kdata`
`rawmidi_out ihandle, isize, idata`

	- ksize : size of data array
	- kdata[] : 8 bit integer data array passed as Csound numbers (float). Only the integer part should be used. 
	- iport_num is port number for RtMidi API - it can be listed with "rawmidi_list_devices"
	- iAPI_index is the API index, among : 
		* 0=UNSPECIFIED
		* 1=MACOSX_CORE
		* 2=LINUX_ALSA
		* 3=UNIX_JACK
		* 4=WINDOWS_MM
		* 5=RTMIDI_DUMMY
		* 6=WEB_MIDI_API
	- ihandle : pointer to the MIDI connection


`sysex_print isize, iArr[]`
`sysex_print ksize, kArr[]`

Also implements most channel messages for input and output. 


