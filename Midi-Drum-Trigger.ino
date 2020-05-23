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

// and some temp arrays for the pads as well
bool sentOn[] = {false};     // remember, if sent Midi On
int padStoreAndHold[] = {0}; // individual dynamic pad noise gate
float releaseFactor = 0.6 ; // dye off factor for dynamic pad noise gate

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
byte midiChannel =   0 ; // 0-15 for 1 to 16
byte noteOn      = 144 + midiChannel; 
byte noteOff     = 128 + midiChannel;

// Dead Times for AD sampling
int deadTime1 = 50;    // between Midi Notes On and Midi Notes Off
int deadTime2 =  0;    // between Midi Notes Off and Midi Notes On
int deadTime3 =  0;     // between individual Midi Messages


////////////////
// AUDIO
////////////////

// Noise Gate & Limiter & Compressor
bool noiseGateOn         =  true  ;   // if True, NOISE GATE is ON
bool compressorOn        =  true  ;   // if True, COMPRESSOR is ON
bool limiterOn           =  true  ;   // if True, LIMITER is ON

bool noiseGateEnhancerOn = false  ;   // if True, regain dynamic range lost by NOISE GATE: Noise Gate Enhancer
bool enhancerOn          =  true  ;   // if True, regain dynamic rangelost by COMPRESSOR and/or LIMITER: Enhancer

float noiseGate          =   120. ;   // Off: set to 0; Noise Gate on Anlog Sample Amplitude >= 0
float compressorKnee     =   120. ;   // Limiter on Anlog Sample Amplitude >= noiseGate & <= limit
float limiter            =   900. ;   // Off: set to 1023; Limiter on Anlog Sample Amplitude <= analogResolution - 1 

float compressorSpread   =  10^6  ;   // Logarythmic spread 

bool checkValues         = false  ;     // if True, check if 0 <= noiseGate <= compressorKnee <= limit <= analogResolution - 1 


/*
 * USAGE of Audio:
 * 
 * NoiseGate:
 * NoiseGate ON :      noiseGateOn         = true  ;
 * NoiseGate OFF:      noiseGateOn         = false ; 
 *
 * NoiseGate HARD CUT: noiseGateEnhancerOn = false ;
 * NoiseGate DYNAMIC : noiseGateEnhancerOn = true  ;
 * 
 * Limiter:
 * Limiter ON :        limiterOn           = true  ;
 * Limiter OFF:        limiterOn           = false ; 
 * 
 * Compressor:
 * Compressor ON :     compressorOn        = true  ;
 * Compressor OFF:     compressorOn        = false ;
 * 
 * Audio Parameter Check:
 * Check ON :           checkValues     = true  ; // WARNING: If this is ON, and above parameters are NOT in Sequence like
 *                                                // 0 <= noiseGate <= compressorKnee <= limit <= analogResolution - 1 
 *                                                // then the program will stop, Ardunino in endless loop, doing nothing !!!
 * Check OFF:           checkValues     = false ;
 * 
 */

// Global Audio Constants
float analogInToMidiCalibration = ( (   midiResolution - 1 )                    /    ( analogResolution - 1 ) );

float noiseGateEnhancerGradient ;
float enhancerGradient ;
float enhancerMinimum ;


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
        if ( noiseGate > compressorKnee or noiseGate < 0 ) {
          return false;
        };
        
        // compressorKnee Value Check
        if ( compressorKnee > limiter ) {
          return false;
        };
        
        // limit Value Check
        if ( limiter > analogResolution - 1 ) {
          return false;
        };
    };

    return true;
}

// calculate audio settings
void calculateAudioSettings(){

    float ngLocal ;
    float eMaxLocal ;
    
    // for NOISE GATE
    if ( compressorOn ) {
        ngLocal = compressorKnee - 1 ;
    } else {
        if ( limiterOn ) {
            ngLocal = limiter - 1 ;
        } else {
            ngLocal = analogResolution - 1 ;
        }
    }
    noiseGateEnhancerGradient = ngLocal / ( ngLocal - noiseGate );

    // for ENHANCER
    if ( limiterOn ) {
        eMaxLocal = limiter ;
    } else {
        if ( compressorOn ) {
            eMaxLocal =  log( ( (analogResolution - 1) - compressorKnee ) *  compressorSpread ) + compressorKnee ;      
        } else {
            eMaxLocal = analogResolution - 1 ;      
        }
    }

    if ( noiseGateEnhancerOn ) {
        enhancerMinimum = limiter ;
    } else {
        enhancerMinimum = 0 ;
    }

    // 
    enhancerGradient = ( analogResolution - 1 ) / ( eMaxLocal - enhancerMinimum ) ;
}



//  Noise Gate, Compressor and Limiter
float noiseGateCompressorLimiter( int analogLocal ) {

    float linearLocal ;
    float compressedLocal ;

    // NOISE GATE
    if ( analogLocal <= noiseGate ) {
        return 0. ;
    }

    // LIMITER
    if ( linearLocal >= limiter  ) {
        return limiter ;
    }

    // noise gate with or without dynamic enhancer
    if ( ( noiseGateEnhancerOn ) && ( analogLocal < compressorKnee ) ) {
        // with dynamic enhancer
        linearLocal = ( analogLocal - noiseGate ) * noiseGateEnhancerGradient ;
    } else {
        // without dynamic enhancer
        linearLocal =  analogLocal ; 
    }

    // Rest linear until compressor Knee, or beyond, if compressor is off anyway
    if ( ( !compressorOn ) || ( analogLocal <= compressorKnee ) ) {
        return linearLocal ;
    }

    // Compressor above the Compressor Knee
    compressedLocal = log( ( analogLocal - compressorKnee ) * compressorSpread + 1 ) + compressorKnee ; 
    return compressedLocal ;
    
} ;

// ENHANCER
float enhancer( float audioLocal ) {
  
  if ( enhancerOn ) {
    audioLocal = ( audioLocal - enhancerMinimum ) * enhancerGradient ;
  }
  
  return audioLocal ;
}


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
   
    float audio ;
    int analogInLocal;
    int analogOutLocal;
        
    analogInLocal = analogRead( padLocal );;
    audio = noiseGateCompressorLimiter( analogInLocal ) ;
    analogOutLocal = int( enhancer( audio) * analogInToMidiCalibration ) ;

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

        // disregard negative Values (due to Low Cut Filter noiseGate)
        if ( ( analogOutLocal > 0 ) && ( analogOutLocal >= padStoreAndHold[i] ) ) {
            MIDImessage( noteOn, midiKey[i], analogOutLocal );   // turn note on
            
            sentOn[i] = true ;
            padStoreAndHold[i] = analogOutLocal ;
            
            delay( deadTime3 );
        }
    }  
}

// Send Midi Off
void midiOff()
{
    int i;
    
    for ( i = 0 ; i < padMax ; i++ ) {
        if ( sentOn[i] = true ) {
            
            MIDImessage( noteOff, midiKey[i], 0 );     // turn note off
            
            sentOn[i] = false ;
            
            delay( deadTime3 );
        }
        
        padStoreAndHold[i]  = int ( releaseFactor * padStoreAndHold[i] ) ;
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

 
