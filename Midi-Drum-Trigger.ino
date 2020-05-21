/* CD to Piezeo to Midi
 *
 * by: sd-pro
 *
 * Stephan Streichhahn
 *
 * 2020
 *
 */

////////////////
// INIT
////////////////

// Arduino PADs
int pad1= A3;

// Arduino baudRate
int baudRate= 115200;

// Midi Note On / Off
byte midiChannel = 0 ; // 0-15 for 1 to 16

byte noteOn= 144 + midiChannel; 
byte noteOff= 128 + midiChannel;

byte midiKey= 38;

// Dead Times for AD sampling
int del1= 50;
int del2= 50;

// Calibration AD to MIDI, with Low Cut
float AR= 1024.;	// Analog Digital Amplitude Resolution
float MR= 128.;		// Midi Amplitude Resolution

float CO= 120.;		// Calibration Offset: Low Cut on Anlog Sample Amplitude

float CG= ( (MR-1) / ( (AR-1 ) - CO )); // Calibration Gradient

// Variables for IN and OUT
int midiVal; 

////////////////
// FUNCTIONS
////////////////

// Read form Analog pad
int readAnalog(int pad)
{
    int ar;
    int mv;
    // get Analog reading, calibrate to Midi Value
    ar= analogRead (pad);
    mv= int((ar-CO)*CG);
    return mv ;
} ;

// Write a MIDI Message
void MIDImessage(byte command, byte data1, byte data2)
{
    Serial.write(command);
    Serial.write(data1);
    Serial.write(data2);
} ;

// send Midi Note On and than Off, with velocity
void midiOnOff( byte velocity, int d1, int d2) 
{
    MIDImessage (noteOn, midiKey, velocity);   // turn note on
    delay(d1);

    MIDImessage (noteOff, midiKey, 0);     // turn note off
    delay(d2);  
}

////////////////
// ARDUINO Stetup and Loop
////////////////

// Arduino SETUP
void setup()
{
    Serial.begin(baudRate);
    pinMode(pad1, INPUT);
}

// Arduino MAIN LOOP
void loop()
{
   // Get Anlog reading converted to Midi velocity
   midiVal=readAnalog(pad1);

   // disregard negative Values (due to Low Cut Filter CO=120)
   if (midiVal>0)
   {
        midiOnOff(midiVal, del1, del2);
   }
}
