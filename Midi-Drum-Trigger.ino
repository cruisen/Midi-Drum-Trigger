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

// include the LCD library code:
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);


////////////////
// Arduino
////////////////

// Arduino PADs
int const padMax        =    1    ;
int       pad[]         = {A3}    ;  // First element is pad[0] !!!
byte      midiKey[]     = {38}    ;  // The Midi Channel for this Pad

// and some temp arrays for the PADs
long      sentOn[]      = {}      ;  // remember, when we did sent our Midi On
float     decayFilter[] = {}      ;  // for each pad: dynamic noise gate (decay Filter), init ARRAY with "0"

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

// Dead Times
int off2OnTime  =   50 ;    // between individual Midi Messages
int delayShort  =  300 ;    // LCD BLink short
int delayLong   =  600 ;    // LCD BLink long
int delayRandom = 1000 ;    // LCD Random Length


////////////////
// AUDIO
////////////////

// Noise Gate & Limiter & Compressor
bool  noiseGateOn          = true   ;   // if True, NOISE GATE is ON
bool  compressorOn         = false  ;   // if True, COMPRESSOR is ON, its a Limiting Compressor
bool  expanderOn           = false  ;   // if True, regain dynamic range lost by LIMITER and / or COMPRTRESSOR : midiOut [(0|noiseGate)..1023] ; Expander
bool  limiterOn            = false  ;   // if True, LIMITER    is ON. Without COMPRESSOR ON, this is a hard Limiter 

bool  decayFilterOn        = true   ;   // if True, DECAY FILTER is ON, next Note On, only, if Velocity is decayFactor of current Note On Velocity
bool  peakFinderOn         = true   ;   // if True, PEAK FILTER is ON

float noiseGate            =   100. ;   // Noise Gate on Analog Sample Amplitude: if ( analogIn <= noiseGate      ) { midiOut = 0 }
float compressorThreshold  =   400. ;   // Compressor on Analog Sample Amplitude: if ( analogIn >  compressorThreshold ) { midiOut = compressor( analogIn) }
float compressorRatioOverX =     5. ;   // Compressor Ratio 1/X
float expanderThreshold    =   400. ;   // Expander on Analog Sample Amplitude: if ( analogIn <  expanderThreshold ) { midiOut = compressor( analogIn) }
float expanderRatioOverX   =     5. ;   // Expander Ratio 1/X
float limiter              =   500. ;   // Limiter    on Analog Sample Amplitude: if ( analogIn >= limiter        ) { midiOut = limiter }

float decayFactor          =     0.5;   // Decay Filter Factor for dynamic pad noise gate

/*
 * USAGE of Audio:
 * 
 * NoiseGate:
 * NoiseGate ON :      noiseGateOn         = true  ;
 * NoiseGate OFF:      noiseGateOn         = false ; 
 *
 * NoiseGate HARD CUT: noiseGateExpanderOn = false ;
 * NoiseGate DYNAMIC : noiseGateExpanderOn = true  ;
 * 
 * Limiter:
 * Limiter ON :        limiterOn           = true  ;
 * Limiter OFF:        limiterOn           = false ; 
 * 
 * Compressor:
 * Compressor ON :     compressorOn        = true  ;
 * Compressor OFF:     compressorOn        = false ;
 * 
 */

// Global Audio Constants
int analogIn ;
float analogInToMidiCalibration ;
int histPointer[  3 ]           = {} ;
int historyPoint[ 3 ][ padMax ] = {} ;

long dataTemp ;


////////////////
// DEBUG FUNCTIONS
////////////////

// Debug Function with Name
void printDebugValueNameValue( String nameLocal, long valueLocal )
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
void printDebugValue( long valueLocal )
{
    if ( DEBUG ) {
       Serial.print( valueLocal, DEC ); \
       Serial.println(               );      
    } ;
}


////////////////
// LCD FUNCTIONS
////////////////

// toHex
String toHex( int intLocal )
{
      String stringLocal ;

      stringLocal =  String(intLocal, HEX) ;

      return stringLocal ;
  
}

