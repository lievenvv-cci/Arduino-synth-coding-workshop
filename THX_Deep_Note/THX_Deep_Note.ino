/* ====================================
 * "SOUND SYNTHESIS WITH ARDUINO"
 *  CCI Technical Skills Workshop
 * 
 * Agnes Cameron & Lieven van Velthoven 
 * 
 * THX Deep Note Arduino version ;)
 * 
 * ====================================
 */

#include <Arduino.h>
#include <FspTimer.h>

// Audio output sample rate:
#define sampleRate 44100  // 'CD quality'


//============== THX ==========================================
//==============================================================

unsigned long startTime = 0;
unsigned long elapsed = 0;

#define GLIDE_DURATION 9000  // 9 seconds buildup
#define NUM_VOICES 28        // 28 simultaneous 'voices'

float currentFrequencies[NUM_VOICES];
float startFrequencies[NUM_VOICES];
float targetFrequencies[NUM_VOICES];

float baseFrequencies[NUM_VOICES] = {
  36, 36, 36, 36, 36, 36, 36, //36, 36,
  72, 72, 72, 72, 72, 72, 72, 72, //72,  // 72,
  144, 144, 144,                       // 144,
  288, 288,
  432, 432,
  576, 576,
  864,  //864,
  1152,
  1458,
  1728
};  //, 3524.16, 4405.2,5286.24,6167.28,7929.36, 13215.6 //1486.755 };

uint32_t phases[NUM_VOICES];
uint32_t phaseIncrements[NUM_VOICES];

uint16_t waveTable[256];
const float PHASE_FACTOR = 4294967296.0 / sampleRate;
//==============================================================
//==============================================================




//=========================================
void setup() {

  // ======================================
  // Initialize outputs  
  // ==================
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect...
  }

  // Enable the DAC hardware and set resolution to 12-bit
  SetupAudioOutput();
  //=======================================
  //=======================================


  //====== THX ============
  //=======================

  // Build a "Cinematic Saw" (Sawtooth + Sine + Triangle wave blend)
  
  // track our min and max wave values, so we can scale them to use the full 12-bit range
  float minVal = 100.0;
  float maxVal = -100.0;

  // fill up the wave table
  for (int i = 0; i < 256; i++) {
    float t = i / 256.0;                        //  goes from 0 to 1 (0% to 100%)
    float saw = 2.0 * t - 1.0;                  // -1 to 1
    float tri = 1.0f - fabsf(4.0f * t - 2.0f);  // -1 to 1
    float sine = sinf(t * 2.0 * PI);            // -1 to 1

    float total = (0.20 * saw + 0.20 * sine + 0.60 * tri);
    float normalizedTotal = (total + 0.805) * 0.71;

    waveTable[i] = (uint16_t)(normalizedTotal * 4095);

    minVal = min(minVal, total);
    maxVal = max(maxVal, total);
  }
  Serial.println(minVal);
  Serial.println(maxVal);

  // for all ~30 'voices':
  for (int i = 0; i < NUM_VOICES; i++) {
    startFrequencies[i] = random(150, 350);                                // start at random frequencies between 150 and 350hz
    targetFrequencies[i] = baseFrequencies[i] * (random(995, 1005) / 1000.0);  // de-tune slightly for extra effect
    currentFrequencies[i] = startFrequencies[i];
    phases[i] = random(0, 255);
  }

  startTime = millis();
  //=======================
  //=======================


}

//========================================
void loop() {
  // UpdateAudio() simlpy fills the output buffer with new samples when needed.
  // It calls NEXT_SAMPLE(), which is where you will want to put your own code!!
  UpdateAudio();

  // MessWithTheAudioSpeed();
}





// =============================================================================================
// RETRIEVE (OR GENERATE) THE NEXT AUDIO SAMPLE!
// THIS IS WHERE YOUR OWN CODE WILL GO!!
//
// (don't worry about the UpdateAudio() and SetupAudioOutput() functions...
// All they do is send audio samples one-by-one into the output pin of your Arduino,
// without ever skipping one.)

// The demo code here creates the audio samples for our THX Deep Note re-creation.
inline uint16_t NEXT_SAMPLE() {

  // ============== THX ================

  if (elapsed < GLIDE_DURATION + 5000) {  // 9sec Glide + 5s hold
    elapsed = millis() - startTime;

    // calculate progress for the first (glide) part of the sound
    float progress = (float)elapsed / GLIDE_DURATION;
    progress = min(progress, 1.0); // make sure it is never more than 1

    // The Curve: make frequencies change slowly at first, and then faster and faster
    float curve = progress * progress * progress * progress;

    uint32_t mixedSample = 0; // the combination of all the voices
    for (int i = 0; i < NUM_VOICES; i++) {
      // Smoothly update frequency
      float f = startFrequencies[i] + (targetFrequencies[i] - startFrequencies[i]) * curve;
      phaseIncrements[i] = (uint32_t)(f * PHASE_FACTOR);

      phases[i] += phaseIncrements[i];
      mixedSample += waveTable[phases[i] >> 24];
    }

    // Master volume swell
    int32_t swell = (elapsed < 3000) ? (elapsed * 256 / 3000) : 256;
    swell = (elapsed > GLIDE_DURATION + 4600) ? (256 - ((elapsed - (GLIDE_DURATION + 4600)) * 256 / 400)) : swell;

    //
    return ((uint16_t)((mixedSample / NUM_VOICES)) * swell) >> 8;  // + 2048;

    //====================================
    //====================================
  }
  else return 2048;
}
// =============================================================================================




