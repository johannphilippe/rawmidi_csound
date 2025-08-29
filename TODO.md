

# Erae sysex engine 

For Embodme Erae touch sysex API.
Two engines : one widget per zone (wpz), and the other will have an actual refresh rate
Now only the widget per zone is implemented with one widget.

## WPZ - Widget per zone (one per zone)

* Try games
	- Pong 
	- Cellular automata
	- A ball we can launch agains walls (bouncing game) 
		- Bouncing ball with gravity (one wall must be gravity)
		- A 4 wall structure with energy decrease (trajectory system)
	- A water game (object in water that can be manually moved and respond to water inertia)
	- A matrix based effect with modulation : (each modulation is a combination of the matrix inputs) and feedbacks 

# Todo 

* Add "rawmidi_note_in" : for both noteon and noteoff
* Add "note forward" opcode to forward notes from specific channel/handle input to specific channel/handle output
* MPE support :	https://d30pueezughrda.cloudfront.net/campaigns/mpe/mpespec.pdf

MPE Spec : 
	* MPE Configuration Message (MCM)
    	* B0/BF 79 00 - B0/BF 64 06 - B0/BF 65 00 - B0/BF 06 mm
    	* B0/BF for lower or upper zone 
    	* mm is 0 for MPE off or number of channels (from 1 to F)
		* Max 2 zones (the lower has master channel 1, upper has 16)
	* MPE usual mode for polyphonic MIDI instruments is mode 3 : Omni off, poly > maximal polyphony	
