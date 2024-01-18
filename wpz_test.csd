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

gSins[], gSouts[] rawmidi_list_devices 2
gimidi_in = rawmidi_open_in(2, 2)
gimidi_out = rawmidi_open_out(2, 2)


opcode erae_open_sysex, 0, iiii
        ihandle, ireceiver, iprefix, ibytes xin
        ;                  F0    00    21    50    00    01    00    01    01    01    04    01 RECEIVER PREFIX BYTES F7
        iArr[] fillarray 0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x04, 0x01, ireceiver, iprefix, ibytes, 0xF7
        ilen = lenarray(iArr)
        rawmidi_out(ihandle, ilen, iArr)
endop

opcode erae_close_sysex, 0, i
        ihandle xin
        ;                  F0    00    21    50    00    01    00    01    01    01    04    02    F7
        iDis[] fillarray 0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x04, 0x02, 0xF7
        ilen = lenarray(iDis)
        rawmidi_out(ihandle, ilen, iDis)
endop

opcode erae_clear, 0, ii
        ihandle, izone xin
        ;                  F0    00    21    50    00    01    00    01    01    01    04  20   ZONE      F7
        iArr[] fillarray 0xF0, 0x00, 0x21, 0x50, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x04, 0x20, izone, 0xF7
        ilen = lenarray(iArr)
        rawmidi_out(ihandle, ilen, iArr)
endop


instr 1 
	erae_close_sysex(gimidi_out)
	erae_open_sysex(gimidi_out, 1, 2, 3)
	iColor1[] fillarray 127, 0, 0
	iColor2[] fillarray 0, 0, 127
	iColor3[] fillarray 0, 127, 0
	izone init 10
	erae_clear(gimidi_out, izone)
	kx, ky, kz, kact, kfinger erae_wpz_xyz gimidi_in, gimidi_out, izone, iColor1
	kx2, ky2, kz2, kact2, kfinger2 erae_wpz_xyz gimidi_in, gimidi_out, izone+1, iColor2
	kx3, ky3, kz3, kact3, kfinger3 erae_wpz_xyz gimidi_in, gimidi_out, izone+2, iColor3


	printk2 kact
endin



</CsInstruments>
; ==============================================
<CsScore>
f 0 z
i 1 0 -1


</CsScore>
</CsoundSynthesizer>

