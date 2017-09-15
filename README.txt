Release Note :
 2017-09-07 : Released by Seongo Park
  1. added output format control parameter "PCM_OUT_INTERLEAVED".
     The default value is PARAM_OUT_INTERLEAVED.
     ( See the OUT_INTERLEAVED Paramter in the nx_pdm.h file ) 
  2. fixes an issue where channel 0 data overwrites other channel data.
  3. remove doxygen build when build tinyalsa
  4. modify audiostream class api
  5. refactoring smart voice lib struct
        - hf      : rebuilded for hard float
        - sf      : rebuilded for soft float
        - android : rebuilded for android
        - arm11   : rebuilded for arm11
        struct
          ├─ nx_pdm.h
          ├─ resample.h
          ├─ hf      : linux library for arm-a9 hard float
          ├─ sf      : linux library for arm-a9 soft float
          ├─ anrdoid : android library for arm-a9 android toolchain
          ├─ anrdoid : android library for arm-a9 android toolchain
          └─ arm11   : linux library for arm11
  6. add tinyalsa (1.0.2) library

 2017-08-09 : modified by SeongO.Park
  1. Add some command in smart_voice application.
    a. Add agc dB control command. (-a)
    b. Add pdm gain control command (-g)
    c. This path is not a change to the libagcpdm library.

 2017-07-18 : released by SeongO.Park (ver 0.10.1)
  1. Add pdm_GetParam() and pdm_SetParam() API in the "nx_pdm.h" file.
    This function control PDM library options.
    This patch adds parameter PDM_PARAM_GAIN.
      This valule control pdm output gain.
      The smaller the value, the lager output output.
      The lager the vaule, the smaller output output.
  2. Reduce default pdm output gain half for protect clipping phenomenon.

 2017-06-20 : released by SeongO.Park
   1. Add resample source code. ( based on FFMPEG )
   2. Ehancement PDM to PCM quality.
   3. Add libraries for cortex a9 (android/softfloat) and arm11.

