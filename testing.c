#include "/usr/local/Cellar/espeak-ng/1.51/include/espeak-ng/speak_lib.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_RATE 22050
#define BUFFER_SIZE 2048

#pragma pack(push, 1)
typedef struct {
  char chunkID[4];        // "RIFF"
  uint32_t chunkSize;     // 36 + SubChunk2Size
  char format[4];         // "WAVE"
  char subchunk1ID[4];    // "fmt "
  uint32_t subchunk1Size; // 16 for PCM
  uint16_t audioFormat;   // PCM = 1
  uint16_t numChannels;   // Mono = 1, Stereo = 2
  uint32_t sampleRate;    // Sample rate
  uint32_t byteRate;      // SampleRate * NumChannels * BitsPerSample/8
  uint16_t blockAlign;    // NumChannels * BitsPerSample/8
  uint16_t bitsPerSample; // Bits per sample
  char subchunk2ID[4];    // "data"
  uint32_t subchunk2Size; // NumSamples * NumChannels * BitsPerSample/8
} WAVHeader;
#pragma pack(pop)

#define MAX_SAMPLES                                                            \
  1000000 // Adjust this value based on expected maximum samples

static short *all_samples = NULL;
static uint32_t total_samples = 0;

#define MAX_SAMPLES                                                            \
  1000000 // Adjust this value based on expected maximum samples

static bool synthesis_complete = false;

int SynthCallback(short *wav, int numsamples, espeak_EVENT *events) {
  if (all_samples == NULL) {
    all_samples = (short *)malloc(MAX_SAMPLES * sizeof(short));
    if (all_samples == NULL) {
      printf("Failed to allocate memory for samples\n");
      return 1;
    }
  }

  if (wav != NULL && numsamples > 0) {
    if (total_samples + numsamples > MAX_SAMPLES) {
      printf("Exceeded maximum number of samples\n");
      return 1;
    }
    memcpy(all_samples + total_samples, wav, numsamples * sizeof(short));
    total_samples += numsamples;
    printf("Received %d samples. Total: %u\n", numsamples, total_samples);
  }

  if ((wav == NULL || numsamples == 0) && !synthesis_complete) {
    synthesis_complete = true;
    FILE *file = fopen("servitor_output.wav", "wb");
    if (file == NULL) {
      printf("Failed to open output file: %s\n", strerror(errno));
      return 1;
    }

    WAVHeader header = {.chunkID = {'R', 'I', 'F', 'F'},
                        .chunkSize = 36 + total_samples * sizeof(short),
                        .format = {'W', 'A', 'V', 'E'},
                        .subchunk1ID = {'f', 'm', 't', ' '},
                        .subchunk1Size = 16,
                        .audioFormat = 1,
                        .numChannels = 1,
                        .sampleRate = SAMPLE_RATE,
                        .byteRate = SAMPLE_RATE * sizeof(short),
                        .blockAlign = sizeof(short),
                        .bitsPerSample = 16,
                        .subchunk2ID = {'d', 'a', 't', 'a'},
                        .subchunk2Size = total_samples * sizeof(short)};

    if (fwrite(&header, sizeof(WAVHeader), 1, file) != 1) {
      printf("Error writing WAV header: %s\n", strerror(errno));
      fclose(file);
      return 1;
    }

    if (fwrite(all_samples, sizeof(short), total_samples, file) !=
        total_samples) {
      printf("Error writing WAV data: %s\n", strerror(errno));
      fclose(file);
      return 1;
    }

    fclose(file);
    free(all_samples);
    all_samples = NULL;
    printf("File written and closed. Total samples: %u\n", total_samples);
  }

  return 0;
}

int main() {
  const char *text =
      "The Emperor protects, but having a loaded bolter never hurts";

  // Initialize eSpeak NG
  int result = espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, BUFFER_SIZE, NULL, 0);
  if (result < 0) {
    printf("Failed to initialize eSpeak NG. Error code: %d\n", result);
    return 1;
  }
  printf("eSpeak NG initialized successfully\n");

  // Set voice properties
  espeak_SetVoiceByName("en-us");
  espeak_SetParameter(espeakRATE, 120, 0);
  espeak_SetParameter(espeakPITCH, 20, 0);

  // Set synthesis callback
  espeak_SetSynthCallback(SynthCallback);

  // Synthesize speech
  espeak_ERROR error = espeak_Synth(
      text, strlen(text) + 1, 0, POS_CHARACTER, 0,
      espeakCHARS_AUTO | espeakPHONEMES | espeakENDPAUSE, NULL, NULL);
  if (error != EE_OK) {
    printf("Speech synthesis failed. Error code: %d\n", error);
    return 1;
  }

  // Wait for synthesis to complete
  espeak_Synchronize();

  // Call SynthCallback one last time to ensure the WAV file is written
  if (SynthCallback(NULL, 0, NULL) != 0) {
    printf("Error writing WAV file\n");
    return 1;
  }

  printf("Speech synthesis completed\n");

  // Check file size after synthesis
  FILE *file = fopen("servitor_output.wav", "rb");
  if (file == NULL) {
    printf("Failed to open output file for size check: %s\n", strerror(errno));
    return 1;
  }
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fclose(file);
  printf("Final file size: %ld bytes\n", file_size);

  return 0;
}
