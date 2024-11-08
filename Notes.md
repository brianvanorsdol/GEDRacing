# TO DO:
## General
- Create wiring diagrams to explain usage of Development Board and Doodads

## Questions to Answer
- Record LIN messages from steering wheel button inputs OEM/Aftermarket, Able to create a "Track Mode" Button?
- How do DBC CAN files work? Is there a specific format we need to use? 
  - Utilize Bryan (Early Apex)'s spreadsheet as a base for this

## Hardware (Motorsport Development Board)
  - Add SD card socket
  - Choose STM chip
  - Add status LED
  - Come up with a cool name?

## Doodads
  - Shift Lights
    - Add button to control brightness and change modes
    - Test with new Arduino Nano ESP32 board
    - Design new housing to integrate Nano and button
  - OLED Gauges
  - Integrated Tire Temperature sensors
    - Utilize RejsaRubberTrac as base - https://github.com/MagnusThome/RejsaRubberTrac
      - Maybe create a display to show temps of each tire ala Assetto style?
  - Lap Timer Integration
    - Can we integrate something like Racebox lap timer GPS?


## Source Code
- Figure out how to aggregate DME log files, lap data files, etc into singular file for review
  - .csv? or other file type?