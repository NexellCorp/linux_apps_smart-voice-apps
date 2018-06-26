#include "resample.h"

struct AVResampleContext;

struct ReSampleContext {
    struct AVResampleContext *resample_context;
    short *temp[2];
    int temp_len;
    float ratio;
    /* channel convert */
    int input_channels, output_channels, filter_channels;

	/* input format */
	int input_fmt;
	void (*stereo_to_mono)(short*, void*, int);
	void (*stereo_split)(short*, short*, void*, int);
};

/* n1: number of samples */
static void stereo_to_mono_16(short *output, void *input, int n1)
{
    short *p, *q;
    int n = n1;

    p = (short*)input;
    q = output;
    while (n >= 4) {
        q[0] = (p[0] + p[1]) >> 1;
        q[1] = (p[2] + p[3]) >> 1;
        q[2] = (p[4] + p[5]) >> 1;
        q[3] = (p[6] + p[7]) >> 1;
        q += 4;
        p += 8;
        n -= 4;
    }
    while (n > 0) {
        q[0] = (p[0] + p[1]) >> 1;
        q++;
        p += 2;
        n--;
    }
}

#if 0
static void stereo_to_mono_24(short *output, void *input, int n1)
{
	short *q;
	int n = n1;
	char *p= (char*)input;
	q = output;

	while (n > 0)
	{
		q[0] = ((short)(p[0] << 8 | p[1]) + (short)(p[3] << 8 | p[4]) )>> 1;
		q++;
		p += 6;
		n--;
	}
}

static void stereo_to_mono_32(short *output, void *input, int n1)
{
	short *q;
	int *p;
	int n = n1;

	p = (int *)input;
	q = output;

	while (n >= 4)
	{
		q[0] = (p[0] + p[1]) >> 17;
		q[1] = (p[2] + p[3]) >> 17;
		q[2] = (p[4] + p[5]) >> 17;
		q[3] = (p[6] + p[7]) >> 17;

		q += 4;
		p += 8;
		n -= 4;
	}
	while (n > 0)
	{
		q[0] = (p[0] + p[1]) >> 17;
		q++;
		p += 2;
		n--;
	}
}
#else		//	Nexell S5pxx18 Foramt : 221100xx
static void stereo_to_mono_24(short *output, void *input, int n1)
{
	short *q;
	int n = n1;
	unsigned char *p = (unsigned char*)input;
	q = output;

	while (n > 0)
	{
		*q++ = ((short)(p[0] << 8 | p[1]) + (short)(p[3] << 8 | p[4])) >> 1;
		p += 6;
		n--;
	}
}

static void stereo_to_mono_32(short *output, void *input, int n1)
{
	short *q;
	unsigned char *p;
	int n = n1;

	p = (unsigned char *)input;
	q = output;

	while (n >= 4)
	{
		q[0] = ((short)(p[ 2]<<8 | p[ 1]) + (short)(p[ 6]<<8 | p[ 5]))>>1;
		q[1] = ((short)(p[10]<<8 | p[ 9]) + (short)(p[14]<<8 | p[13]))>>1;
		q[2] = ((short)(p[18]<<8 | p[17]) + (short)(p[22]<<8 | p[21]))>>1;
		q[3] = ((short)(p[26]<<8 | p[25]) + (short)(p[30]<<8 | p[29]))>>1;

		q += 4;
		p += 32;
		n -= 4;
	}
	while (n > 0)
	{
		*q++ = ((short)(p[ 2]<<8 | p[ 1]) + (short)(p[ 6]<<8 | p[ 5]))>>1;
		p += 8;
		n--;
	}
}
#endif

