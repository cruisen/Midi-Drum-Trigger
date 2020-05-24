/* CD to Piezo to Midi
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
bool TEST  = true ;


////////////////
// LCD
////////////////

// include the library code:
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);


////////////////
// Arduino
////////////////

// Arduino PADs
int   padMax            =    1    ;
int   pad[]             = {A3}    ;  // First element is pad[0] !!!
byte  midiKey[]         = {38}    ;  // The Midi Channel for this Pad

// and some temp arrays for the PADs
bool  sentOn[]          = {false} ;  // remember, if sent Midi On
int   decayFilter[]     = {0}     ;  // for each pad: dynamic noise gate (decay Filter), init ARRAY with "0"

// Arduino Midi Serial Out Baud Rate
long  midiRate          = 115200L ;
long  serialRate        =   9600L ;




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
int deadTime3 =  0;    // between individual Midi Messages


////////////////
// AUDIO
////////////////

// Noise Gate & Limiter & Compressor
bool  noiseGateOn         =  true  ;   // if True, NOISE GATE is ON
bool  decayFilterOn       =  true  ;   // if True, DECAY FILTER is ON
bool  compressorOn        =  true  ;   // if True, COMPRESSOR is ON, turns also LIMITER ON (with limit=limiter !), so we have a Limiting Compressor
bool  limiterOn           =  true  ;   // if True, LIMITER    is ON. Without COMPRESSOR ON, this is a hard Limiter 

bool  noiseGateEnhancerOn = false  ;   // if True, regain dynamic range lost by NOISE GATE: midiOut [0..noiseGate..(limiter|1023)] ; Noise Gate Enhancer
bool  enhancerOn          =  true  ;   // if True, regain dynamic range lost by LIMITER   : midiOut [(0|noiseGate)..1023]          ; Enhancer

float noiseGate           =   100. ;   // Noise Gate on Analog Sample Amplitude: if ( analogIn <= noiseGate      ) { midiOut = 0 }
float decayFactor         =     0.5;   // Decay Filter Factor for dynamic pad noise gate
float compressorKnee      =   600. ;   // Compressor on Analog Sample Amplitude: if ( analogIn >  compressorKnee ) { midiOut = compressor( analogIn) }
float limiter             =   900. ;   // Limiter    on Analog Sample Amplitude: if ( analogIn >= limiter        ) { midiOut = limiter }

bool  checkValues         =  true  ;   // if True, check if 0 <= noiseGate <= compressorKnee <= limit <= analogResolution - 1 
                                       // if Check FAILS, send SOS on onboard LED for 100 times !


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
 *                                                // then the program will stop, Arduino in endless loop, doing nothing !!!
 * Check OFF:           checkValues     = false ;
 * 
 */

// Global Audio Constants
int analogIn ;
float noiseGateEnhancerGradient ;
float enhancerGradient ;
float enhancerInMinimum ;
float analogInToMidiCalibration ;



////////////////
// HELPER FUNCTIONS
////////////////

// Debug Function with Name
void printDebugName( String nameLocal, long valueLocal )
{
    if ( DEBUG ) {
       Serial.print( millis()        ); \
       Serial.print( ": "            ); \
       Serial.print( nameLocal       ); \
       Serial.print( ": "            ); \
       Serial.print( valueLocal, DEC ); \
       Serial.println(               );      
    } ;
}

// Debug Function
void printDebug( long valueLocal )
{
    if ( DEBUG ) {
       Serial.print( valueLocal, DEC ); \
       Serial.println(               );      
    } ;
}

// SOS
void SOS()
{
    int delayLongLocal  = 600 ;
    int delayShortLocal = 300 ;

    int i ;

    delay(delayLongLocal) ;
    
    for ( i = 1 ; i <= 99 ; i++ ) {
        for ( i = 1 ; i <= 3 ; i++ ) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(delayShortLocal) ;
            digitalWrite(LED_BUILTIN, LOW);
            delay(delayShortLocal) ;
        }
        
        delay(delayLongLocal) ;
        for ( i = 1 ; i <= 3 ; i++ ) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(delayLongLocal) ;
            digitalWrite(LED_BUILTIN, LOW);
            delay(delayShortLocal) ;
        }
        
        delay(delayLongLocal) ;
       for ( i = 1 ; i <= 3 ; i++ ) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(delayShortLocal) ;
            digitalWrite(LED_BUILTIN, LOW);
            delay(delayShortLocal) ;
        }
        
        delay(delayLongLocal) ;
        delay(delayLongLocal) ;
        delay(delayLongLocal) ;
    }
 }


