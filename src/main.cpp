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
#include <tables/cos2048_int8.h> // for filter modulation

// Custom wavetables
#include <moog-square-c3.h> // moog wavetable for carrier

 // Created and binds the MIDI interface to the default hardware Serial port
MIDI_CREATE_DEFAULT_INSTANCE();

// Envelope generator
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

// AutoMap Functions
AutoMap kMapCarrierFreq(0,1023,MIN_CARRIER_FREQ,MAX_CARRIER_FREQ);
AutoMap kMapFilterFreq(0,1023,MIN_FILTER_FREQ,MAX_FILTER_FREQ);
AutoMap kMapIntensity(0,1023,MIN_INTENSITY,MAX_INTENSITY);
AutoMap kMapModSpeed(0,1023,MIN_MOD_SPEED,MAX_MOD_SPEED);

// Input definitions
const int FREQ_PIN = 0;
const int MOD_PIN = 2;
const int INTENSITY_PIN = 4;
const int FILTER_PIN = 6;

// Oscillator definitions
Oscil<MOOG_SQUARE_C3_NUM_CELLS, AUDIO_RATE> aMoogCarrier(MOOG_SQUARE_C3_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kFilterMod(COS2048_DATA);
Oscil<MOOG_SQUARE_C3_NUM_CELLS, CONTROL_RATE> kIntensityMod(MOOG_SQUARE_C3_DATA);
Oscil<MOOG_SQUARE_C3_NUM_CELLS, AUDIO_RATE> aModulator(MOOG_SQUARE_C3_DATA);

// Simple integer ratios will produce harmonic (pleasant) sounds while non-simple ratios will produce inharmonic (clanging) sounds.
int mod_ratio = 5; // brightness (harmonics)
long fm_intensity; // carries control info from updateControl to updateAudio
LowPassFilter aLowPassFilter;

// Smoothing
float smoothness = 0.95f;
Smooth <long> aSmoothCarrierFreq(smoothness);
Smooth <long> aSmoothLowPassFilter(smoothness);
Smooth <long> aSmoothIntensity(smoothness);

// Midi note on handling
void HandleNoteOn(byte channel, byte note, byte velocity) {
  // TODO: Set frequency
  envelope.noteOn();
}

// Midi note off handling
void HandleNoteOff(byte channel, byte note, byte velocity) {
  envelope.noteOff();
}

void setup(){
  Serial.begin(115200); // set up the Serial output so we can look at the piezo values // set up the Serial output so we can look at the light level
  startMozzi(); 
  // set default values
  aLowPassFilter.setResonance(100);
}

void updateControl(){
  // read the input
  int freq_value = mozziAnalogRead(FREQ_PIN);
  int filter_value = mozziAnalogRead(FILTER_PIN);
  int intensity_value = mozziAnalogRead(INTENSITY_PIN);
  float mod_value = mozziAnalogRead(MOD_PIN);

  // map the input to carrier frequency
  int carrier_freq = kMapCarrierFreq(freq_value);
  int filter_freq = kMapFilterFreq(filter_value);
  int intensity_calibrated = kMapIntensity(intensity_value);
  float mod_speed = (float)kMapModSpeed(mod_value)/1000;

  // Calculate the modulation frequency to stay in ratio
  int mod_freq = carrier_freq * mod_ratio;

  // calculate the fm_intensity
  fm_intensity = ((long)intensity_calibrated * (kIntensityMod.next()+128))>>8; // shift back to range after 8 bit multiply

  // smoothen values
  int smooth_carrier_freq = aSmoothCarrierFreq.next(carrier_freq);
  int smooth_filter_freq = aSmoothLowPassFilter.next(filter_freq);
  
  // set the carrier frequency
  aMoogCarrier.setFreq(smooth_carrier_freq); 
  aModulator.setFreq(mod_freq);
  kIntensityMod.setFreq(mod_speed);
  aLowPassFilter.setCutoffFreq(smooth_filter_freq);

  // Serial monitor
  Serial.print("Carrier Frequency = ");
  Serial.print(smooth_carrier_freq);
  Serial.print("\t"); // prints a tab
  Serial.print("Modulator Speed = ");
  Serial.print(mod_speed);
  Serial.print("\t"); // prints a tab
  Serial.print("Intensity = ");
  Serial.print(fm_intensity);
  Serial.print("\t"); // prints a tab
  Serial.print("Filter Cutoff = ");
  Serial.print(smooth_filter_freq);
  Serial.print("\t"); // prints a tab
  Serial.println(); 
}

int updateAudio(){
  long modulation = aSmoothIntensity.next(fm_intensity) * aModulator.next();
  char carrier = aMoogCarrier.phMod(modulation);
  char filtered = aLowPassFilter.next(carrier);

  return (int) filtered;
}

void loop(){
  audioHook();
}
