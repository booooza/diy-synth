// MIDI Library
#include <MIDI.h>

// Mozzi
#include <MozziGuts.h>
#include <AutoMap.h> // maps unpredictable inputs to a range
#include <ADSR.h> // simple ADSR envelope generator
#include <LowPassFilter.h> // low pass filter
#include <mozzi_midi.h> // mozzi midi
#include <Oscil.h> // oscillator 
#include <Smooth.h> // infinite impulse response low pass filter for smoothing signals

// Custom wavetables
#include <moog-square-c3.h> // moog wavetable for carrier

MIDI_CREATE_DEFAULT_INSTANCE();

// use #define for CONTROL_RATE, not a constant
#define CONTROL_RATE 128 // Hz, powers of 2 are most reliable

// audio oscillator
Oscil<MOOG_SQUARE_C3_NUM_CELLS, AUDIO_RATE> aMoogCarrier(MOOG_SQUARE_C3_DATA);
Oscil<MOOG_SQUARE_C3_NUM_CELLS, AUDIO_RATE> aModulator(MOOG_SQUARE_C3_DATA);
Oscil<MOOG_SQUARE_C3_NUM_CELLS, CONTROL_RATE> kIntensityMod(MOOG_SQUARE_C3_DATA);

// envelope generator
ADSR <CONTROL_RATE, AUDIO_RATE> envelope;

// Desired carrier frequency max and min, for AutoMap
const int MIN_CARRIER_FREQ = 440;
const int MAX_CARRIER_FREQ = 22;

// Desired filter frequency max and min, for AutoMap
const int MIN_FILTER_FREQ = 230;
const int MAX_FILTER_FREQ = 1;

// desired intensity max and min, for AutoMap (inverted)
const int MIN_INTENSITY = 700;
const int MAX_INTENSITY = 10;

// desired mod speed max and min, for AutoMap
const int MIN_MOD_SPEED = 1;
const int MAX_MOD_SPEED = 10000;

// Input definitions
const int FREQ_PIN = 0;
const int MOD_PIN = 2;
const int INTENSITY_PIN = 4;
const int FILTER_PIN = 6;

LowPassFilter aLowPassFilter;

int mod_ratio = 5; // brightness (harmonics)
long fm_intensity; // carries control info from updateControl to updateAudio

// smoothing for intensity to remove clicks on transitions
float smoothness = 0.95f;
Smooth <long> aSmoothIntensity(smoothness);
Smooth <long> aSmoothLowPassFilter(smoothness);

// AutoMap Functions
AutoMap kMapFilterFreq(0,1023,MIN_FILTER_FREQ,MAX_FILTER_FREQ);
AutoMap kMapCarrierFreq(0,1023,MIN_CARRIER_FREQ,MAX_CARRIER_FREQ);
AutoMap kMapIntensity(0,1023,MIN_INTENSITY,MAX_INTENSITY);
AutoMap kMapModSpeed(0,1023,MIN_MOD_SPEED,MAX_MOD_SPEED);

void HandleNoteOn(byte channel, byte note, byte velocity) {
  int mod_freq = mtof(float(note)) * mod_ratio;
  aModulator.setFreq(mod_freq);
  aMoogCarrier.setFreq(mtof(float(note)));
  envelope.noteOn();
}

void HandleNoteOff(byte channel, byte note, byte velocity) {
  envelope.noteOff();
}

void setup() {
  Serial.begin(115200);

  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  MIDI.setHandleNoteOn(HandleNoteOn);  // Put only the name of the function
  MIDI.setHandleNoteOff(HandleNoteOff);  // Put only the name of the function
  // Initiate MIDI communications, listen to all channels (not needed with Teensy usbMIDI)
  MIDI.begin(1);

  envelope.setADLevels(255,64);
  envelope.setTimes(50,200,10000,200); // 10000 is so the note will sustain 10 seconds unless a noteOff comes

  // set default values
  aLowPassFilter.setResonance(0);

  aMoogCarrier.setFreq(440); // default frequency
  startMozzi(CONTROL_RATE);
}


void updateControl(){
  // read the input
  MIDI.read();
  int filter_value = mozziAnalogRead(FILTER_PIN);
  int intensity_value = mozziAnalogRead(INTENSITY_PIN);
  float mod_value = mozziAnalogRead(MOD_PIN);

  // map the input
  int filter_freq = kMapFilterFreq(filter_value);
  int intensity_calibrated = kMapIntensity(intensity_value);
  float mod_speed = (float)kMapModSpeed(mod_value)/1000;
  
  // smoothen values
  int smooth_filter_freq = aSmoothLowPassFilter.next(filter_freq);

  // calculate the fm_intensity
  fm_intensity = ((long)intensity_calibrated * (kIntensityMod.next()+128))>>8; // shift back to range after 8 bit multiply
  
  // set cutoff frequency
  aLowPassFilter.setCutoffFreq(smooth_filter_freq);
  // aLowPassFilter.setCutoffFreq(50);

  envelope.update();

  // set mod speed
  kIntensityMod.setFreq(mod_speed);
}


int updateAudio(){
  long modulation = aSmoothIntensity.next(fm_intensity) * aModulator.next();
  char carrier = aMoogCarrier.phMod(modulation);
  char filtered = aLowPassFilter.next(carrier);
  return (int) (envelope.next() * filtered)>>8;
}


void loop(){
  audioHook();
}