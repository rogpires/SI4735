/*
  SS4735 SSB Support example with OLED-I2C and Encoder.
  
  This sketch uses the Rotary Encoder Class implementation from Ben Buxton. The source code is included together with this sketch.

  This sketch will download a SSB patch to your SI4735 device (patch_content.h). It will take about 15KB of the Arduino memory.
    
  In this context, a patch is a piece of software used to change the behavior of the SI4735 device.
  There is little information available about patching the SI4735. The following information is the understanding of the author of 
  this project and it is not necessarily correct. A patch is executed internally (run by internal MCU) of the device. 
  Usually, patches are used to fixes bugs or add improvements and new features of the firmware installed in the internal ROM of the device. 
  Patches to the SI4735 are distributed in binary form and have to be transferred to the internal RAM of the device by 
  the host MCU (in this case Arduino). Since the RAM is volatile memory, the patch stored into the device gets lost when you turn off the system.
  Consequently, the content of the patch has to be transferred again to the device each time after turn on the system or reset the device.

  ATTENTION: The author of this project does not guarantee that procedures shown here will work in your development environment. 
  Given this, it is at your own risk to continue with the procedures suggested here. 
  This library works with the I2C communication protocol and it is designed to apply a SSB extension PATCH to CI SI4735-D60. 
  Once again, the author disclaims any liability for any damage this procedure may cause to your SI4735 or other devices that you are using.  

  Features of this sketch: 

  1) Only SSB (LSB and USB);
  2) Audio bandwidth filter 0.5, 1, 1.2, 2.2, 3 and 4Khz;
  3) Eight ham radio bands pre configured;
  4) BFO Control; and
  5) Frequency step switch (1, 5 and 10KHz);

  Main Parts: 
  Encoder with push button; 
  Seven bush buttons;
  OLED Display with I2C protocol;
  Arduino Pro mini 3.3V;  


  By Ricardo Lima Caratti, Nov 2019.
*/

#include <SI4735.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"
#include "Rotary.h"

// Test it with patch_init.h or patch_full.h. Do not try load both.
// #include "patch_init.h"       // SSB patch for whole SSBRX initialization string
#include "patch_full.h"    // SSB patch for whole SSBRX full download

const int size_content = sizeof ssb_patch_content;  // see ssb_patch_content in patch_full.h or patch_init.h

#define AM_FUNCTION 1

// OLED Diaplay constants
#define I2C_ADDRESS 0x3C
#define RST_PIN -1 // Define proper RST_PIN if required.

#define RESET_PIN 12

// Enconder PINs
#define ENCODER_PIN_A 3
#define ENCODER_PIN_B 2

// Buttons controllers
#define AVC_SWITCH 4       // Switch SSB Automatic Volume Control ON/OFF 
#define BANDWIDTH_BUTTON 5 // Used to select the banddwith. Values: 1.2, 2.2, 3.0, 4.0, 0.5, 1.0 KHz
#define VOL_UP 6           // Volume Up
#define VOL_DOWN 7         // Volume Down
#define BAND_BUTTON_UP 8   // Next band
#define BAND_BUTTON_DOWN 9 // Previous band
#define AGC_SWITCH 11      // Switch AGC ON/OF
#define STEP_SWITCH 10     // Used to select the increment or decrement frequency step (1, 5 or 10 KHz)
#define BFO_SWITCH 13      // Used to select the enconder control (BFO or VFO)
// Seek Function

#define MIN_ELAPSED_TIME 200
#define LSB 1
#define USB 2

bool bfoOn = false;
bool disableAgc = true;
bool avc_en = true;

int currentBFO = 0;
int previousBFO = 0;

long elapsedButton = millis();

// Encoder control variables
volatile int encoderCount = 0;

// Some variables to check the SI4735 status
unsigned currentFrequency;
unsigned previousFrequency;
byte currentStep = 1;

byte bandwidthIdx = 2;
char *bandwitdth[] = {"1.2", "2.2", "3.0", "4.0", "0.5", "1.0"};


typedef struct {
  unsigned   minimumFreq;
  unsigned   maximumFreq;
  unsigned   currentFreq;
  unsigned   currentStep;
  byte       currentSSB;
} Band;


Band band[] = {
  {1800, 2000, 1900, 1, LSB}, 
  {3500, 4000, 3700, 1, LSB},
  {7000, 7500, 7100, 1, LSB},
  {10000,10500,10050,1, USB}, 
  {14000, 14300, 14200, 1, USB},
  {18000, 18300, 18100,1,USB},
  {21000, 21400, 21200, 1, USB},
  {27000, 27500, 27220, 1, USB},
  {28000, 28500, 28400, 1, USB}
};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int  currentFreqIdx = 2;


