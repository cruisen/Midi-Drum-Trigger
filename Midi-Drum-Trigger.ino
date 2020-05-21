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
int pad1 = A3;

// Arduino baudRate
int baudRate = 115200;

// Midi Note On / Off
byte midiChannel = 0 ; // 0-15 for 1 to 16

byte noteOn = 144 + midiChannel; 
byte noteOff = 128 + midiChannel;

byte midiKey = 38;

// Dead Times for AD sampling
int deadTime1 = 50;
int deadTime2 = 50;

// Calibration AD to MIDI, with Low Cut
float analogResolution = 1024.;	// Analog Digital Amplitude Resolution
float midiResolution = 128.;		// Midi Difital Amplitude Resolution

float calibrationOffSet = 120.;		// Low Cut on Anlog Sample Amplitude

float calibrationGradient = ( ( midiResolution - 1 ) / ( ( analogResolution - 1 ) - calibrationOffSet ));
// Variables for IN and OUT
int midiVal; 

////////////////
// FUNCTIONS
////////////////

// Read form Analog pad
int readAnalog( int pad )
{
    int localAnalog;
    int localMidi;
    
    // get Analog reading, calibrate to Midi Value
    localAnalog = analogRead( pad );
    localMidi = int(( localAnalog - calibrationOffSet ) * calibrationGradient );
    
    return localMidi ;
} ;

// Write a MIDI Message
void MIDImessage( byte command, byte data1, byte data2 )
{
    Serial.write( command );
    Serial.write( data1 );
    Serial.write( data2 );
} ;

// send Midi Note On and than Off, with velocity
void midiOnOff( byte velocityLocal, int deadTimeLocal1, int deadTimeLocal2 ) 
{
    MIDImessage( noteOn, midiKey, velocityLocal );   // turn note on
    delay( deadTimeLocal1 );

    MIDImessage( noteOff, midiKey, 0 );     // turn note off
    delay( deadTimeLocal2 );  
}

////////////////
// ARDUINO Stetup and Loop
////////////////

// Arduino SETUP
void setup()
{
    Serial.begin( baudRate );
    pinMode( pad1, INPUT );
}

// Arduino MAIN LOOP
void loop()
{
   // Get Anlog reading converted to Midi velocity
   midiVal = readAnalog( pad1 );

   // disregard negative Values (due to Low Cut Filter calibrationOffSet = 120)
   if ( midiVal > 0 )
   {
        midiOnOff( midiVal, deadTime1, deadTime2 );
   }
}
