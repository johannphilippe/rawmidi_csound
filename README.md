# rawmidi_csound

A raw MIDI opcode for Csound using RtMidi
It does not use Csound's internal MIDI implementation. 

# Use it 


`ihandle rawmidi_in_open iport_num, [iAPI_index]`
`ihandle rawmidi_out_open iport_num, [iAPI_index]`

`ksize, kdata[] rawmidi_in ihandle`
`isize, idata[] rawmidi_in ihdnale`

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


