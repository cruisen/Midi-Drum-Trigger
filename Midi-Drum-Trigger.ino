/* CD to Piezeo to Midi
 *
 * by: sd-pro
 *
 * Stephan Streichhahn
 *
 * 2020
 * 
 * Support by Nikolai von Krusenstiern
 *
 */

////////////////
// INIT
////////////////

bool DEBUG = false ;


////////////////
// Arduino
////////////////

// Arduino PADs
int padMax = 1;
int pad[] = {A3};        // First element is pad[0] !!!
byte midiKey[] = {38};   // The Midi Channel for this Pad

// Arduino Midi Serial Out Baud Rate
long midiRate = 115200L;
long serialRate = 9600L;


////////////////
// Calibration
////////////////

// Calibration AD to MIDI
float analogResolution = 1024.;  // Analog Digital Amplitude Resolution
float midiResolution   =  128.;  //   Midi Digital Amplitude Resolution


////////////////
// Midi
////////////////

// Midi Channel & Note On / Off
byte midiChannel = 0 ; // 0-15 for 1 to 16
byte noteOn =  144 + midiChannel; 
byte noteOff = 128 + midiChannel;

// Dead Times for AD sampling
int deadTime1 = 50;    // between Midi Notes On and Midi Notes Off
int deadTime2 = 50;    // between Midi Notes Off and Midi Notes On
int deadTime3 =  0;     // between individual Midi Messages


////////////////
// AUDIO
////////////////

// Noise Gate & Limiter & Compressor
float noiseGate       =  120. ;	  // Off: set to 0; Noise Gate on Anlog Sample Amplitude >= 0
float compressorKnee  =  500. ;   // Limiter on Anlog Sample Amplitude >= noiseGate & <= limit
float limit           = 1000. ;   // Off: set to 1023; Limiter on Anlog Sample Amplitude <= analogResolution - 1 


bool checkValues      = false ;   // if True, check if 0 <= noiseGate <= compressorKnee <= limit <= analogResolution - 1 
bool noiseGateDynamic = true  ;   // if True, Amplitude < noiseGate = 0 and Ampitide >= noisegate := Amplitude
                                  // if False, Amplitude < noiseGate = 0 and Ampitide >= noisegate = (Amplitude - noiseGate) * Gradient
bool compressorOn     = true  ;   // if True, Compressor is ON


float analogInToMidiCalibration = ( (   midiResolution - 1 )                    /    ( analogResolution - 1 ) );
float noiseGateEnhancerGardient = ( ( analogResolution - 1 )                    /  ( ( analogResolution - 1 ) - noiseGate ) );
float compressorGardient =        ( ( analogResolution - 1 )                    /  ( ( analogResolution - 1 ) - compressorKnee ) ) ;
float compressorLimit =         ( ( ( analogResolution - 1 ) - compressorKnee ) / log( analogResolution     ) ) ;


/*
 * USAGE of Audio:
 * 
 * NoiseGate:
 * NoiseGate OFF:      noiseGate        =    0. ;
 * NoiseGate ON:       noiseGate        =  100. ; // Or Other Values >=0 and <= compressorKnee
 *
 * NoiseGate HARD CUT: noiseGateDynamic = false ;
 * NoiseGate DYNAMIC : noiseGateDynamic = true  ;
 * 
 * Limiter:
 * Limiter OFF:        limit            = 1023. ;
 * Limiter ON :        limit            = 1000. ; // Or Other Values >=  compressorKnee and <= analogResolution - 1
 * 
 * Compressor:
 * Compressor OFF:     compressorOn     = false ;
 * Compressor ON :     compressorOn     = true  ;
 * 
 * Crompessor Knee:    compressorKnee   =  500. ; // Or Other Value >= noiseGate and <= limit
 * 
 * 
 * Audio Parameter Check:
 * Check OFF:           checkValues     = false ;
 * Check ON :           checkValues     = true  ; // WARNING: If this is ON, and above parameters are NOT:
 *                                                // 0 <= noiseGate <= compressorKnee <= limit <= analogResolution - 1 
 *                                                // then the program will stop, Ardunino in endless loop, doing nothing !!!
 * 
 * 
 */

////////////////
// FUNCTIONS
////////////////

