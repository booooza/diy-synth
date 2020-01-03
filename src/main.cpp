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

#define CONTROL_RATE 128 // Hz, powers of 2 are most reliable

// Audio oscillator
Oscil<MOOG_SQUARE_C3_NUM_CELLS, AUDIO_RATE> aMoogCarrier(MOOG_SQUARE_C3_DATA);
Oscil<MOOG_SQUARE_C3_NUM_CELLS, AUDIO_RATE> aModulator(MOOG_SQUARE_C3_DATA);
Oscil<MOOG_SQUARE_C3_NUM_CELLS, CONTROL_RATE> kIntensityMod(MOOG_SQUARE_C3_DATA);

// Envelope generator
ADSR<CONTROL_RATE, AUDIO_RATE> envelope;

// Low-Pass filter
LowPassFilter aLowPassFilter;

// smoothing for intensity to remove clicks on transitions
float smoothness = 0.95f;
Smooth<long> aSmoothIntensity(smoothness);
Smooth<long> aSmoothLowPassFilter(smoothness);
Smooth<long> aSmoothRelease(smoothness);

// Desired carrier frequency max and min, for AutoMap
const int MIN_RELEASE_TIME = 5000;
const int MAX_RELEASE_TIME = 200;

// Desired filter frequency max and min, for AutoMap
const int MIN_FILTER_FREQ = 200;
const int MAX_FILTER_FREQ = 1;

// desired intensity max and min, for AutoMap
const int MIN_INTENSITY = 400;
const int MAX_INTENSITY = 10;

// desired mod speed max and min, for AutoMap
const int MIN_MOD_SPEED = 1;
const int MAX_MOD_SPEED = 5000;

// Input definitions
const int RELEASE_PIN = 0;
const int MOD_PIN = 2;
const int INTENSITY_PIN = 4;
const int FILTER_PIN = 6;

int mod_ratio = 5; // brightness (harmonics)
long fm_intensity; // carries control info from updateControl to updateAudio

MIDI_CREATE_DEFAULT_INSTANCE();

// AutoMap Functions
AutoMap kMapFilterFreq(0,1023,MIN_FILTER_FREQ,MAX_FILTER_FREQ);
AutoMap kMapReleaseTime(0,1023,MIN_RELEASE_TIME,MAX_RELEASE_TIME);
AutoMap kMapIntensity(0,1023,MIN_INTENSITY,MAX_INTENSITY);
AutoMap kMapModSpeed(0,1023,MIN_MOD_SPEED,MAX_MOD_SPEED);


void HandleNoteOn(byte channel, byte note, byte velocity) {
  // Set modulator frequency
  int mod_freq = mtof(float(note)) * mod_ratio;
  aModulator.setFreq(mod_freq);

  // Set carrier frequency
  aMoogCarrier.setFreq(mtof(float(note)));

  // Turn envelope on
  envelope.noteOn();
}

// Function triggered when receiving note off midi signal
void HandleNoteOff(byte channel, byte note, byte velocity) {
  // Turn envelope off
  envelope.noteOff();
}

void setup() {
  // Start Serial Connection for Debugging
  Serial.begin(115200);

  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  MIDI.setHandleNoteOn(HandleNoteOn);
  MIDI.setHandleNoteOff(HandleNoteOff);
  // Initiate MIDI communications, listen to all channels (not needed with Teensy usbMIDI)
  MIDI.begin(1);

  // Set envelope levels
  envelope.setADLevels(255,64);
  envelope.setReleaseLevel(150);
  
  // Set envelope timing
  envelope.setAttackTime(50);
  envelope.setDecayTime(200);
  envelope.setSustainTime(10000); //  Sustain 10 seconds until ADSR receives a noteOff()
  envelope.setReleaseTime(200);

  // Set default values
  aLowPassFilter.setResonance(20);
  aMoogCarrier.setFreq(440);

  // Start mozzi
  startMozzi(CONTROL_RATE);
}


void updateControl(){
  // Read the inputs
  MIDI.read();
  int release_value = mozziAnalogRead(RELEASE_PIN);
  int filter_value = mozziAnalogRead(FILTER_PIN);
  int intensity_value = mozziAnalogRead(INTENSITY_PIN);
  float mod_value = mozziAnalogRead(MOD_PIN);

  // Map the input values
  int release_time = kMapReleaseTime(release_value);
  int filter_freq = kMapFilterFreq(filter_value);
  int intensity_calibrated = kMapIntensity(intensity_value);
  float mod_speed = (float)kMapModSpeed(mod_value)/1000;
  
  // Smoothen values
  int smooth_filter_freq = aSmoothLowPassFilter.next(filter_freq);
  int smooth_release_time = aSmoothRelease.next(release_time);

  // Calculate the FM intensity, shift back to range after 8 bit multiply
  fm_intensity = ((long)intensity_calibrated * (kIntensityMod.next()+128))>>8;

  // Set release time
  envelope.setReleaseTime(smooth_release_time);
  
  // Set cutoff frequency
  aLowPassFilter.setCutoffFreq(smooth_filter_freq);

  // Updates the internal controls of the ADSR
  envelope.update();

  // Set mod speed
  kIntensityMod.setFreq(mod_speed);
}


int updateAudio(){
  // Calculate the modulation
  long modulation = aSmoothIntensity.next(fm_intensity) * aModulator.next();
  // Modulate carrier with modulation
  char carrier = aMoogCarrier.phMod(modulation);
  // Filter the signal
  char filtered = aLowPassFilter.next(carrier);
  // Return the signal
  return (int) (envelope.next() * filtered)>>8;
}


void loop(){
  audioHook();
}