byte rssi = 0;
byte stereo = 1;
byte volume = 0;

// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

SSD1306AsciiAvrI2c display;

SI4735 si4735;

void setup()
{

  // Encoder pins
  pinMode(ENCODER_PIN_A, INPUT);
  pinMode(ENCODER_PIN_B, INPUT);

  pinMode(BANDWIDTH_BUTTON, INPUT);
  pinMode(BAND_BUTTON_UP, INPUT);
  pinMode(BAND_BUTTON_DOWN, INPUT);
  pinMode(VOL_UP, INPUT);
  pinMode(VOL_DOWN, INPUT);
  pinMode(BFO_SWITCH, INPUT);
  pinMode(AGC_SWITCH, INPUT);
  pinMode(STEP_SWITCH,INPUT);
  pinMode(AVC_SWITCH, INPUT);

  display.begin(&Adafruit128x64, I2C_ADDRESS);
  display.setFont(Adafruit5x7);
  delay(500);

  // Splash - Change it for your introduction text.
  display.set1X();
  display.setCursor(0, 0);
  display.print("Si4735 Arduino Library");
  delay(500);
  display.setCursor(30, 3);
  display.print("SSB TEST");
  delay(500);
  display.setCursor(30, 6);
  display.print("By PU2CLR");
  delay(3000);
  display.clear();
  // end Splash

  // Encoder interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

  si4735.setup(RESET_PIN, AM_FUNCTION);

  delay(500);
  
  loadSSB();

  delay(1500);
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentSSB);
  delay(500);
  currentFrequency = previousFrequency = si4735.getFrequency();
  si4735.setVolume(40);
  showBFO();
  showStatus();
}

// Use Rotary.h and  Rotary.cpp implementation to process encoder via interrupt
void rotaryEncoder()
{ // rotary encoder events
  uint8_t encoderStatus = encoder.process();
  if (encoderStatus)
  {
    if (encoderStatus == DIR_CW)
    {
      encoderCount = 1;
    }
    else
    {
      encoderCount = -1;
    }
  }
}

// Show current frequency
void showStatus()
{
  String unit, freqDisplay;
  String bandMode;

  bandMode = String("SSB");
  unit = "KHz";
  freqDisplay = String((float) currentFrequency/1000,3);

  display.set1X();
  display.setCursor(0, 0);
  display.print(String(bandMode));

  display.setCursor(98, 0);
  display.print(unit);

  display.set2X();
  display.setCursor(26, 1);
  display.print("        ");
  display.setCursor(26, 1);
  display.print(freqDisplay);


  // Show AGC Information
  si4735.getAutomaticGainControl();
  display.set1X();
  display.setCursor(0, 4);
  display.print((si4735.isAgcEnabled()) ? "AGC ON " : "AGC OFF");

  display.setCursor(0, 5);
  display.print("          ");
  display.setCursor(0, 5);
  display.print("Step:");
  display.print(currentStep);
  display.print("KHz");
      
  display.set1X();
  display.setCursor(0, 7);
  display.print("           ");
  display.setCursor(0, 7);
  display.print("BW:");
  display.print(String(bandwitdth[bandwidthIdx]));
  display.print("KHz");
}

/* *******************************
   Shows RSSI status
*/
void showRSSI()
{
  int blk;

  display.set1X();
  display.setCursor(70, 7);
  display.print("S:");
  display.print(rssi);
  display.print(" dBuV");
}

/* ***************************
   Shows the volume level on LCD
*/
void showVolume()
{
  display.set1X();
  display.setCursor(70, 5);
  display.print("          ");
  display.setCursor(70, 5);
  display.print("V:");
  display.print(volume);
}


void showBFO() {

  String bfo;

  if ( currentBFO > 0 ) 
      bfo = "+" + String(currentBFO);
  else 
      bfo = String(currentBFO);
       
 
  display.setCursor(70, 4);
  display.print("         ");
  display.setCursor(70, 4);
  display.print("BFO:");
  display.print(bfo);

}

void bandUp() {

  // save the current frequency for the band
  band[currentFreqIdx].currentFreq = currentFrequency;
  if ( currentFreqIdx < lastBand ) {
    currentFreqIdx++;
  } else {
    currentFreqIdx = 0;
  }

  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentSSB);
  currentStep = band[currentFreqIdx].currentStep;
}

