<CsoundSynthesizer>                                                             
<CsOptions>
</CsOptions>
; ==============================================
<CsInstruments>
 
sr      =       48000
ksmps   =       1
nchnls =       2
0dbfs   =       1
 

opcode readFile, S, S
    Spath xin

    Sstr = ""
    read:
        Sline, iLineNum readfi Spath
        Sstr strcat Sstr, Sline
        if(iLineNum != -1) igoto read   

    xout Sstr
endop

instr 1 
    Sscale = readFile("scale.scl")
    ilen, iScaleArr[] sysex_text_to_bytes Sscale
    ilen, iScaleWithIndex[] array_prepend 1, iScaleArr
    ilen, iFooter[] array_append iScaleWithIndex, 0xF7
    iHeader[] fillarray 0xF0, 0x00, 0x21, 0x27, 0x2F, 0x11
    ilen, iFullArray[] array_concat iHeader, iFooter
    sysex_print(iFullArray, 1)
endin
 
</CsInstruments>
; ==============================================
<CsScore>

i 1 0 1
 
 
 
</CsScore>
</CsoundSynthesizer>
