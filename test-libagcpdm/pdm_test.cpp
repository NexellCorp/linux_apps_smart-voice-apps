#include <stdint.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../nx_pdm.h"
#include "./wav.h"

struct sWAVE {
	unsigned int 	ChunkID;
	unsigned int 	ChunkSize;
	unsigned int 	Format;
	unsigned int 	Subchunk1ID;
	unsigned int 	Subchunk1Size;
	unsigned short 	AudioFormat;
	unsigned short 	NumChannels;
	unsigned int 	SampleRate;
	unsigned int 	ByteRate;
	unsigned short 	BlockAlign;
	unsigned short 	BitsPerSample;
	unsigned int 	Subchunk2ID;
	unsigned int 	Subchunk2Size;
} Wave;

pdm_STATDEF	pdm_st;

#define	FRAME_LENGTH		(256)
#define PDM_LENGTH			(8192)
#define OUT_FREQ			(16000)

static void set_cpu_freq( uint32_t freq )
{
	char strMaxFreq[1024];
	char strMinFreq[1024];
	sprintf(strMinFreq, "echo %d > /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", freq);
	sprintf(strMaxFreq, "echo %d > /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", freq);

	//	Sometimes error occured. i don't know why.
	system( strMinFreq );
	system( strMaxFreq );
	system( strMinFreq );
	system( strMaxFreq );
	system( strMinFreq );
	system( strMaxFreq );
}

int pdm_main(const char *input, const char *output)
{
	FILE *fp;
	int32_t fileSize;
	int32_t *pPDM;
	short *pResult;
	int32_t numFrames;
	struct timeval tv;
	int64_t start, end;	//	usec
	char outFile[1024];
	int64_t totalTime;



	//	open input file
	fp=fopen(input,"rb");
	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	pPDM    = (int32_t*)malloc(fileSize);
	//	Read All Stream
	fread( pPDM, 1, fileSize, fp);
	fclose(fp);



	numFrames = fileSize / PDM_LENGTH;
	totalTime = numFrames * FRAME_LENGTH * 1000 / OUT_FREQ;		//	msec

	printf("\n");
	printf("=========================================\n");
	printf(" InputFile : %s\n", input);
	printf(" OutputPath : %s\n", output);
	printf(" Total Frames = %d\n", numFrames);
	printf(" Total Frames Time = %lld msec\n", totalTime);
	printf("=========================================\n");

	pResult = (short*)malloc(numFrames * FRAME_LENGTH * 4 * sizeof(short));

	//-------------------------------------------------------------
	// AGC
	pdm_Init (&pdm_st);

	//	Normal Mode
	{
		//	open output wave file
		CWAVFile *hWave = new CWAVFile();
		sprintf( outFile, "%s.normal.wav", output );
		hWave->Open( 4, 16000, 16, outFile );
		gettimeofday(&tv, NULL);
		start = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
		for (int i=0; i<numFrames; i++)	
		{
			//	AGC Off
			pdm_Run_channel (&pdm_st, pResult+(i*FRAME_LENGTH*4), pPDM+i*FRAME_LENGTH*8, 0, 4, 0, 0);
		}
		gettimeofday(&tv, NULL);
		end = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
		printf("Normal Mode : (FrameTime/RunningTime) = %lld / %lld (%lld%%)\n", totalTime, end - start, (end-start)*100/totalTime );
		hWave->Write(pResult, numFrames * FRAME_LENGTH * 4 * sizeof(short));
		hWave->Close();
		delete hWave;
	}

	{
		//	open output wave file
		CWAVFile *hWave = new CWAVFile();
		sprintf( outFile, "%s.fast.wav", output );
		hWave->Open( 4, 16000, 16, outFile );
		gettimeofday(&tv, NULL);
		start = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
		for (int i=0; i<numFrames; i++)	
		{
			//	AGC Off & Fast Mode
			pdm_Run_channel (&pdm_st, pResult+(i*FRAME_LENGTH*4), pPDM+i*FRAME_LENGTH*8, 0, 4, 0, 1);
		}
		gettimeofday(&tv, NULL);
		end = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
		printf("Fast Mode : (FrameTime/RunningTime) = %lld / %lld (%lld%%)\n", totalTime, end - start, (end-start)*100/totalTime );
		hWave->Write(pResult, numFrames * FRAME_LENGTH * 4 * sizeof(short));
		hWave->Close();
		delete hWave;
	}

	printf("done\n");

	free(pResult);
	free(pPDM);

	return 1;
}



