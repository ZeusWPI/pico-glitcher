# Pico-glitcher

Most information is in the blogpost: https://zeus.ugent.be/blog/22-23/reverse_engineering_epaper/

This is a voltage glitching exploit tool that works against the CC2510, and probably also against other CCxxxx Texas Instruments wireless chips that contain an 8051 core and the ChipCon debugging protocol.

It runs on a Raspberry Pi Pico chip. To start working on an exploit for your device, get a development board (or a spare device) and run the following steps on a separate computer, without moving or touching the board the CC2510 chip is on too much:

1. Check if readout protection is enabled using `serial_number.py`
2. If not, you're in luck, use `reader.py` to read out the firmware
3. If it is, wipe the development board and run `explore.py` (without the MOSFET soldered) to make sure the debug sequence works. Then write to it (`writer.py`) and lock it again (`locker.py`).
3. Solder the MOSFET and use `explore.py` with a wide range and low iterations count (100) to find the approximate location of both glitch locations. In `analyze.py` are some estimations that worked on my board.
4. Narrow the glitch locations and duration down with 10000 iterations and a smaller range (~50)
5. Verify you can read out data with `exploit.py` (you'll need to modify the params)
5. Solder the MOSFET on the second board, and redo the parameter tuning, and re-run `exploit.py`. This is best done in a tmux session, since it will take a long time.

I used the IRLML6246TRPBF MOSFET, but other fast N-channel MOSFETs should also work.