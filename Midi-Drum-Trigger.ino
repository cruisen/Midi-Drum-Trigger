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
int padMax = 1;
int pad[] = {A3};        // first element is pad[0] !!!
byte midiKey[] = {38};   // The Midi Channel for this Pad

// Arduino baudRate
int baudRate = 115200;

// Midi Note On / Off
byte midiChannel = 0 ; // 0-15 for 1 to 16

byte noteOn = 144 + midiChannel; 
byte noteOff = 128 + midiChannel;

// Dead Times for AD sampling
int deadTime1 = 50;    // between Midi Notes On and Midi Notes Off
int deadTime2 = 50;    // between Midi Notes Off and Midi Notes On
int deadTime3 = 0;     // between individual Midi Messages

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

// Debug Function

//#define DEBUG
#ifdef DEBUG
   #define DEBUG_PRINT(x)     Serial.print(x)
   #define DEBUG_PRINTDEC(x)  Serial.print(x, DEC)
   #define DEBUG_PRINTLN(x)   Serial.println(x)
#else
   #define DEBUG_PRINT(x)
   #define DEBUG_PRINTDEC(x)
   #define DEBUG_PRINTLN(x)
#endif

// Read form Analog pad
int readAnalog( int padLocal )
{
    int localAnalog;
    int localMidi;
    
    // get Analog reading, calibrate to Midi Value
    localAnalog = analogRead( padLocal );
    localMidi = int(( localAnalog - calibrationOffSet ) * calibrationGradient );
    
    return localMidi ;
} ;

// Write a MIDI Message
void MIDImessage( byte commandLocal, byte dataLocal1, byte dataLocal2 )
{
    #ifdef DEBUG
        DEBUG_PRINT("commandLocal: ");
        DEBUG_PRINTDEC(commandLocal);
        
        DEBUG_PRINT(", dataLocal1: ");
        DEBUG_PRINTDEC(dataLocal1);
        
        DEBUG_PRINT(", dataLocal2:");
        DEBUG_PRINTDEC(dataLocal2);

        DEBUG_PRINTLN("");

    #else
        Serial.write( commandLocal );
        Serial.write( dataLocal1 );
        Serial.write( dataLocal2 );
    #endif
} ;


// Midi ON
void midiOn()
{
    int i;
    
    for ( i = 0 ; i < padMax ; i++ ) {
        // Get Analog reading converted to Midi velocity
        midiVal = readAnalog( pad[i] );
    
        // disregard negative Values (due to Low Cut Filter calibrationOffSet = 120)
        if ( midiVal > 0 )
        {
            MIDImessage( noteOn, midiKey[i], midiVal );   // turn note on
            delay( deadTime3 );
        }
    }  
}

void midiOff()
{
    int i;
    
    for ( i = 0 ; i < padMax ; i++ ) {
        MIDImessage( noteOff, midiKey[i], 0 );     // turn note off
        delay( deadTime3 );
    }
}


////////////////
// ARDUINO Stetup and Loop
////////////////

// Arduino SETUP
void setup()
{
    int i;

    #ifdef DEBUG
        Serial.begin(9600);
    #else
        Serial.begin( baudRate );
    #endif
    
    // init ALL PADs as INPUT
    for ( i = 0 ; i < padMax ; i++ ) {
        pinMode( pad[i], INPUT );
    }
}

// Arduino MAIN LOOP
void loop()
{
    midiOn();
    delay( deadTime1 );
    
    midiOff();
    delay( deadTime2 );
}