// hundreds to HEX
String hundredsToHex( long longLocal )
{
      int hundred ;
      String stringLocal ;

      hundred = int( ( longLocal +50 ) / 100  ) ;
      stringLocal = toHex( hundred ) ; 
      
      return stringLocal ;
}


// print Status via LCD
void statusLcd()
{
    lcd.clear() ; // Clear LCD

    // NOISE GATE status
    if ( noiseGateOn ) {
        lcd.print( "N" );
    } else {
        lcd.print( "n" );
    }
    lcd.print( hundredsToHex( noiseGate ) ) ;
       
    // COMPRESSOR status
    if ( compressorOn ) {
        lcd.print( "C" );
    } else {
        lcd.print( "c" );
    }
    lcd.print( hundredsToHex( compressorThreshold ) ) ;
    lcd.print( toHex( compressorRatioOverX ) ) ;

    // EXPANDER status
    if ( expanderOn ) {
        lcd.print( "E" );
    } else {
        lcd.print( "e" );
    }
    lcd.print( hundredsToHex( expanderThreshold ) ) ;
    lcd.print( toHex( expanderRatioOverX ) ) ;

    // LIMITER status
    if ( limiterOn ) {
        lcd.print( "L" );
    } else {
        lcd.print( "l" );
    }
    lcd.print( hundredsToHex( limiter ) ) ;

    // PEEK FINDER status
    if ( peakFinderOn ) {
        lcd.print( "P" );
    } else {
        lcd.print( "p" );
    }
      
    // DECAY FILTER status
    if ( decayFilterOn ) {
        lcd.print( "-D" );
    } else {
        lcd.print( "-d" );
    }
    lcd.print( toHex( decayFactor * 10 ) ) ;
}


// LCD Warning
void lcdWarning()
{
    lcd.setCursor(0, 1) ; 
    lcd.print( "                " ); // Clear second line

    lcd.setCursor(0, 1);
    lcd.print( "Check Audio Para" );  
}


// print to LCD
void printLcd( long dataLocal ) {

    // only update LCD, if we have new data and not every time
    if ( ( ( peakFinderOn && !TEST && !DEBUG ) || millis() % 5 == 0 ) && dataLocal != 0 && dataLocal != dataTemp ) {

        // set the cursor to column 0, line 1
        // (note: line 1 is the second row, since counting begins with 0):
        lcd.setCursor(0, 1) ; 
        lcd.print( "                " ); // Clear second line
    
        lcd.setCursor(0, 1);
        lcd.print( analogIn );
    
        lcd.setCursor(5, 1);
        lcd.print( dataLocal );
        
        dataTemp = dataLocal ;
    }
}


////////////////
// LED FUNCTIONS
////////////////

// LED MORSE SHORT
void morseShort() {
    digitalWrite(LED_BUILTIN, HIGH) ;
    delay(delayShort) ; 
    digitalWrite(LED_BUILTIN, LOW) ;
    delay(delayShort) ;
}

// LED MORSE LONG
void morseLong() {
    digitalWrite(LED_BUILTIN, HIGH) ;
    delay(delayLong) ; 
    digitalWrite(LED_BUILTIN, LOW) ;
    delay(delayLong) ;
}


// Morse SOS vie OnBoard LED
void ledMorseSOS()
{
    int i ;
    
    do {
        // S      
        for ( i = 1 ; i <= 3 ; i++ ) {
            morseShort() ;
        }
        delay(delayLong) ;

        // O     
        for ( i = 1 ; i <= 3 ; i++ ) {
            morseLong() ;
        }
        delay(delayLong) ;
       
        // S      
        for ( i = 1 ; i <= 3 ; i++ ) {
            morseShort() ;
        }
        delay(delayLong) ;

        // PAUSE
        delay(delayLong) ;
        delay(delayLong) ;
    
    } while ( true ) ; // Endless loop !!!
    exit(1);
 }


