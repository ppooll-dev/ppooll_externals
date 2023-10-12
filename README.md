# ppooll_externals

Max externals required for running [ppooll](https://github.com/ppooll-dev/ppooll):

- ll_2dslider
- ll_fastforward
- ll_mcwaveform
- ll_number


## Install and Build

Please change `/path/to/max-sdk-base`.
Note: the following instructions are for building both targets on an OSX machine.

Build for OSX:
1. `mkdir build-mac`
2. `cd build-mac`
3. `cmake .. -DMAX_SDK_BASE_PATH=/path/to/max-sdk-base`
4. `make`

Build for Windows:
1. `mkdir build-win`
2. `cd build-win`
3. `cmake .. -DMAX_SDK_BASE_PATH=/path/to/max-sdk-base -DCMAKE_TOOLCHAIN_FILE=WindowsToolchain.cmake` 
4. `make`