<CsoundSynthesizer>
<CsOptions>
-odac
;-+rtmidi=alsaseq
;-M hw:1,0
--opcode-lib="./librawmidi.so"
</CsOptions>
; ==============================================
<CsInstruments>

sr	=	48000
ksmps	=	64
nchnls	=	2
0dbfs	=	1

gimidi_in = rawmidi_in_open(1, 2)
gimidi_out = rawmidi_out_open(1, 2)


; Tests are performed on Embodme Erae Touch  sysex API


opcode erae_open_sysex, 0, i
	ihandle xin
	;		   F0    00    21    50    00    01    00    01    01    01    04    01 RECEIVER PREFIX BYTES F7
	iArr[] fillarray 0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x04, 0x01, 0x00, 0x00, 0x01, 0xF7
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

instr 1 
	rawmidi_list_devices(2)
	erae_close_sysex(gimidi_out)
	erae_open_sysex(gimidi_out)
	erae_clear(gimidi_out, 0)
	erae_draw_rectangle(gimidi_out, 0x00, 0x00, 0x00, 0x10, 0x10, 0x7F, 0x00, 0x7F)
	ksize, kArr[] rawmidi_in gimidi_in
	sysex_print kArr, ksize
	;printk2 ksize
endin







</CsInstruments>
; ==============================================
<CsScore>
f 0 z
i 1 0 -1


</CsScore>
</CsoundSynthesizer>