// Just for fun ;)
void MessWithTheAudioSpeed() {
  // ============================
  // MESS WITH THE AUDIO SPEED!
  // uint32_t nextTimerPeriod = 1088; // 44.1KHz (standard 'sample rate' / audio playback speed)

  uint32_t nextTimerPeriod = (uint32_t)((sinf(millis() * 0.001f) + 1.0f) * 2000.0f) + 100;
  R_GPT4->GTPR = nextTimerPeriod;
  R_GPT4->GTCR = 0x01;  // start
  // ============================
}






//=============================================================================================
// This bit of code sets up the audio engine. You probably DON'T want to change anything here!!
//=============================================================================================

// The audio output ring buffer (512 bytes = 256 samples)
#define bufferSize 2048
uint16_t audioBuffer[bufferSize] __attribute__((aligned(4096)));

// Where in the buffers are we currently?
int audioBufferIndex = 0;
// The hardware timer responsible for outputting samples at a rock solid pace, regardless of what goes on in Loop()/UpdateAudio()
FspTimer audioOutputTimer;

//==============================================================================================
void SetupAudioOutput() {
  // 1. Power on the DAC (bit 20)
  R_MSTP->MSTPCRD &= ~(1U << 20);

  // 2. Configure DAC (The critical VREF fix)
  R_DAC->DACR = 0x5F;      // DAOE0=1 (Enable Output)
  R_DAC->DADPR = 0;        // Right-aligned data
  R_DAC->DAVREFCR = 0x01;  // VREF = AVCC0/AVSS0

  // 3. Route GPT4 Underflow (Event 0x21) to DMA Channel 0
  R_ICU->DELSR[0] = 0x7D;  // 0x21 = AGT_TIMER 1

  // 4. DMAC0 Configuration (Ring Buffer Mode)
  R_DMA->DMAST = 1;  // Master DMA Enable
  R_DMAC0->DMSAR = (uint32_t)audioBuffer;
  R_DMAC0->DMDAR = (uint32_t)&R_DAC->DADR[0];
  R_DMAC0->DMCRA = 0;  // Free-running

  // DMAMD: Source Inc (10), SARA 512-bytes (01001), Dest Fixed (00)
  // R_DMAC0->DMAMD = 0x8900;  // 256 (512) buffer size
  R_DMAC0->DMAMD = 0x8C00;  // 2048 (4096) buffer size 

  // DMTMD: Normal Mode (00), Source Repeat Area (10), 16-bit (01), Hardware Trig (01)
  R_DMAC0->DMTMD = 0x2101;

  // 5. Fire it up!
  R_DMAC0->DMCNT = 1;  // Enable DMA Channel 0

  // 6. Timer Setup (GPT 4 - 44.1kHz via FspTimer)
  audioOutputTimer.begin(TIMER_MODE_PERIODIC, GPT_TIMER, 4, sampleRate, 50.0, nullptr, nullptr);
  audioOutputTimer.open();
  audioOutputTimer.start();  // Start the timer

  Serial.println("Audio Engine Running...");
}


//===================================================================
// This code fills up the output buffer with new samples when needed.
// You probably DON'T want to change anything here!!
//===================================================================
void UpdateAudio() {

  // Calculate how much space is in the ring buffer ahead of where the audio output is reading from:
  // Where is the DMA currently reading?
  int readIndex = (R_DMAC0->DMSAR - (uint32_t)audioBuffer) >> 1;  // Subtract start address from current address, divide by 2 (because 16-bit)
  int unread_samples = audioBufferIndex - readIndex;

  if (unread_samples < 0) {        // all samples read;
    unread_samples += bufferSize;  // swap to other buffer
  }

  int space_left = bufferSize - unread_samples - 1;

  // If there is room, feed the buffer with your audio array
  while (space_left > 1) {
    space_left--;
    audioBufferIndex++;
    audioBufferIndex %= bufferSize;  // Wrap around

    audioBuffer[audioBufferIndex] = NEXT_SAMPLE();  // GET ACTUAL AUDIO DATA!
  }
}





// 256-element Sine table (Amplitude 0-255)
const uint8_t sinTable[256] = {
  128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170, 173,
  176, 179, 182, 185, 188, 190, 193, 196, 198, 201, 203, 206, 208, 211, 213, 215,
  218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 238, 240, 241, 243, 244,
  245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255,
  255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246,
  245, 244, 243, 241, 240, 238, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220,
  218, 215, 213, 211, 208, 206, 203, 201, 198, 196, 193, 190, 188, 185, 182, 179,
  176, 173, 170, 167, 165, 162, 158, 155, 152, 149, 146, 143, 140, 137, 134, 131,
  128, 124, 121, 118, 115, 112, 109, 106, 103, 100, 97, 93, 90, 88, 85, 82,
  79, 76, 73, 70, 67, 65, 62, 59, 57, 54, 52, 49, 47, 44, 42, 40,
  37, 35, 33, 31, 29, 27, 25, 23, 21, 20, 18, 17, 15, 14, 12, 11,
  10, 9, 7, 6, 5, 5, 4, 3, 2, 2, 1, 1, 1, 0, 0, 0,
  0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 4, 5, 5, 6, 7, 9,
  10, 11, 12, 14, 15, 17, 18, 20, 21, 23, 25, 27, 29, 31, 33, 35,
  37, 40, 42, 44, 47, 49, 52, 54, 57, 59, 62, 65, 67, 70, 73, 76,
  79, 82, 85, 88, 90, 93, 97, 100, 103, 106, 109, 112, 115, 118, 121, 124
};