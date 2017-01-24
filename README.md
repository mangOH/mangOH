# MangOH

Base project containing apps playing with MangOH hardware

## Setup

### Linux

Open a terminal and load Legato development environment
* `source ~/legato/packages/legato.sdk.latest/resources/configlegatoenv`

### Windows

Launch the **Legato Command Line** shortcut to load the Legato development environment

### Get the source code

Clone this repository and the submodules it requires
* `git clone --recursive git://github.com/mangOH/mangOH`

### Build

Build the mangOH Legato system
* `make --include-dir=$LEGATO_ROOT wp85`
