# rawmidi_csound

A raw MIDI opcode for Csound using RtMidi
It does not use Csound's internal MIDI implementation. 

# Use it 

`ksize, kdata[] rawmidi_in iport_num, [iAPI_index]`
`isize, idata[] rawmidi_in iport_num, [iAPI_index]`

`rawmidi_out iport_num, ksize, kdata, [iAPI_index]`
`rawmidi_out iport_num, isize, idata, [iAPI_index]`

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


`iArr[] sysex_arr iarg1, arg2, arg3, [iarg4 ... iarg128]`
`kArr[] sysex_arr iarg1, arg2, arg3, [iarg4 ... iarg128]`
Generates a sysex array from a list of integer values between 0 and 127. The opcode will create the array adding the sysex header and footer, and making sure no value is above 127 in the list of arguments. 