// TEST
void testOutToSerial()
{
    if ( DEBUG && TEST ) {
        int i ;
        int analogOutLocal ;
        
        for ( i = 0 ; i < analogResolution ; i++ ) {
            analogOutLocal = analogChain( i );
            printDebug( long( i ) ) ;
            printDebug( long( analogOutLocal ) ) ;
            Serial.println();

        }
    }

    int randNumber = random(10) ;
    if ( randNumber == 0 ) {
        digitalWrite(LED_BUILTIN, HIGH);
    }
    if ( randNumber == 1 ) {
        digitalWrite(LED_BUILTIN, LOW);
    }
}


////////////////
// INIT HARDWARE FUNCTIONS
////////////////

// INIT all PADs
void initPads()
{
      int i;

      // INIT ALL PADs as INPUT
      for ( i = 0 ; i < padMax ; i++ ) {
          pinMode( pad[i], INPUT );
      }

      // prepare Builtin LED for DEBUG
      pinMode(LED_BUILTIN, OUTPUT);
}


// Prepare SERIAL Out Baud Rate
void prepareSerial()
{
    long rate ; 
 
    if ( DEBUG ) {
        rate = serialRate ;
    } else {
        rate = midiRate ;      
    }
    
    Serial.begin( rate );    
}


////////////////
// INIT AUDIO FUNCTIONS
////////////////

// Check Audio Constants
bool checkIfAudioPara()
{
    if ( checkValues ) {
      
        // noiseGate Value Check
        if ( noiseGate > compressorKnee or noiseGate < 0 ) {
          return false;
        };
        
        // compressorKnee Value Check
        if ( compressorKnee >= limiter ) {
          return false;
        };
        
        // limit Value Check
        if ( limiter > analogResolution - 1 ) {
          return false;
        };
    };

    return true;
}


// Exit, if not OK
void checkIfAudioParaOK()
{
    // IF WRONG AUDIO configuration -> dye
    // AI NvK: Would be better to have a Compiler Error here...

    if ( !checkIfAudioPara() ) {
        SOS();
        exit(1);
    }
}

// get noiseGateEnhancerGradient
float getNoiseGateEnhancerGradient()
{   
    float noiseGateEnhancerInMaxLocal ;
    float noiseGateEnhancerGradientLocal ;

    // Gradient for NOISE GATE
    if ( compressorOn ) {
            noiseGateEnhancerInMaxLocal = compressorKnee - 1 ;
    } else {
        if ( limiterOn ) {
            noiseGateEnhancerInMaxLocal = limiter - 1 ;
        } else {
            noiseGateEnhancerInMaxLocal = analogResolution - 1 ;
        }
    }
    noiseGateEnhancerGradientLocal = noiseGateEnhancerInMaxLocal / ( noiseGateEnhancerInMaxLocal - noiseGate );

    return noiseGateEnhancerGradientLocal ;
}

// get enhancerGradient
float getEnhancerGradient()
{
    
    float enhancerInMaxLocal    ;
    int   enhancerOutMaxLocal   ;
    float enhancerGradientLocal ;

    // Minimum of ENHANCER IN signal
    if ( noiseGateEnhancerOn ) {
        enhancerInMinimum = limiter ;
    } else {
        enhancerInMinimum = 0 ;
    }

    // Maximum of ENHANCER IN signal
    if ( compressorOn ) {
        enhancerInMaxLocal =  log( ( (analogResolution - 1) - compressorKnee ) +  1 ) + compressorKnee ;      
    } else {
        if ( limiterOn ) {
            enhancerInMaxLocal = limiter ;
        } else {
            enhancerInMaxLocal = analogResolution - 1 ;      
        }
    }

    // Maximum of ENHANCER OUT signal
    if ( enhancerOn ) {
        enhancerOutMaxLocal = analogResolution - 1 ;
    } else {
        enhancerOutMaxLocal = limiter ;
    }

    // Now, calculate the ENHANCER Gradient
    enhancerGradientLocal = ( enhancerOutMaxLocal ) / ( enhancerInMaxLocal - enhancerInMinimum ) ;

    return enhancerGradientLocal ;
}

// calculate audio settings
void calculateAudioSettings()
{
    // NOISE_GATE_ENHANCER
    noiseGateEnhancerGradient = getNoiseGateEnhancerGradient() ;

    // ENHANCER
    enhancerGradient = getEnhancerGradient();
   
    // If COMPRESSOR is on, turn on ENHANCER
    if ( compressorOn ) {
        enhancerOn = true ;
    }

    // to MIDI Calibration
    analogInToMidiCalibration = ( ( midiResolution - 1 ) / ( analogResolution - 1 ) );
    if ( TEST ) {
        analogInToMidiCalibration = 1 ;
    }
}


////////////////
// AUDIO FUNCTIONS
////////////////