// TEST
void testOutToSerial()
{
    if ( DEBUG && TEST ) {
      
        int i ;
        int midiOutLocal ;
        float audioLocal ;
        
        for ( i = 0 ; i < analogResolution ; i++ ) {

            analogIn = i ; 
            
            audioLocal = noiseGateCompressorExpanderLimiter( analogIn ) ; 
            midiOutLocal = int( audioLocal * analogInToMidiCalibration ) ;
            
            printDebugValue( long( analogIn ) ) ;
            printDebugValue( long( midiOutLocal ) ) ;
            Serial.println();

            printLcd( midiOutLocal ) ;
        }
    }
}

// random blink of OnBoard LED, to show code is running
void ledRandomBusySignal()
{
    int randNumber = random( delayRandom ) ;
    
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
    // noiseGate Value Check
    if ( noiseGate > compressorThreshold or noiseGate > expanderThreshold or noiseGate < 0 ) {
      return false;
    };
    
    // compressorThreshold Value Check
    if ( compressorThreshold >= limiter ) {
      return false;
    };
    
    // expanderThreshold Value Check
    if ( expanderThreshold >= limiter ) {
      return false;
    };
    
    // limit Value Check
    if ( limiter > analogResolution - 1 ) {
      return false;
    };

    // compressorRatioOverX  Value Check
    if ( compressorRatioOverX < 0. || compressorRatioOverX > 16 ) {
      return false;
    };

    // expanderRatioOverX  Value Check
    if ( expanderRatioOverX < 0. || expanderRatioOverX > 16 ) {
      return false;
    };

    // decayFactor  Value Check
    if ( decayFactor <= 0. || decayFactor >= 1 ) {
      return false;
    };
    
    return true;
}


// Exit, if not OK
void checkIfAudioParaOK()
{
    // IF WRONG AUDIO configuration -> dye
    // AI NvK: Would be better to have a Compiler Error here...

    if ( !checkIfAudioPara() ) {
        lcdWarning();
        ledMorseSOS();
        exit(1);
    }
}


// calculate audio settings
void calculateAudioSettings()
{   
    int i ;
    
    // to MIDI Calibration
    analogInToMidiCalibration = ( ( midiResolution - 1 ) / ( analogResolution - 1 ) );
    if ( DEBUG && TEST ) {
        analogInToMidiCalibration = 1 ;
    }

    for ( i = 0 ; i < padMax ; i++ ) {
        sentOn[i] = 0 ;
    }
}


////////////////
// AUDIO FUNCTIONS
////////////////

//  Noise Gate, Compressor and Limiter (on analogSignal) [as Waterfall]
float noiseGateCompressorExpanderLimiter( int analogInLocal )
{
    float analogLocal ;

    // NOISE GATE
    if ( noiseGateOn && analogInLocal <= noiseGate ) {
        return 0. ;
    }

    analogLocal = analogInLocal ;

    // COMPRESSOR
    if (  compressorOn && analogInLocal > compressorThreshold  ) {
        analogLocal   = ( analogLocal - compressorThreshold  ) / compressorRatioOverX + compressorThreshold ;
    }

    // EXPANDER
    if (  expanderOn   && analogInLocal < expanderThreshold    ) {
        analogLocal   = ( analogLocal - expanderThreshold    ) / expanderRatioOverX   + expanderThreshold ;
    }

    // LIMITER (Hard Limiter)
    if ( limiterOn && analogLocal >= limiter  ) {
        analogLocal = limiter ;
    }

    // Avoid Clipping
    if ( analogLocal >= analogResolution ) 
    {
        analogLocal = analogResolution -1 ; 
    }
    
    return analogLocal ;
}


////////////////
// MIDI OUT FUNCTIONS
////////////////

// Write a MIDI Message
void MIDImessage( byte commandLocal, byte dataLocal1, byte dataLocal2 )
{    
    printDebugValue( long( dataLocal2 ) );
    printLcd(   long( dataLocal2 ) );
    
    if ( !DEBUG ) {
        Serial.write( commandLocal );
        Serial.write( dataLocal1 );
        Serial.write( dataLocal2 );
    }
}


////////////////
// AUDIO MANIPULATION FUNCTIONS
////////////////


