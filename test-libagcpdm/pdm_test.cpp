#include <stdint.h>
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

int main(int arc, char *argv[])
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


	if (arc!=3) {
		printf("./%s InputFile OutFile \n", argv[0]);
		return 0;
	}


	//	open input file
	fp=fopen(argv[1],"rb");
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
	printf(" InputFile : %s\n", argv[1]);
	printf(" OutputPath : %s\n", argv[2]);
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
		sprintf( outFile, "%s.normal.wav", argv[2] );
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
		printf("Normal Mode : (FrameTime/RunningTime) = %lld / %lld (%lld%)\n", totalTime, end - start, (end-start)*100/totalTime );
		hWave->Write(pResult, numFrames * FRAME_LENGTH * 4 * sizeof(short));
		hWave->Close();
		delete hWave;
	}

	{
		//	open output wave file
		CWAVFile *hWave = new CWAVFile();
		sprintf( outFile, "%s.fast.wav", argv[2] );
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
		printf("Fast Mode : (FrameTime/RunningTime) = %lld / %lld (%lld%)\n", totalTime, end - start, (end-start)*100/totalTime );
		hWave->Write(pResult, numFrames * FRAME_LENGTH * 4 * sizeof(short));
		hWave->Close();
		delete hWave;
	}

	printf("done\n");

	free(pResult);
	free(pPDM);

	return 1;
}
