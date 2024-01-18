<CsoundSynthesizer>
<CsOptions>
-odac
;-+rtmidi=alsaseq
;-M hw:1,0
;--opcode-lib="./librawmidi.so"
</CsOptions>
; ==============================================
<CsInstruments>

sr	=	48000
ksmps	=	64
nchnls	=	2
0dbfs	=	1

gimidi_in = rawmidi_open_in(2, 2)
gimidi_out = rawmidi_open_out(2, 2)


; Tests are performed on Embodme Erae Touch  sysex API

opcode erae_open_sysex, 0, iiii
	ihandle, ireceiver, iprefix, ibytes xin
	;		   F0    00    21    50    00    01    00    01    01    01    04    01 RECEIVER PREFIX BYTES F7
	iArr[] fillarray 0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x04, 0x01, ireceiver, iprefix, ibytes, 0xF7
	ilen = lenarray(iArr)
	rawmidi_out(ihandle, ilen, iArr)
endop

opcode erae_close_sysex, 0, i
	ihandle xin
	;		   F0    00    21    50    00    01    00    01    01    01    04    02    F7
	iDis[] fillarray 0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x04, 0x02, 0xF7
	ilen = lenarray(iDis)
	rawmidi_out(ihandle, ilen, iDis)
endop

opcode erae_clear, 0, ii
	ihandle, izone xin
	; 	  	   F0    00    21    50    00    01    00    01    01    01    04  20   ZONE      F7
	iArr[] fillarray 0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x04, 0x20, izone, 0xF7
	ilen = lenarray(iArr)
	rawmidi_out(ihandle, ilen, iArr)
endop

opcode erae_draw_rectangle, 0, iiiiiiiii
	ihandle, izone, ix, iy, iw, ih, ir, ig, ib xin
	;  	      F0    00    21    50    00    01    00    01    01    01    04    22 ZONE XPOS YPOS WIDTH HEIGHT RED GREEN BLUE F7
	iRect[] fillarray 0xF0,  0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x04, 0x22, izone, ix, iy, iw, ih, ir, ig, ib, 0xF7
	ilen = lenarray(iRect)
	sysex_print(iRect, ilen)
	rawmidi_out(ihandle, ilen, iRect)
endop

opcode erae_request_boundary, 0, ii
	ihandle, iZone xin
//F0 00 21 50 00 01 00 01 01 01 04 10 ZONE F7
	iArr[] fillarray 0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x04, 0x10, iZone, 0xF7
	ilen = lenarray(iArr)
	rawmidi_out(ihandle, ilen, iArr)
endop


opcode erae_decode_input, 0, i
	ihandle xin

	; Boundary reply 
	;F0 RECEIVER PREFIX BYTES 7F 01 ZONE Width Height F7
	; Fingerstream 
	;F0 RECEIVER PREFIX BYTES DAT1 DAT2 XYZ1 ... XYZ14 CHK F7

	kchanged, ksize, kData[] rawmidi_in ihandle

	kwidth init 0
	kheight init 0
	kzone init -1
	
	if(kchanged == 1) then 
		; fingerstream message is size 22
		; boundary is 10 
		if( (ksize == 10) && (kData[4] == 0x7F) ) then  ; boundary reply 
			kzone = kData[6]
			kwidth = kData[7]
			kheight = kData[8]
			printf("Zone : %d  ----- Width and Height : %d  &  %d \n", kchanged, kzone, kwidth, kheight)
		elseif(ksize == 22) then 
			kToDecode[] slicearray kData, 6, 19
			kexpsum = kData[20]
			kact = kData[4]
			kzone = kData[5]
			kxyz[] = decode_floats(kToDecode, kexpsum, 14)
			printf("fingerstream for Zone : %d : xyz -- %f %f %f \n", kchanged, kzone, kxyz[0], kxyz[1], kxyz[2])
			;printk2 kxyz[0]
			;printk2 kxyz[1]
			printk2 kxyz[0]
		endif
	endif
endop

instr 1 
	print gimidi_in
	print gimidi_out
	Sins[], Souts[] rawmidi_list_devices 2
	erae_close_sysex(gimidi_out)
	erae_open_sysex(gimidi_out, 0x00, 0x20, 16)
	erae_open_sysex(gimidi_out, 1, 0x20, 16)
	erae_clear(gimidi_out, 0)
	erae_draw_rectangle(gimidi_out, 0, 0x00, 0x00, 0x05, 0x02, 0x7F, 0x00, 0x7F)
	;erae_draw_rectangle(gimidi_out, 0, 0x10, 0x10, 0x10, 0x10, 0x7F, 0x00, 0x7F)
	erae_draw_rectangle(gimidi_out, 1, 0x00, 0x00, 0x03, 0x05, 0x7F, 0x7F, 0x7F)

	erae_decode_input(gimidi_in)

	erae_request_boundary(gimidi_out, 0)
	erae_request_boundary(gimidi_out, 1)
endin



instr 2

	erae_draw_rectangle(gimidi_out, 1, 0x20, 0x20, 0x30, 0x30, 0x7F, 0x7F, 0x7F)

endin




</CsInstruments>
; ==============================================
<CsScore>
f 0 z
i 1 0 -1
i 2 3 -1


</CsScore>
</CsoundSynthesizer>

