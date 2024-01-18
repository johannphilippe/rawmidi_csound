<CsoundSynthesizer>
<CsOptions>

-odac
</CsOptions>
; ==============================================
<CsInstruments>

sr	=	48000
ksmps	=	1
nchnls	=	2
0dbfs	=	1

/*
instr 1	

Sins[], Souts[] rawmidi_list_devices 2

ilen = lenarray(Sins)
print(ilen)

prints("\n\n")
prints("Inputs \n")
icnt init 0
while icnt < ilen do 
	puts Sins[icnt], 1
	icnt += 1	
od

ilen = lenarray(Souts)

prints("\n\n")
prints("Outputs \n")
icnt = 0
while icnt < ilen do 
	puts Souts[icnt], 1
	icnt += 1	
od
prints("\n\n")


ihandle = rawmidi_open_out(3, 2)
print(ihandle)
rawmidi_noteon_out(ihandle, 1, 1, 1)
rawmidi_noteon_out(ihandle, 1, 1, 2)
rawmidi_noteon_out(ihandle, 1, 1, 3)
rawmidi_noteon_out(ihandle, 1, 1, 4)
rawmidi_noteon_out(ihandle, 1, 1, 5)
rawmidi_noteon_out(ihandle, 1, 1, 6)

rawmidi_noteoff_out(ihandle, 2, 1, 1)
rawmidi_noteoff_out(ihandle, 2, 1, 2)
rawmidi_noteoff_out(ihandle, 2 , 1, 3)
rawmidi_noteoff_out(ihandle, 2, 1, 4)
rawmidi_noteoff_out(ihandle, 2, 1, 5)
rawmidi_noteoff_out(ihandle, 2, 1, 6)
endin

instr 2 ; inputs
	ihandle = rawmidi_open_in(2, 2)
	kch, knote, kvel rawmidi_noteon_in ihandle 
	printk2 kch
	printk2 knote, 15
	printk2 kvel, 30
endin

instr 3 ; cc in 
	Sins[], Souts[] rawmidi_list_devices 2
	ihandle = rawmidi_open_in(2, 2)
	kch, kctl, kval rawmidi_cc_in ihandle
	
	printk2 kch
	printk2 kctl, 15
	printk2 kval, 30
endin

instr 4 ; cc out 
	Sins[], Souts[] rawmidi_list_devices 2
	ihandle = rawmidi_open_out(2, 2)

	kctl = int(phasor:k(1) * 125)
	rawmidi_cc_out(ihandle, 2, 37, kctl)
	
endin

instr 5 ; Test to see if several inputs of same handle are "stealing" or not >> CC
	ihandle = rawmidi_open_in(2, 2)
	kch, kctl, kval rawmidi_cc_in ihandle
	kch2, kctl2, kval2 rawmidi_cc_in ihandle
	
	printk2 kch, 0
	printk2 kctl, 15
	printk2 kval, 30

	printk2 kch2, 50
	printk2 kctl, 65
	printk2 kval, 80
endin

// Error, sharing handle causes second in to receive nothing (logic after all)
instr 6 ; Test to see if several inputs of same handle are "stealing" or not >> Notes
	ihandle = rawmidi_open_in(2, 2)
	ihandle2 = rawmidi_open_in(2, 2)
	kch, knote, kvel rawmidi_noteon_in ihandle 
	kch2, knote2, kvel2 rawmidi_noteon_in ihandle2
	
	printk2 kch, 0
	printk2 knote, 15
	printk2 kvel, 30

	printk2 kch2, 50
	printk2 knote2, 65
	printk2 kvel2, 80
endin

// Not a big deal here since CC can duplicate most of the time ? 
instr 7 ; Test to see if several inputs of same handle are "stealing" or not >> Notes
	ihandle = rawmidi_open_in(2, 2)
	ihandle2 = rawmidi_open_in(2, 2)
	kch, knote, kvel rawmidi_noteon_in ihandle 
	kch2, knote2, kvel2 rawmidi_cc_in ihandle2
	
	printk2 kch, 0
	printk2 knote, 15
	printk2 kvel, 30

	printk2 kch2, 70
	printk2 knote2, 86
	printk2 kvel2, 90
endin

*/
gihandle = rawmidi_open_in(2, 2)
instr 8	
	kch, knote, kvel rawmidi_noteon_in gihandle
	printk2 kch
	printk2 knote, 10
	printk2 kvel, 20

	kch2, knote2, kvel2 rawmidi_noteon_in gihandle
	printk2 kch2, 40
	printk2 knote2, 50
	printk2 kvel2, 60
endin



</CsInstruments>
; ==============================================
<CsScore>

f 0 z
;i 1 0 1
i 8 0 -1

</CsScore>
</CsoundSynthesizer>

