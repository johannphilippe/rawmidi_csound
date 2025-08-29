<CsoundSynthesizer>
<CsOptions>
</CsOptions>
; ==============================================
<CsInstruments>

sr	=	48000
ksmps	=	1
nchnls	=	2
0dbfs	=	1


Sins[], Souts[] rawmidi_list_devices 2

giIn = rawmidi_open_in(1, 2)
giOut = rawmidi_open_out(1, 2)
instr 1	


endin

</CsInstruments>
; ==============================================
<CsScore>



</CsScore>
</CsoundSynthesizer>

