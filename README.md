# Infrared Repeater with M5StickC

## Devices
- M5StickC x 2
  - https://docs.m5stack.com/#/en/core/m5stickc
  - https://docs.m5stack.com/#/ja/core/m5stickc
- Grove Infrared Emitter x 1
  - http://wiki.seeedstudio.com/Grove-Infrared_Emitter/
- Grove - Infrared Receiver
  - http://wiki.seeedstudio.com/Grove-Infrared_Receiver/

## Pin Assign
- IR Sender M5StickC
  - Grove Port
    - IO33: Grove Infrared Emitter
    - VOUT(**5V**): Grove Infrared Emitter (Works, but I recommend connecting to 3.3V)
    - GND: Grove Infrared Emitter
    - IO32: Grove Infrared Emitter (unused)

- IR Receiver M5StickC
  - Grove Port
    - IO33: Grove Infrared Receiver
    - VOUT: Grove Infrared Receiver
    - GND: Grove Infrared Receiver
    - IO32: Grove Infrared Receiver (unused)

## System abstract

```
IR remote controller --> [Grove Receiver] --> [recv.ino] -->
>-- (HTTP POST) -->
>-- [send] -- [Grove Emitter] --> Devices you want to control with IR.
```

## License
- Infrared Repeater with M5StickC
  - The 2-Clause BSD License

- 3rd Party Libraries
  - M5StickC
    - GNU Lesser General Public License v2.1
      - https://github.com/espressif/arduino-esp32
    - Unknown
      - https://github.com/m5stack/M5StickC
  - IRremoteESP8266
    - https://github.com/crankyoldgit/IRremoteESP8266
      - GNU Lesser General Public License v2.1