int single_main(const char *input, const char *output , int32_t channels)
{
	FILE *fp;
	int32_t fileSize;
	int32_t *pPDM;
	short *pResult;
	int32_t numFrames;
	struct timeval tv;
	int64_t start, end;	//	usec
	char outFile[1024];
	int64_t totalTime;

	//	open input file
	fp=fopen(input,"rb");
	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	pPDM    = (int32_t*)malloc(fileSize);
	//	Read All Stream
	fread( pPDM, 1, fileSize, fp);
	fclose(fp);

	numFrames = fileSize / (PDM_LENGTH / 4 * channels);
	totalTime = numFrames * FRAME_LENGTH * 1000 / OUT_FREQ;		//	msec

	printf("\n");
	printf("=========================================\n");
	printf(" InputFile : %s\n", input);
	printf(" OutputPath : %s\n", output);
	printf(" Channels : %d\n", channels);
	printf(" Total Frames = %d\n", numFrames);
	printf(" Total Frames Time = %lld msec\n", totalTime);
	printf("=========================================\n");

	pResult = (short*)malloc(numFrames * FRAME_LENGTH * sizeof(short) * channels);

	//-------------------------------------------------------------
	// AGC
	pdm_Init (&pdm_st);

	if( channels == 1 )
	{
		//	Normal Mode
		{
			//	open output wave file
			CWAVFile *hWave = new CWAVFile();
			sprintf( outFile, "%s.normal.wav", output );
			hWave->Open( 1, 16000, 16, outFile );
			gettimeofday(&tv, NULL);
			start = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
			for (int i=0; i<numFrames; i++)	
			{
				//	AGC Off
				pdm_Run_filter(&pdm_st, pResult+i*FRAME_LENGTH, pPDM+i*FRAME_LENGTH*2, 0, 0, 0);
			}
			gettimeofday(&tv, NULL);
			end = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
			printf("Normal Mode : (FrameTime/RunningTime) = %lld / %lld (%lld%%)\n", totalTime, end - start, (end-start)*100/totalTime );
			hWave->Write(pResult, numFrames * FRAME_LENGTH * channels * sizeof(short));
			hWave->Close();
			delete hWave;
		}

		{
			//	open output wave file
			CWAVFile *hWave = new CWAVFile();
			sprintf( outFile, "%s.fast.wav", output );
			hWave->Open( 1, 16000, 16, outFile );
			gettimeofday(&tv, NULL);
			start = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
			for (int i=0; i<numFrames; i++)	
			{
				//	AGC Off & Fast Mode
				pdm_Run_filter(&pdm_st, pResult+i*FRAME_LENGTH, pPDM+i*FRAME_LENGTH*2, 0, 0, 1);
			}
			gettimeofday(&tv, NULL);
			end = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
			printf("Fast Mode : (FrameTime/RunningTime) = %lld / %lld (%lld%%)\n", totalTime, end - start, (end-start)*100/totalTime );
			hWave->Write(pResult, numFrames * FRAME_LENGTH * channels * sizeof(short));
			hWave->Close();
			delete hWave;
		}
	}
	else
	{
		#define	PDM_PERIOD_BYTES_OUT	(2048)
		//	Normal Mode
		{
			//	open output wave file
			CWAVFile *hWave = new CWAVFile();
			sprintf( outFile, "%s.normal.wav", output );
			hWave->Open( 4, 16000, 16, outFile );
			gettimeofday(&tv, NULL);
			start = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
			uint8_t *pTmpPdm = (uint8_t*)pPDM;
			for (int i=0; i<numFrames; i++)	
			{
				uint8_t *PPtr[4] = {
					pTmpPdm,
					pTmpPdm + PDM_PERIOD_BYTES_OUT,
					pTmpPdm + PDM_PERIOD_BYTES_OUT * 2,
					pTmpPdm + PDM_PERIOD_BYTES_OUT * 3 };
				//	AGC Off
				pdm_Run_filters(&pdm_st, pResult+i*FRAME_LENGTH*channels, (int32_t**)PPtr, 4, 0, 0, 0);
				pTmpPdm += 8192;
			}
			gettimeofday(&tv, NULL);
			end = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
			printf("Normal Mode : (FrameTime/RunningTime) = %lld / %lld (%lld%%)\n", totalTime, end - start, (end-start)*100/totalTime );
			hWave->Write(pResult, numFrames * FRAME_LENGTH * channels * sizeof(short));
			hWave->Close();
			delete hWave;
		}

		{
			//	open output wave file
			CWAVFile *hWave = new CWAVFile();
			sprintf( outFile, "%s.normal.wav", output );
			hWave->Open( 4, 16000, 16, outFile );
			gettimeofday(&tv, NULL);
			start = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
			uint8_t *pTmpPdm = (uint8_t*)pPDM;
			for (int i=0; i<numFrames; i++)	
			{
				uint8_t *PPtr[4] = {
					pTmpPdm,
					pTmpPdm + PDM_PERIOD_BYTES_OUT,
					pTmpPdm + PDM_PERIOD_BYTES_OUT * 2,
					pTmpPdm + PDM_PERIOD_BYTES_OUT * 3 };
				//	AGC Off
				pdm_Run_filters(&pdm_st, pResult+i*FRAME_LENGTH*channels, (int32_t**)PPtr, 4, 0, 0, 1);
				pTmpPdm += 8192;
			}
			gettimeofday(&tv, NULL);
			end = (int64_t)tv.tv_sec * 1000  + tv.tv_usec/1000;
			printf("Normal Mode : (FrameTime/RunningTime) = %lld / %lld (%lld%%)\n", totalTime, end - start, (end-start)*100/totalTime );
			hWave->Write(pResult, numFrames * FRAME_LENGTH * channels * sizeof(short));
			hWave->Close();
			delete hWave;
		}
	}

	printf("done\n");

	free(pResult);
	free(pPDM);

	return 1;
}


