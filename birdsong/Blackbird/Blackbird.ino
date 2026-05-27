#include <FspTimer.h>

// AO writes true analog out
#define OUT_PIN A0
#define NUM_SAMPLES   256

const float sampleRate = 44100.0;
volatile float fOut = 1000.0;
volatile float vol = 0.5;

volatile int phInc;                  // dds phase increment
volatile unsigned long phAcc;        // dds phase accumulator
unsigned long tuningWord;            // dds tuning word (M)
FspTimer Timer;

uint8_t sineData[NUM_SAMPLES] = {
  128, 131, 134, 137, 140, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174,
  177, 179, 182, 185, 188, 191, 193, 196, 199, 201, 204, 206, 209, 211, 213, 216,
  218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 239, 240, 241, 243, 244,
  245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255,
  255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246,
  245, 244, 243, 241, 240, 239, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220,
  218, 216, 213, 211, 209, 206, 204, 201, 199, 196, 193, 191, 188, 185, 182, 179,
  177, 174, 171, 168, 165, 162, 159, 156, 153, 150, 147, 144, 140, 137, 134, 131,
  128, 125, 122, 119, 116, 112, 109, 106, 103, 100,  97,  94,  91,  88,  85,  82,
   79,  77,  74,  71,  68,  65,  63,  60,  57,  55,  52,  50,  47,  45,  43,  40,
   38,  36,  34,  32,  30,  28,  26,  24,  22,  21,  19,  17,  16,  15,  13,  12,
   11,  10,   8,   7,   6,   6,   5,   4,   3,   3,   2,   2,   2,   1,   1,   1,
    1,   1,   1,   1,   2,   2,   2,   3,   3,   4,   5,   6,   6,   7,   8,  10,
   11,  12,  13,  15,  16,  17,  19,  21,  22,  24,  26,  28,  30,  32,  34,  36,
   38,  40,  43,  45,  47,  50,  52,  55,  57,  60,  63,  65,  68,  71,  74,  77,
   79,  82,  85,  88,  91,  94,  97, 100, 103, 106, 109, 112, 116, 119, 122, 125
};

// IRq is interrupt request

void setup () {
  pinMode(OUT_PIN, OUTPUT);
  fOut = 400.0;                                // set output frequency in Hz
  tuningWord = pow(2, 32) * fOut / sampleRate;
  Serial.begin(115200);
  delay(5);
  

  // turn on the FSP timer in the renesas chip
  uint8_t timerType = GPT_TIMER;
  int8_t channel = FspTimer::get_available_timer(timerType);
  
  Timer.begin(TIMER_MODE_PERIODIC, GPT_TIMER, channel, sampleRate, 0.0f, &TC4_Handler, nullptr);
  Timer.setup_overflow_irq(); // Required to trigger interrupt
  Timer.open();
  Timer.start();
}

void hill (int base_freq, int speed) {
  vol = 0.0;
  int i = base_freq;


  while(i<base_freq + 400) {
    if (i%20 == 0) {
      speed = speed - 1;
      if (speed <=1) speed = 1;
    }

    vol = vol + 0.02;
    if (vol >= 1.0) vol = 1.0;

    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(1500);
    i=i+speed;
  }

  while(i>=base_freq) {
    if (i%20 == 0) {
      speed = speed + 1;
    }

    if (i <= base_freq + 200) {
      vol = vol - 0.02;
      if (vol <= 0.0) vol = 0.0;
    }

    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(2000);
    i=i-speed;
  }
 
}

void trill (int base_freq, int top_freq, int speed, int trills) {
  int j=0;
  while (j<trills) {
    int i = base_freq;
    
    while(i<base_freq + top_freq) {
      fOut = i;                                  // set output frequency in Hz
      tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
      delayMicroseconds(1);
      i=i+speed;
    }

    while(i>=base_freq) {
      fOut = i;                                  // set output frequency in Hz
      tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
      delayMicroseconds(1);
      i=i-speed;
    }
    j++;
  }
}

