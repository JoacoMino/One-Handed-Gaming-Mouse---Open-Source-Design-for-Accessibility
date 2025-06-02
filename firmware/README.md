# Firmware

This directory contains the Arduino source code (`.ino` file) and required header file (`firmware_data.h`) for the gaming mouse.

## How to use

1. Open the `.ino` file in the Arduino IDE.
2. Make sure the `firmware_data.h` file is in the same folder.
3. Select the correct board and port in the Arduino IDE.
4. Upload the firmware to the SparkFun Pro Micro board.

## Notes

- The firmware implements a one-handed mouse mode (WASD + analog joystick).
- It uses libraries: `SPI.h`, `Keyboard.h`, `Mouse.h`, and `avr/pgmspace.h`.
