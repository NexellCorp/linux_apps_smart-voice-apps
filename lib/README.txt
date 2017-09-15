Version information
 0.11.2
  1. Added output format control parameter "PCM_OUT_INTERLEAVED".
     The default value is PARAM_OUT_INTERLEAVED.
     ( See the OUT_INTERLEAVED Paramter in the nx_pdm.h file ) 
  2. Fixes an issue where channel 0 data overwrites other channel data.

 0.10.1
  1. Add pdm_GetParam() and pdm_SetParam() API in the "nx_pdm.h" file.
    This function control PDM library options.
    This patch adds parameter PDM_PARAM_GAIN.
      This valule control pdm output gain.
      The smaller the value, the lager output output.
      The lager the vaule, the smaller output output.
  2. Reduce default pdm output gain half for protect clipping phenomenon.

 0.9.0
  1. Add void pdm_GetVersion( int *major, int *minor, int *rev );
  2. Add libraries( arm11 / cortex-a9 hf / cortex-a9 sf )
  3. Add android lollipop binary for hardware floating point.