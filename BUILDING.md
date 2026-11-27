# Building NexPlay for Aurora OS

1. Clone the Aurora OS repository and install devkitPro/devkitARM.
2. Ensure `arm-none-eabi-gcc` is on PATH.
3. Build the application:

```sh
python3 sdk/aurcc.py build NexPlay -o NexPlay.bin
```

4. Copy the resulting `NexPlay.bin` to `SD:/Aurora/Apps/C/`.
5. On the SD card, create `/roms/gb/` and copy your Game Boy ROM files there.

Aurora OS C apps require Beta 6 v0.1.5 or later for the `AURC` app container.