// Debug Function
void printDebug( String nameLocal, long valueLocal ) {
    if ( DEBUG ) {
       Serial.print( millis()        ); \
       Serial.print( ': '            ); \
       Serial.print( __FUNCTION__    ); \
       Serial.print( '() '           ); \
       Serial.print( ': '            ); \
       Serial.print( __LINE__        ); \
       Serial.print( ': '            ); \
       Serial.print( nameLocal       ); \
       Serial.print( ': '            ); \
       Serial.print( valueLocal, DEC ); \
       Serial.println(               );      
    } ;
}


// Check Constants
bool checkIfAudioParaOK()
{
    if ( checkValues ) {
      
        // noiseGate Value Check
        if (      noiseGate > compressorKnee or noiseGate < 0 ) {
          return false;
        };
        
        // compressorKnee Value Check
        if ( compressorKnee > limit) {
          return false;
        };
        
        // limit Value Check
        if (          limit > analogResolution - 1 ) {
          return false;
        };
    };

    return true;
}


//  Noise Gate, Compressor and Limiter
float noiseGateCompressorLimiter( int analogLocal ) {

    float linearLocal ;
    float localCompressed ;

    // noise Gate
    if ( analogLocal < noiseGate ) {
        return 0. ;
    }

    // noise gate with or without dynamic enhancer
    if ( noiseGate ) {
        // without dynamic enhancer
        linearLocal =  analogLocal ;    
    } else {
        // with dynamic enhancer
        linearLocal = ( analogLocal - noiseGate ) * noiseGateEnhancerGardient ;    
    }
    
    // Then linear until compressor Knee, or beyond, if compressor is of anyway
    if ( ( linearLocal < compressorKnee ) or ( !compressorOn ) ) {
        return linearLocal ;
    }

    // Compressor above the Compressor Knee
    localCompressed = log( ( linearLocal - compressorKnee ) * compressorGardient + 1 )  * compressorLimit + compressorKnee ; 
    return localCompressed ;
    
} ;


// Write a MIDI Message
void MIDImessage( byte commandLocal, byte dataLocal1, byte dataLocal2 )
{
    printDebug( "commandLocal", long(commandLocal) );
    printDebug( "dataLocal1",   long(dataLocal1)   );
    printDebug( "dataLocal2",   long(dataLocal2)   );
    
    if ( !DEBUG ) {
        Serial.write( commandLocal );
        Serial.write( dataLocal1 );
        Serial.write( dataLocal2 );
    }
} ;


// get audio calibrated for MIDI
int getAnalog( int padLocal ) {
   
    int analogInLocal;
    int analogOutLocal;
        
    analogInLocal = analogRead( padLocal );;
    analogOutLocal = int( noiseGateCompressorLimiter( analogInLocal ) * analogInToMidiCalibration ) ;

    return analogOutLocal ;
}


// Send Midi ON
void midiOn()
{
    int i;
    int analogOutLocal;
    
    for ( i = 0 ; i < padMax ; i++ ) {
      
        // Get Analog reading already converted to Midi velocity
        analogOutLocal = getAnalog( pad[i] );

        // disregard negative Values (due to Low Cut Filter noiseGate = 120)
        if ( analogOutLocal > 0 ) {
            MIDImessage( noteOn, midiKey[i], analogOutLocal );   // turn note on
            delay( deadTime3 );
        }
    }  
}

// Send Midi Off
void midiOff()
{
    int i;
    
    for ( i = 0 ; i < padMax ; i++ ) {
        MIDImessage( noteOff, midiKey[i], 0 );     // turn note off
        delay( deadTime3 );
    }
}

// Prepare SERIAL Out Baud Rate
void prepareSerial() {

    long rate ; 
 
    if ( DEBUG ) {
        rate = serialRate ;
    } else {
        rate = midiRate ;      
    }
    
    Serial.begin( rate );
}


////////////////
// ARDUINO Stetup and Loop
////////////////

// Arduino SETUP
void setup()
{
    int i;

    // INIT ALL PADs as INPUT
    for ( i = 0 ; i < padMax ; i++ ) {
        pinMode( pad[i], INPUT );
    }

    // Prepare Serial
    prepareSerial();
    
    // Check Audio Parameter Configuration
    if ( !checkIfAudioParaOK() ) {
        // IF WRONG AUDIO configuration -> dye
        // AI NvK: Would be better to have a Compiler Error here...
        exit(1);
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


/*
 * END
 */

 
