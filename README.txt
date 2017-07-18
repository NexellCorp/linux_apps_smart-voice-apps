Release Note :
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