static void usage( const char *appName )
{
	printf("Usage : %s [options]\n", appName);
	printf("   -i [input file]  : setting input file name\n");
	printf("   -o [output file] : setting output file path\n");
	printf("   -c [1 or 4]      : single channel mode value(1 or 4)\n");
}

int32_t main( int32_t argc, char *argv[] )
{
	int opt;
	char *inputFile = NULL;
	char *outputFile = NULL;
	int channelMode = 0;

	while (-1 != (opt = getopt(argc, argv, "hi:o:c:")))
	{
		switch( opt )
		{
			case 'h':
				usage(argv[0]);
				break;
			case 'i':
				inputFile = strdup(optarg);
				break;
			case 'o':
				outputFile = strdup(optarg);
				break;
			case 'c':
				channelMode = atoi(optarg);
				break;
		}
	}

	if( !inputFile || !outputFile )
	{
		usage( argv[0] );
		return -1;
	}

	if( channelMode  )
	{
		set_cpu_freq( 800000 );
		single_main( inputFile, outputFile, (channelMode==4)?4:1 );
		set_cpu_freq( 1000000 );
		single_main( inputFile, outputFile, (channelMode==4)?4:1 );
		set_cpu_freq( 1200000 );
		single_main( inputFile, outputFile, (channelMode==4)?4:1 );
		return 0;
	}
	else
	{
		set_cpu_freq( 800000 );
		pdm_main( inputFile, outputFile );
		set_cpu_freq( 1000000 );
		pdm_main( inputFile, outputFile );
		set_cpu_freq( 1200000 );
		pdm_main( inputFile, outputFile );
	}
}
