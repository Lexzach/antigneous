[![forthebadge](https://forthebadge.com/images/badges/made-with-c-plus-plus.svg)](https://forthebadge.com) [![forthebadge](https://forthebadge.com/images/badges/open-source.svg)](https://forthebadge.com) [![forthebadge](https://forthebadge.com/images/badges/built-with-love.svg)](https://forthebadge.com)
![Icon](https://github.com/Lexzach/antigneous/blob/official-firmware/gh_icon.png?raw=true)
# What is this? 🔥
Antigneous is an open-source conventional fire alarm control panel, 3 years in the making. A fire alarm control panel is the device responsible for controlling all of the devices in a fire alarm system, such as smoke detectors, pull stations, and notification devices. Normally, a fire alarm control panel will run you back hundreds, if not thousands of dollars. This project strives to fix that issue by allowing for anyone to build their own reliable fire alarm control panel without breaking the bank.

# What do you mean by "open-source" 🔓
**You do not have to give a single cent to me in order to build this panel.**

The code that is being ran in an Antigneous fire alarm control panel is completely visible to everyone and anyone, [right here](https://github.com/Lexzach/antigneous/blob/official-firmware/facp/src/main.cpp)! Don't like something about Antigneous, you can change it (or even better submit an issue or pull request). All hardware that is used in the making of an Antigneous panel is available to anyone. The wiring diagram for making your own is also available to anyone.

# Instruction Manuals 📄
📕 [Normal Instruction Manual](https://github.com/Lexzach/antigneous/blob/official-firmware/instructions/antigneous_instructions.pdf) - [Download](https://github.com/Lexzach/antigneous/raw/official-firmware/instructions/antigneous_instructions.pdf)

📚 [Technical Instruction Manual](https://github.com/Lexzach/antigneous/blob/official-firmware/instructions/antigneous_tech_instructions.pdf) - [Download](https://github.com/Lexzach/antigneous/raw/official-firmware/instructions/antigneous_tech_instructions.pdf)

# Features and Limitations ⚖️
Although I have tried to make Antigneous as feature-rich as possible, there are some limitations to take into consideration...

## Features
- LCD display
- Very customizable, tinker friendly design
- Many coding selections: Temporal three, Code 4-4, Marchtime, Continuous, California Code, and Slower Marchtime
- Togglable keyless silence
- Walk test, silent walk test, and strobe test
- EOL resistor support for activating loop
- Pull station and smoke detector verification
- Fail-safe mode in case of unbootable panel
- Selectable security levels, electronic keyswitch or no keyswitch

## Current Limitations
- No EOL support for NAC circuit
- No strobe sync, SmartSync, or two wire sync (CURRENTLY)
- Only 2 zones