void dip (int base_freq, int speed) {
  int i = base_freq + 900;

  while(i>=base_freq) {
    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(1000);
    i=i-speed;
  }

  speed = speed - 2;
  while(i<base_freq + 300) {
    if (i%20 == 0) {
      speed = speed - 1;
      if (speed <=1) speed = 1;
    }

    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(1500);
    i=i+speed;
  } 

  trill(i, 400, 30, 8);
}

void swoop (int base_freq) {
  int i = base_freq + 100;
  int speed = 10;

  while(i>=base_freq) {
    if (i%20 == 0) {
      speed = speed -1;
      if (speed <=1) speed = 1;
    }

    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(150);
    i=i-speed;
  }

  while(i<=base_freq+200) {
    if (i%20 == 0) {
      speed = speed +1;
      speed = 1;
    }

    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(100);
    i=i+speed;
  }

  while(i<=base_freq+400) {
    if (i%20 == 0) {
      speed = speed-1;
      if (speed <=1) speed = 1;
    }

    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(100);
    i=i+speed;
  }

  while(i>=base_freq+200) {
    if (i%50 == 0) {
      speed = speed+1;
    }

    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(150);
    i=i-speed;
  }

  while(i>=base_freq) {
    if (i%20 == 0) {
      speed = speed -1;
      if (speed <=1) speed = 1;
    }

    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(150);
    i=i-speed;
  }
}

void hold (int base_freq, int duration){

  vol = 0.0;
  int i = base_freq;
  int speed = 10;

  while(i<base_freq + 400) {
    vol = vol + 0.02;
    if (vol >= 0.7) vol = 0.7;

    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(250);
    i=i+speed;
  }

  delayMicroseconds(duration);

  speed = 3;

  while(i>=base_freq) {
    if (i <= base_freq + 200) {
      if(i%20 == 0) {
        speed = speed + 1;
      }
      vol = vol - 0.02;
      if (vol <= 0.0) vol = 0.0;
    }

    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(500);
    i=i-speed;
  }
}

void wibble (int base_freq) {
  int wait = 1;
  int i = base_freq + 300;
  int exp_add = 5;

  // sharp slope up
  while(i<base_freq + 500) {
    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    delayMicroseconds(1);
    i=i+exp_add;
  }

  // fall off slowly then more rapidly
  while(i>base_freq) {
    fOut = i;                                  // set output frequency in Hz
    tuningWord = pow(2, 32) * fOut / sampleRate;  // DDS tuning word for target frequency
    int exp_add = 8;

    if(i%30 == 0) {
      exp_add = exp_add + 1;
      if( exp_add >= 5) exp_add = 5;
    }

    i=i-exp_add;
    delayMicroseconds(1);
  }

}

void rest_del(int del) {
  fOut = 0;
  tuningWord = pow(2, 32) * fOut / sampleRate;
  delay(del);
}

void loop () {

  vol = 0.8;

  hill(1500, 15);
  rest_del(500);
  dip(1200, 15);


  rest_del(800);
  hold(900, 5000);
  rest_del(400);
  hold(1300, 1800);
  rest_del(500);
 
  vol = 0.6;
  wibble(2800);
  rest_del(150);
  wibble(2800);
  rest_del(150);
  wibble(2100);
  rest_del(500);

  // trill(1100, 400, 1, 3);
  swoop(1100);
  rest_del(1500);
}

void TC4_Handler(timer_callback_args_t *p_args)
{

  // increment 32 bit phase accumulator by the tuning word amount, then
  // use the (tableAddrWidth) number of most significant bits of the phase
  // accumulator as a phase increment, which is the index position in the sine 
  // data table to get a sample from
  // the phase accumulator is always incrementing and when it reaches the maximum 
  // number possible in 32 bits, it rolls over and continues accumulating from 0, so
  // the phase increment (index pointer) is also continuously incrementing by some 
  // amount based on target frequency and rolling over to continue accessing sine table
  // data at the required intervals to generate a sine wave at the desired frequency.
  phAcc += tuningWord;
  phInc = phAcc >> 24; // 32 - 8 bytes for word

  // send current sine data sample to the DAC
  analogWrite(OUT_PIN, sineData[phInc]*vol);
}