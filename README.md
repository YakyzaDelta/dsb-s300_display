# dsb-s300_display
We connect directly to the board from the old DSB-S300. Among the features, the board contains a display (Tof-2411BG-B3-TS), 8 buttons, an IR sensor, and an LED. All this is attached to the main board with a ribbon cable. The pinout of the cable is: 
1) GND
2) srclk - SH_CP
3) DATA
4) NC
5) rclk - ST_CP
6) NC
7)
IR 8) +3.3v.

Also on the back of the board are two 74HC595D.

Program for ESP32, for Arduino version 2.1.1
