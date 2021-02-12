# rp2040-logic-analyzer

This project modified the PIO logic analyzer example that that was part of the 
Raspberry Pi Pico examples. The example now allows interactive configuration 
of the capture to perform and outputs the capture in CSV format suitable for
importing into sigrock / Pulseview for further analysis.

To use the analyzer install it on a Pico and connect to the COM port at 921600 
baud. Once connected press h to get help of the commands. The capture is
only limited by the abilities of the Pico.

The commands are:
  * p# - Set the first pin to receive capture data
  * n# - Set how many pins to receive capture data
  * f# - Set the freqency to capture data at in Hz
  * t(1)(0) - Set the trigger to high or low. Trigger happens off first pin
  * s# - Set how many samples to capture
  * g - Go!

Once "go" is selected the trigger will arm and wait for the specified signal.
The output is a CSV file, each line contains every pin being sampled. The output
can be saved with any program that can read a serial port to a file. Just be
aware a large number of samples can take quite a while to transfer. The
onboard LED will blink as the transfer is happening so you can know when to end
the save.
