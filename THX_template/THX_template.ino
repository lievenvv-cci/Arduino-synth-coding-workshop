/* ====================================
 * "SOUND SYNTHESIS WITH ARDUINO"
 *  CCI Technical Skills Workshop
 * 
 * Agnes Cameron & Lieven van Velthoven 
 * 
 *  DEMO 1: audio sample playback
 *    (mess with playback speed!)
 *                    
 * ====================================
 */

#include <Arduino.h>
#include <FspTimer.h>

// Audio output sample rate:
#define sampleRate 44100  // 'CD quality'

//=========================================
void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect...
  }

  // Enable the DAC hardware and set resolution to 12-bit
  SetupAudioOutput();
}
//========================================

//========================================
void loop() {
  
  // UpdateAudio() simlpy fills the output buffer with new samples when needed.
  // It calls NEXT_SAMPLE(), which is where you will want to put your own code!!
  UpdateAudio();
}
//========================================




// =============================================================================================
// RETRIEVE (OR GENERATE) THE NEXT AUDIO SAMPLE!
// THIS IS WHERE YOUR OWN CODE WILL GO!!
//
// (don't worry about the UpdateAudio() and SetupAudioOutput() functions...
// All they do is send audio samples one-by-one into the output pin of your Arduino,
// without ever skipping one.)

// No synthesis going on (yet)!
uint16_t NEXT_SAMPLE() {

  // YOUR CODE GOES HERE!

}
// =============================================================================================









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
//===================================================================