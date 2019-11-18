/*
   Test and validation of the SI4735 Arduino Library.

   By Ricardo Lima Caratti, Nov 2019.
*/

#include <SI4735.h>

#define RESET_PIN 12

#define AM_FUNCTION 1
#define FM_FUNCTION 0


typedef struct {
    unsigned   minimumFreq;
    unsigned   maximumFreq;
    unsigned   currentFreq;
    unsigned   currentStep;    
} Band;


Band band[] = {{4600,5200,4700,5},
               {5700,6200,6000,5},
               {7000,7500,7200,5},
               {9300,10000,9600,5},
               {11400,12200,1800,5},
               {13500,13900,13600,5},
               {15000,15800,15200,5},
               {17400,17900,17600,5},
               {21400,21800,21500,5},
               {27000,27500,27220,1}}; 
 
const int lastBand = (sizeof band / sizeof(Band)) - 1;
int  currentFreqIdx = 2; // 41M



unsigned currentFrequency;
unsigned previousFrequency;

SI4735 si4735;

void setup()
{
  Serial.begin(9600);
  Serial.println("Test and validation of the SI4735 Arduino Library.");
  Serial.println("AM and FM station tuning test.");

  showHelp();

  delay(500);

  si4735.setup(RESET_PIN, FM_FUNCTION);
  
  // Starts defaul radio function and band (FM; from 84 to 108 MHz; 103.9 MHz; step 100KHz)
  si4735.setFM(8400, 10800,  10390, 10);
  
  delay(500);
  
  currentFrequency = previousFrequency = si4735.getFrequency();
  si4735.setVolume(45);
  showStatus();
}


void showHelp() {
  Serial.println("Type F to FM; A to MW; L to LW; and 1 to SW");
  Serial.println("Type U to increase and D to decrease the frequency");
  Serial.println("Type S or s to seek station Up or Down");
  Serial.println("Type + or - to volume Up or Down");
  Serial.println("Type 0 to show current status");  
  Serial.println("Type ? to this help.");
  Serial.println("==================================================");
  delay(1000);
}

// Show current frequency
void showStatus()
{
  
  Serial.print("You are tuned on ");
  if (si4735.isCurrentTuneFM() ) {
    Serial.print(String(currentFrequency / 100.0, 2));
    Serial.print("MHz ");
    Serial.print((si4735.getCurrentPilot())?"STEREO":"MONO");
  } else {
    Serial.print(currentFrequency);
    Serial.print("KHz");
  }
  Serial.print(" [SNR:" );
  Serial.print(si4735.getCurrentSNR());
  Serial.print("dB");
    
  Serial.print(" Signal:" );
  Serial.print(si4735.getCurrentRSSI());
  Serial.println("dBuV]");
  
}

// Main
void loop()
{
  if (Serial.available() > 0)
  {
    char key = Serial.read();
    switch (key)
    {
      case '+':
        si4735.volumeUp();
        break;
      case '-':
        si4735.volumeDown();
        break;
      case 'a':
      case 'A':
        si4735.setAM(570, 1710,  810, 10);
        break;
      case 'f':
      case 'F':
        si4735.setFM(8600, 10800,  10390, 10);
        break;
      case '2': 
           if ( currentFreqIdx < lastBand ) {
              currentFreqIdx++; 
           } else {
            currentFreqIdx = 0; 
           }
           si4735.setAM(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep); 
           break;
      case '1': 
           if ( currentFreqIdx > 0 ) {
              currentFreqIdx--; 
           } else {
            currentFreqIdx = lastBand; 
           }
           si4735.setAM(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep); 
           break;           
      case 'U':
      case 'u':
        si4735.frequencyUp();
        break;
      case 'D':
      case 'd':
        si4735.frequencyDown();
        break;
      case 'S':
        si4735.seekStationUp();
        break;
      case 's':
        si4735.seekStationDown();
        break;
      case '0':
        showStatus();
        break;  
      case '?':
        showHelp();
        break;
      default:
        break;
    }
  }
  delay(100);
  band[currentFreqIdx].currentFreq = currentFrequency = si4735.getFrequency();
  if ( currentFrequency != previousFrequency ) {
    previousFrequency = currentFrequency;
    showStatus();
    delay(300);
  }
}