void bandDown() {
  // save the current frequency for the band
  band[currentFreqIdx].currentFreq = currentFrequency;
  if ( currentFreqIdx > 0 ) {
    currentFreqIdx--;
  } else {
    currentFreqIdx = lastBand;
  }
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep,  band[currentFreqIdx].currentSSB);
  currentStep = band[currentFreqIdx].currentStep;
}


void loadSSB()
{
  delay(500);
  si4735.queryLibraryId(); // Is it really necessary here? I will check it.
  delay(500);
  si4735.patchPowerUp();
  delay(500);
  si4735.downloadPatch(ssb_patch_content, size_content);
  delay(500);
  // Parameters  
  // AUDIOBW - SSB Audio bandwidth; 0 = 1.2KHz (default); 1=2.2KHz; 2=3KHz; 3=4KHz; 4=500Hz; 5=1KHz;
  // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
  // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
  // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
  // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
  // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable. 
  si4735.setSSBConfig(bandwidthIdx, 1, 0, 1, 0, 1);
  delay(500);
  showStatus();
}

/*
   Main
*/
void loop()
{

  // Check if the encoder has moved.
  if (encoderCount != 0)
  {

    if (bfoOn) {
      currentBFO = (encoderCount == 1) ?  (currentBFO + 50) : (currentBFO - 50);
    } else {
      if (encoderCount == 1)
        si4735.frequencyUp();
      else
        si4735.frequencyDown();
    }
    encoderCount = 0;
  }

  // Check button commands
  if ( (( millis() - elapsedButton) > MIN_ELAPSED_TIME)  && (digitalRead(BANDWIDTH_BUTTON) | digitalRead(BAND_BUTTON_UP) | digitalRead(BAND_BUTTON_DOWN) | digitalRead(VOL_UP) | digitalRead(VOL_DOWN) | 
      digitalRead(BFO_SWITCH) | digitalRead(AGC_SWITCH)  | digitalRead(STEP_SWITCH ) | digitalRead(AVC_SWITCH) ) )
  {

    // check if some button is pressed
    if (digitalRead(BANDWIDTH_BUTTON) == HIGH)
    {
      bandwidthIdx++;
      if (bandwidthIdx > 5)  bandwidthIdx = 0;
      si4735.setSSBAudioBandwidth(bandwidthIdx);
      // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
      if (bandwidthIdx == 0 || bandwidthIdx == 4 || bandwidthIdx == 5)
        si4735.setSBBSidebandCutoffFilter(0); 
      else
        si4735.setSBBSidebandCutoffFilter(1);
        showStatus();
    }
    else if (digitalRead(BAND_BUTTON_UP) == HIGH)
      bandUp();
    else if (digitalRead(BAND_BUTTON_DOWN) == HIGH)
      bandDown();
    else if (digitalRead(VOL_UP) == HIGH)
      si4735.volumeUp();
    else if (digitalRead(VOL_DOWN) == HIGH)
      si4735.volumeDown();
    else if (digitalRead(BFO_SWITCH) == HIGH) {
      bfoOn = !bfoOn;
    } else if ( digitalRead(AGC_SWITCH) == HIGH) {
       disableAgc = !disableAgc; 
       // siwtch on/off ACG; AGC Index = 0. It means Minimum attenuation (max gain)
       si4735.setAutomaticGainControl(disableAgc,1);     
       showStatus(); 
    } else if ( digitalRead(STEP_SWITCH) == HIGH) {
       if (currentStep == 1) 
          currentStep = 5;
       else if ( currentStep == 5) 
          currentStep = 10;
       else 
          currentStep = 1;  
       si4735.setFrequencyStep(currentStep);
       band[currentFreqIdx].currentStep = currentStep;
       showStatus();       
    } else if ( digitalRead(AVC_SWITCH) == HIGH ) {
      avc_en = !avc_en;
      si4735.setSSBAutomaticVolumeControl(avc_en);   
    }
    elapsedButton = millis();
  }


  // Show the current frequency only if it has changed
  currentFrequency = si4735.getFrequency();
  if (currentFrequency != previousFrequency)
  {
    previousFrequency = currentFrequency;
    showStatus();
  }

  // Show RSSI status only if this condition has changed
  if (rssi != si4735.getCurrentRSSI())
  {
    rssi = si4735.getCurrentRSSI();
    showRSSI();
  }

  // Show volume level only if this condition has changed
  if (si4735.getCurrentVolume() != volume)
  {
    volume = si4735.getCurrentVolume();
    showVolume();
  }

  if (currentBFO != previousBFO ) {
    previousBFO = currentBFO;
    si4735.setSSBBfo(currentBFO);
    showBFO();
  }

  delay(50);
}