/* n1: number of samples */
static void mono_to_stereo(short *output, short *input, int n1)
{
    short *p, *q;
    int n = n1;
    int v;

    p = input;
    q = output;
    while (n >= 4) {
        v = p[0]; q[0] = v; q[1] = v;
        v = p[1]; q[2] = v; q[3] = v;
        v = p[2]; q[4] = v; q[5] = v;
        v = p[3]; q[6] = v; q[7] = v;
        q += 8;
        p += 4;
        n -= 4;
    }
    while (n > 0) {
        v = p[0]; q[0] = v; q[1] = v;
        q += 2;
        p += 1;
        n--;
    }
}

/* XXX: should use more abstract 'N' channels system */
static void stereo_split_16(short *output1, short *output2, void *input, int n)
{
	int i;
	short *in = (short*)input;

	for (i = 0; i<n; i++) {
		*output1++ = *in++;
		*output2++ = *in++;
	}
}

#if 0
static void stereo_split_24(short *output1, short *output2, void *input, int n)
{
	int i;
	char *p = (char*)input;

	for (i = 0; i<n; i++) {
		*output1++ = (short)(p[2] << 8 | p[1]);
		*output2++ = (short)(p[5] << 8 | p[4]);
		p += 6;
	}
}

static void stereo_split_32(short *output1, short *output2, void *input, int n)
{
	int i;
	int *in = (int*)input;

	for (i = 0; i<n; i++) {
		*output1++ = (short)((*in++)>>16);
		*output2++ = (short)((*in++)>>16);
	}
}
#else	//	Nexell S5pxx18 Foramt : 221100xx
static void stereo_split_24(short *output1, short *output2, void *input, int n)
{
	int i;
	char *p = (char*)input;

	for (i = 0; i<n; i++) {
		*output1++ = (short)(p[0] << 8 | p[1]);
		*output2++ = (short)(p[3] << 8 | p[4]);
		p += 6;
	}
}

static void stereo_split_32(short *output1, short *output2, void *input, int n)
{
	int i;
	unsigned char *p = (unsigned char*)input;

	for (i = 0; i<n; i++) {
		*output1++ = (short)(p[2] << 8 | p[1] );
		*output2++ = (short)(p[6] << 8 | p[5] );
		p += 8;
	}
}
#endif

static void stereo_mux(short *output, short *input1, short *input2, int n)
{
    int i;

    for(i=0;i<n;i++) {
        *output++ = *input1++;
        *output++ = *input2++;
    }
}

static void ac3_5p1_mux(short *output, short *input1, short *input2, int n)
{
    int i;
    short l,r;

    for(i=0;i<n;i++) {
      l=*input1++;
      r=*input2++;
      *output++ = l;           /* left */
      *output++ = (l/2)+(r/2); /* center */
      *output++ = r;           /* right */
      *output++ = 0;           /* left surround */
      *output++ = 0;           /* right surroud */
      *output++ = 0;           /* low freq */
    }
}

ReSampleContext *audio_resample_init(int output_channels, int input_channels,
                                      float output_rate, float input_rate, int input_fmt)
{
	ReSampleContext *s;

	if ( input_channels > 2)
	{
		av_log(NULL, AV_LOG_ERROR, "Resampling with input channels greater than 2 unsupported.");
		return NULL;
	}

	s = av_mallocz(sizeof(ReSampleContext));
	if (!s)
	{
		av_log(NULL, AV_LOG_ERROR, "Can't allocate memory for resample context.");
		return NULL;
	}

	s->ratio = (float)output_rate / (float)input_rate;

	s->input_channels = input_channels;
	s->output_channels = output_channels;

	s->filter_channels = s->input_channels;
	if (s->output_channels < s->filter_channels)
		s->filter_channels = s->output_channels;

 /* ac3 output is the only case where filter_channels could be greater than 2. */
 /* input channels can't be greater than 2, so resample the 2 channels and then */
 /* expand to 6 channels after the resampling. */

    if(s->filter_channels>2)
		s->filter_channels = 2;

	if(input_fmt == PCM_FMT_16BIT)
	{
		s->input_fmt = PCM_FMT_16BIT;
		s->stereo_to_mono = &stereo_to_mono_16;
		s->stereo_split = &stereo_split_16;
	}
	else if (input_fmt == PCM_FMT_24BIT)
	{
		s->input_fmt = PCM_FMT_24BIT;
		s->stereo_to_mono = &stereo_to_mono_24;
		s->stereo_split = &stereo_split_24;
	}
	else
	{
		s->input_fmt = PCM_FMT_32BIT;
		s->stereo_to_mono = &stereo_to_mono_32;
		s->stereo_split = &stereo_split_32;
	}

#define TAPS 16
    s->resample_context= av_resample_init(output_rate, input_rate, TAPS, 10, 0, 0.8);

    return s;
}