//  Noise Gate, Compressor and Limiter (on analogSignal) [as Waterfall]
float noiseGateCompressorLimiter( int analogLocal )
{
    float linearLocal ;
    float compressedLocal ;

    // NOISE GATE
    if ( noiseGateOn && analogLocal <= noiseGate ) {
        return 0. ;
    }

    // NOISE GATE: with or without dynamic enhancer?
    if ( noiseGateOn && noiseGateEnhancerOn ) {
        linearLocal = ( analogLocal - noiseGate ) * noiseGateEnhancerGradient ;
    } else {
        linearLocal =  analogLocal ; 
    }

    // Rest linear until compressor Knee
    if (  analogLocal <= compressorKnee  ) {
        return linearLocal ;
    }

    // Compressor above the Compressor Knee (and will be enhanced to limiter, or even to analogResolution - 1)
    if ( compressorOn ) {
        compressedLocal = log( ( analogLocal - compressorKnee ) + 1 ) + compressorKnee ; 
        return compressedLocal ;
    }

    // LIMITER (Hard Limiter)
    if ( limiterOn && analogLocal >= limiter  ) {
        return limiter ;
    }

    return linearLocal ;
}


// ENHANCER, on analogSignal
float enhancer( float audioLocal )
{
  float outLocal ; 
  
  if ( enhancerOn ) {
      outLocal = ( audioLocal - enhancerInMinimum ) * enhancerGradient ;
  } else {
      outLocal = audioLocal ;
  }
  
  return outLocal ;
}


////////////////
// MIDI OUT FUNCTIONS
////////////////

// Write a MIDI Message
void MIDImessage( byte commandLocal, byte dataLocal1, byte dataLocal2 )
{
    //printDebugName( "commandLocal", long(commandLocal) );
    //printDebugName( "dataLocal1",   long(dataLocal1)   );
    //printDebugName( "dataLocal2",   long(dataLocal2)   );
    
    printDebug( long( dataLocal2 ) );

    // set the cursor to column 0, line 1
    // (note: line 1 is the second row, since counting begins with 0):
    lcd.setCursor(0, 1);
    lcd.print( "                       " );

    lcd.setCursor(0, 1);
    lcd.print( analogIn );

    lcd.setCursor(5, 1);
    lcd.print( dataLocal2 );
    
    if ( !DEBUG ) {
        Serial.write( commandLocal );
        Serial.write( dataLocal1 );
        Serial.write( dataLocal2 );
    }
}


////////////////
// AUDIO IN & AUDIO MANIPULATION FUNCTIONS
////////////////

// get audio calibrated for MIDI
int analogChain( int analogInLocal )
{ 
    float audioLocal     ;
    int   analogOutLocal ;
       
    audioLocal     = noiseGateCompressorLimiter( analogInLocal ) ;
    analogOutLocal = int( enhancer( audioLocal ) * analogInToMidiCalibration ) ;

    return analogOutLocal ;
}


////////////////
// HANDLE MIDI IN & OUT FUNCTIONS
////////////////

// Send Midi ON
void midiOn()
{
    int i;
    int analogOutLocal;
    
    for ( i = 0 ; i < padMax ; i++ ) {
      
        // Get Analog reading already converted to Midi velocity
        analogIn  = analogRead( pad[i] ) ;
        analogOutLocal = analogChain( analogIn );

        // disregard negative Values (due to Low Cut Filter noiseGate) and check if above decay
        if ( ( analogOutLocal > 0 ) && ( !decayFilterOn || ( analogOutLocal >= decayFilter[i] ) ) ) {

            MIDImessage( noteOn, midiKey[i], analogOutLocal ) ;   // turn note on
            
            sentOn[i] = true ;
            decayFilter[i] = analogOutLocal ;             // set decay filter

            delay( deadTime3 ) ;
        }
    }  
}


// Send Midi Off
void midiOff()
{
    int i;
    
    for ( i = 0 ; i < padMax ; i++ ) {
        if ( sentOn[i] == true ) {
            
            MIDImessage( noteOff, midiKey[i], 0 ) ;     // turn note off
            
            sentOn[i] = false ;

            delay( deadTime3 ) ;
        }

        if ( decayFilterOn ) {
           decayFilter[i] = int ( decayFactor * decayFilter[i] ) ; // reduce decay Filter value
        }
    }
}


////////////////
// ARDUINO Setup and Loop
////////////////

// Arduino SETUP
void setup() 
{
    lcd.begin(16, 2);
    // Print a message to the LCD.
    lcd.print("Setup ... ");

    // INIT ALL PADs
    initPads() ;

    // Prepare Serial
    prepareSerial();
    
    // Check Audio Parameter Configuration
    checkIfAudioParaOK() ;

    // Calculate Audio Settings
    calculateAudioSettings() ;

    // Print a message to the LCD.
    lcd.print("Working");

}


// Arduino MAIN LOOP
void loop()
{
    // TEST
    testOutToSerial();
    
    midiOn();
    delay( deadTime1 );
    
    midiOff();
    delay( deadTime2 );
}


/*
 * END
 */
