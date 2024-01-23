<CsoundSynthesizer>
<CsOptions>
-odac
-+rtaudio=jack
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


opcode matrixate, a[], iik[]k[]a[]
	irow, icol, kmatrix[], kctl[], ains[] xin

	asigs[] = ains
	ares[] init icol
	clear ares
	kmat_size init  irow * icol

	kcnt = 0
	kcol init 0
	krow init 0
	while kcnt < kmat_size do 
		krow = floor(kcnt / 8)
		kcol = (kcnt) % 8
		kmult = kmatrix[kcnt]
		ares[kcol] = ares[kcol] +( (ains[kcol] * (kctl[krow]*kmult)) )
		kcnt += 1
	od	
	
	xout ares
endop


instr 1
	erae_sysex_close(gimidi_out)
	erae_sysex_open(gimidi_out, 1, 2, 3)
	izone init 10
	erae_clear(gimidi_out, izone)

	ibaseCol[] fillarray 0, 0, 127
	iselCol[] fillarray 127, 127, 0


	kCtrl[] init 8
	kCtrl[0] = rspline:k(0, 1, 4, 8)
	kCtrl[1] = phasor:k(9)
	kCtrl[2] = abs(oscili:k(1, 5))
	kCtrl[3] = random:k(0, 1)
	kCtrl[4] = 1 - phasor:k(4)
	kCtrl[5] = rspline:k(0, 1, 5, 10)
	kCtrl[6] = phasor:k(20)
	kCtrl[7] = randomi:k(0, 1, 10)


	kx[], kchanged, krow, kcol, kindex  erae_wpz_matrix_dyn gimidi_in, gimidi_out, izone, 8, 8, ibaseCol, iselCol, 2, kCtrl




	asigs[] init 8

	asigs[0] = oscili:a(0.1, 50)
	asigs[1] = oscili:a(0.1, 110)
	asigs[2] = vco2(0.1, 150)
	asigs[3] = vco2(0.1, 205)
	asigs[4] = oscili:a(0.1, 300)
	asigs[5] = oscili:a(0.1, 310)
	asigs[6] = vco2(0.05, 490)
	asigs[7] = oscili(0.05, 510)

	ares[] matrixate 8, 8, kx, kCtrl, asigs

	aout = sumarray(ares) * 0.3
	outs aout, aout
endin


</CsInstruments>
; ==============================================
<CsScore>
f 0 z
i 1 0 -1

</CsScore>
</CsoundSynthesizer>