/* resample audio. 'nb_samples' is the number of input samples */
/* XXX: optimize it ! */
int audio_resample(ReSampleContext *s, short *output, void *input, int nb_samples)
{
    int i, nb_samples1;
    short *bufin[2];
    short *bufout[2];
    short *buftmp2[2], *buftmp3[2];
    int lenout;

    if (s->input_channels == s->output_channels && s->ratio == 1.0 && 0) {
        /* nothing to do */
        memcpy(output, input, nb_samples * s->input_channels * sizeof(short));
        return nb_samples;
    }

    /* XXX: move those malloc to resample init code */
    for(i=0; i<s->filter_channels; i++){
        bufin[i]= (short*) av_malloc( (nb_samples + s->temp_len) * sizeof(short) );
        memcpy(bufin[i], s->temp[i], s->temp_len * sizeof(short));
        buftmp2[i] = bufin[i] + s->temp_len;
    }

    /* make some zoom to avoid round pb */
    lenout= (int)(nb_samples * s->ratio) + 16;
    bufout[0]= (short*) av_malloc( lenout * sizeof(short) );
    bufout[1]= (short*) av_malloc( lenout * sizeof(short) );

    if (s->input_channels == 2 && s->output_channels == 1)
	{
        buftmp3[0] = output;
		s->stereo_to_mono(buftmp2[0], input, nb_samples);
	} else if (s->output_channels >= 2 && s->input_channels == 1) {
        buftmp3[0] = bufout[0];
        memcpy(buftmp2[0], input, nb_samples*sizeof(short));
    } else if (s->output_channels >= 2) {
        buftmp3[0] = bufout[0];
        buftmp3[1] = bufout[1];
        s->stereo_split(buftmp2[0], buftmp2[1], input, nb_samples);
    } else {
        buftmp3[0] = output;
        memcpy(buftmp2[0], input, nb_samples*sizeof(short));
    }

    nb_samples += s->temp_len;

    /* resample each channel */
    nb_samples1 = 0; /* avoid warning */
    for(i=0;i<s->filter_channels;i++) {
        int consumed;
        int is_last= i+1 == s->filter_channels;

        nb_samples1 = av_resample(s->resample_context, buftmp3[i], bufin[i], &consumed, nb_samples, lenout, is_last);
        s->temp_len= nb_samples - consumed;
        s->temp[i]= av_realloc(s->temp[i], s->temp_len*sizeof(short));
        memcpy(s->temp[i], bufin[i] + consumed, s->temp_len*sizeof(short));
    }

    if (s->output_channels == 2 && s->input_channels == 1) {
        mono_to_stereo(output, buftmp3[0], nb_samples1);
    } else if (s->output_channels == 2) {
        stereo_mux(output, buftmp3[0], buftmp3[1], nb_samples1);
    } else if (s->output_channels == 6) {
        ac3_5p1_mux(output, buftmp3[0], buftmp3[1], nb_samples1);
    }

    for(i=0; i<s->filter_channels; i++)
        av_free(bufin[i]);

    av_free(bufout[0]);
    av_free(bufout[1]);
    return nb_samples1;
}

void audio_resample_close(ReSampleContext *s)
{
    av_resample_close(s->resample_context);
    av_freep(&s->temp[0]);
    av_freep(&s->temp[1]);
    av_free(s);
}
