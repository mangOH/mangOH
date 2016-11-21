mangOH Voice Call Tutorial App
==============================

This app demonstrates the ability to place a call from a mangOH board to another phone.

Steps to Run
------------
1. Install a voice-capable SIM card into a mangOH Green board
1. mangOH must be registered on the network with the SIM in the ready state
1. Insert a headset audio jack into CN500 to listen to the audio
1. Make sure voiceCallService is running by executing `app start voiceCallService`
1. Execute `logread -f` in a separate terminal to view logs while placing the voice call
1. Execute `app runProc mangohVoiceCall --exe=mangohVoiceCall -- <Destination phone number>`
1. Answer the phone call at the remote end
1. Terminate the call by hanging up on the remote phone