// Peak Finder
int peakFinder()
{
    int i ;
    
    int rise ;
    int peak ;
    int fall ;

    int risePoint ;
    int peakPoint ;
    int fallPoint ;

    int peekLocal ;

    // calc pointers
    rise = ( histPointer[i] + 1 ) % 3 ;
    peak = ( histPointer[i] + 2 ) % 3 ;
    fall = ( histPointer[i] + 3 ) % 3 ;

    // advance and keep history
    histPointer[  i ]         = rise ;
    historyPoint[ i ][ fall ] = analogIn ;

    // name data points
    risePoint = historyPoint[ i ][ rise ] ;
    peakPoint = historyPoint[ i ][ peak ] ;
    fallPoint = historyPoint[ i ][ fall ] ;

    // find Peek
    if ( risePoint < peakPoint && peakPoint > fallPoint ) {
        peekLocal = peakPoint ;
    } else {
        peekLocal = 0 ;
    }

    return peekLocal ;
}

////////////////
// MIDI NOTE ON & OFF FUNCTIONS
////////////////

// Send Midi Note ON
void sampleAndSendNoteOn()
{
    int i ;
    
    float audioLocal ;
    float outLocal ;

    int peakLocal ;
    int midiOutLocal ;
    
    for ( i = 0 ; i < padMax ; i++ ) {

        // If Note is OFF
        if ( sentOn[i] == 0 ) {
      
            // Get Analog reading
            analogIn   = analogRead( pad[i] ) ;
            audioLocal = noiseGateCompressorExpanderLimiter( analogIn ) ;
        
            // PEAK FINDER
            if ( peakFinderOn ) {
              
                peakLocal = peakFinder();
              
                if ( peakLocal > 0 ) {
                    if ( decayFilterOn && audioLocal < decayFilter[i] )  {
                        outLocal = 0 ;        
                    } else {
                        outLocal = noiseGateCompressorExpanderLimiter( peakLocal ) ;                  
                    }
                }
                
            } else {
    
                // DEACY FILTER
                if ( decayFilterOn && audioLocal > 0 && audioLocal >= decayFilter[i] )  {
                    outLocal = audioLocal ;
                } else {
                    outLocal = 0 ;        
                }
            }
    
            // MIDI OUT
            if ( outLocal > 0 ) {
              
                midiOutLocal = int( outLocal * analogInToMidiCalibration ) ;

                MIDImessage( noteOn, midiKey[i], midiOutLocal ) ;   // turn note on
                
                sentOn[i] = millis() ;
                decayFilter[i] = outLocal ;             // set decay filter
            }
        }
    }  
}


// Send Midi Note Off
void sendNoteOff()
{
    int i;
    
    for ( i = 0 ; i < padMax ; i++ ) {
        if (  sentOn[i] > 0 && millis() > sentOn[i] + off2OnTime ) {
            
            MIDImessage( noteOff, midiKey[i], 0 ) ;     // turn note off
            sentOn[i] = 0 ;

            if ( decayFilterOn ) {
                decayFilter[i] = decayFactor * decayFilter[i] ; // reduce decay Filter value
            }
        }

    }
}


////////////////
// ARDUINO Setup and Loop
////////////////

// Arduino SETUP
void setup() 
{
    // Setup LCD screen.
    lcd.begin(16, 2);
    
    // Print a message to the LCD.
    lcd.print("Setup... ");

    // INIT ALL PADs
    initPads() ;

    // Prepare Serial
    prepareSerial();
    
    // Calculate Audio Settings
    calculateAudioSettings() ;

    // Print a message to the LCD.
    lcd.print("Working");
    delay(delayLong);

    // show Audio Setup
    statusLcd();
    
    // Check Audio Parameter Configuration
    checkIfAudioParaOK() ;

}

// Arduino MAIN LOOP
void loop()
{
    // TEST
    testOutToSerial();

    // OnBoard LED Random blink
    ledRandomBusySignal() ;

    // AUDIO
    sampleAndSendNoteOn();    
    sendNoteOff();
}


/*
 * END
 */
