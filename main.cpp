/*
 * Digital Signal Processing
 * Tiffany Liu
 * 
 * IMPORTANT: for mac, because of permission issues, compile and run in terminal.
 *            Commands:
 *            g++ -o recordAudio main.cpp -lportaudio -framework CoreAudio -framework CoreFoundation -framework AudioToolbox
 *            ./recordAudio

 */

#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <iostream>
#include "portaudio.h"


using namespace std;

//user definitions
#define SAMPLE_RATE  (48000) //44100 or 48000  change to sample rate listed in output
#define FRAMES_PER_BUFFER (1024) //512 and lower causes loud buzzing. 2048 causes out of bounds memory
#define NUM_SECONDS     (5)
#define NUM_CHANNELS    (1)
/* #define DITHER_FLAG     (paDitherOff) */
#define DITHER_FLAG     (0) /**/
/** Set to 1 if you want to capture the recording to a file. */
#define WRITE_TO_FILE   (0)

#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"


typedef struct
{
    int          frameIndex;  /* Index into sample array. */
    int          maxFrameIndex;
    SAMPLE      *recordedSamples;
}
paTestData;


//record function
static int recordCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    paTestData *data = (paTestData *)userData;
    const SAMPLE *rptr = (const SAMPLE *)inputBuffer;
    SAMPLE *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    long framesToCalc;
    long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void)outputBuffer;  // Prevent unused variable warning
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    if (framesLeft < framesPerBuffer) {
        framesToCalc = framesLeft;
        finished = paComplete;
    } else {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    if (inputBuffer == NULL) {
        for (i = 0; i < framesToCalc; i++) {
            *wptr++ = SAMPLE_SILENCE;  // No audio input, fill with silence
            if (NUM_CHANNELS == 1) *wptr++ = SAMPLE_SILENCE;  // Mono input, don't need this, but keep it in case of future changes
        }
    } else {
        for (i = 0; i < framesToCalc; i++) {
            *wptr++ = *rptr++;  // Copy audio data from input buffer to recorded samples
            if (NUM_CHANNELS == 1) *wptr++ = SAMPLE_SILENCE;  // Mono input, ignore right channel
        }
    }
    data->frameIndex += framesToCalc;
    return finished;
}

//playback function
static int playCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
{
    paTestData *data = (paTestData*)userData;
    SAMPLE *rptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    SAMPLE *wptr = (SAMPLE*)outputBuffer;
    unsigned int i;
    int finished;
    unsigned int framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) inputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if( framesLeft < framesPerBuffer )
    {
        /* final buffer... */
        for( i=0; i<framesLeft; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
        for( ; i<framesPerBuffer; i++ )
        {
            *wptr++ = 0;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = 0;  /* right */
        }
        data->frameIndex += framesLeft;
        finished = paComplete;
    }
    else
    {
        for( i=0; i<framesPerBuffer; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
        data->frameIndex += framesPerBuffer;
        finished = paContinue;
    }
    return finished;
}

//call main function
int main(void);

//main function
int main(void) {

    PaStreamParameters  inputParameters,
                        outputParameters;
    PaStream*           stream;
    //PaError             err = paNoError;
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("PortAudio initialization failed: %s\n", Pa_GetErrorText(err));
    return -1;
    }
    paTestData          data;
    int                 i;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;
    SAMPLE              max, val;
    double              average;

    
    //print i/o info
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(inputParameters.device); 
    if (deviceInfo) {
    printf("Input device: %s\n", deviceInfo->name);
    printf("Maximum input channels: %d\n", deviceInfo->maxInputChannels);
    printf("Default input latency: %f seconds\n", deviceInfo->defaultLowInputLatency);
    printf("Sample rate: %f\n", deviceInfo->defaultSampleRate);
    }

    const PaDeviceInfo *OutdeviceInfo = Pa_GetDeviceInfo(outputParameters.device); 
    if (deviceInfo){
    printf("Output device: %s\n", OutdeviceInfo->name);
    printf("Default output latency: %f seconds\n", OutdeviceInfo->defaultLowOutputLatency);
    }

    
    data.maxFrameIndex = totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
    data.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(SAMPLE);
    data.recordedSamples = (SAMPLE *) malloc( numBytes );
    if( data.recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        goto done;
    }
   
    for( i=0; i<numSamples; i++ ) data.recordedSamples[i] = 0;

    if( err != paNoError ) goto done;


    inputParameters.device = Pa_GetDefaultInputDevice();  /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = NUM_CHANNELS;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,                  /* &outputParameters, */
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              recordCallback,
              &data );
    if( err != paNoError ) goto done;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto done;
    printf("\nRecording... \n"); fflush(stdout);

    while( ( err = Pa_IsStreamActive( stream ) ) == 1 )
    {
        Pa_Sleep(1000);
        printf("index = %d\n", data.frameIndex ); fflush(stdout);
    }
    if( err < 0 ) goto done;

    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto done;

    /* Measure maximum peak amplitude. */
    max = 0;
    average = 0.0;
    for( i=0; i<numSamples; i++ )
    {
        val = data.recordedSamples[i];
        if( val < 0 ) val = -val; /* ABS */
        if( val > max )
        {
            max = val;
        }
        average += val;
    }

    average = average / (double)numSamples;

    printf("sample max amplitude = "PRINTF_S_FORMAT"\n", max );
    printf("sample average = %lf\n", average );

    /* Write recorded data to a file. */
#if WRITE_TO_FILE
    {
        FILE  *fid;
        fid = fopen("recorded.raw", "wb");
        if( fid == NULL )
        {
            printf("Could not open file.");
        }
        else
        {
            fwrite( data.recordedSamples, NUM_CHANNELS * sizeof(SAMPLE), totalFrames, fid );
            fclose( fid );
            printf("Wrote data to 'recorded.raw'\n");
        }
    }
#endif

    /* Playback recorded data.  -------------------------------------------- */
    data.frameIndex = 0;

  
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default output device.\n");
        goto done;
    }
    outputParameters.channelCount = 2;                     /* stereo output */
    outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    printf("\n=== Now playing back. ===\n"); fflush(stdout);
    err = Pa_OpenStream(
              &stream,
              NULL, /* no input */
              &outputParameters,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              playCallback,
              &data );
    if( err != paNoError ) goto done;

    if( stream )
    {
        err = Pa_StartStream( stream );
        if( err != paNoError ) goto done;
        
        printf("Waiting for playback to finish.\n"); fflush(stdout);

        while( ( err = Pa_IsStreamActive( stream ) ) == 1 ) Pa_Sleep(100);
        if( err < 0 ) goto done;
        
        err = Pa_CloseStream( stream );
        if( err != paNoError ) goto done;
        
        printf("Done.\n"); fflush(stdout);
    }
   
    //CLOSE PROGRAM
done:
    
    Pa_Terminate();

    // free memory in case it wasnt freed 
    if (data.recordedSamples != NULL) {
        free(data.recordedSamples);
        data.recordedSamples = NULL;  // Avoid double-free by setting to NULL
    }

    // Error handling
    if (err != paNoError) {
        fprintf(stderr, "An error occurred while using the PortAudio stream\n");
        fprintf(stderr, "Error number: %d\n", err);
        fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
        err = 1; 
    }
    
    
    return err;
}
