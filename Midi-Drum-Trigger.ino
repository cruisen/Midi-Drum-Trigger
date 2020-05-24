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

bool DEBUG = true ;
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
bool  noiseGateOn          = true   ;   // if True, NOISE GATE is ON
bool  compressorOn         = false  ;   // if True, COMPRESSOR is ON, its a Limiting Compressor
bool  limiterOn            = false  ;   // if True, LIMITER    is ON. Without COMPRESSOR ON, this is a hard Limiter 
bool  expanderOn           = false  ;   // if True, regain dynamic range lost by LIMITER and / or COMPRTRESSOR : midiOut [(0|noiseGate)..1023] ; Expander

bool  decayFilterOn        = false  ;   // if True, DECAY FILTER is ON, next Note On, only, if Velocity is decayFactor of current Note On Velocity

float noiseGate            =   100. ;   // Noise Gate on Analog Sample Amplitude: if ( analogIn <= noiseGate      ) { midiOut = 0 }
float compressorThreshold  =   400. ;   // Compressor on Analog Sample Amplitude: if ( analogIn >  compressorThreshold ) { midiOut = compressor( analogIn) }
float compressorRatioOverX =     5. ;   // Compressor Ratio 1/X
float limiter              =   800. ;   // Limiter    on Analog Sample Amplitude: if ( analogIn >= limiter        ) { midiOut = limiter }
float expanderThreshold    =   400. ;   // Expander on Analog Sample Amplitude: if ( analogIn <  expanderThreshold ) { midiOut = compressor( analogIn) }
float expanderRatioOverX   =     5. ;   // Expander Ratio 1/X

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
float expanderGradient ;
float expanderInMinimum ;
float analogInToMidiCalibration ;


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
        lcd.print( "-C" );
    } else {
        lcd.print( "-c" );
    }
    lcd.print( hundredsToHex( compressorThreshold ) ) ;
    lcd.print( toHex( compressorRatioOverX ) ) ;

    // LIMITER status
    if ( limiterOn ) {
        lcd.print( "-L" );
    } else {
        lcd.print( "-l" );
    }
    lcd.print( hundredsToHex( limiter ) ) ;

    // EXPANDER status
    if ( expanderOn ) {
        lcd.print( "-E" );
    } else {
        lcd.print( "-e" );
    }
    lcd.print( hundredsToHex( expanderThreshold ) ) ;
    lcd.print( toHex( expanderRatioOverX ) ) ;

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

    long analogInOld ;
    long dataLocalOld ;

    // only update LCD, if we have new data and not every time
    if ( millis() % 7 == 0 && ( analogIn != analogInOld || dataLocal != dataLocalOld ) ) {
        // set the cursor to column 0, line 1
        // (note: line 1 is the second row, since counting begins with 0):
        lcd.setCursor(0, 1) ; 
        lcd.print( "                " ); // Clear second line
    
        lcd.setCursor(0, 1);
        lcd.print( analogIn );
    
        lcd.setCursor(5, 1);
        lcd.print( dataLocal );
    
        analogInOld = analogIn ;
        dataLocalOld = dataLocal ;
    }
}


////////////////
// LED FUNCTIONS
////////////////

// LED MORSE SHORT
void morseShort(int delayShortLocal) {
    digitalWrite(LED_BUILTIN, HIGH) ;
    delay(delayShortLocal) ; 
    digitalWrite(LED_BUILTIN, LOW) ;
    delay(delayShortLocal) ;
}

// LED MORSE LONG
void morseLong(int delayShortLocal, int delayLongLocal) {
    digitalWrite(LED_BUILTIN, HIGH) ;
    delay(delayLongLocal) ; 
    digitalWrite(LED_BUILTIN, LOW) ;
    delay(delayLongLocal) ;
}


// Morse SOS vie OnBoard LED
void ledMorseSOS()
{
    int delayShortLocal = 300 ;
    int delayLongLocal  = 2 *  delayShortLocal;

    int i ;
    
    do {
        // S      
        for ( i = 1 ; i <= 3 ; i++ ) {
            morseShort( delayShortLocal ) ;
        }
        delay(delayLongLocal) ;

        // O     
        for ( i = 1 ; i <= 3 ; i++ ) {
            morseLong( delayShortLocal, delayLongLocal ) ;
        }
        delay(delayLongLocal) ;
       
        // S      
        for ( i = 1 ; i <= 3 ; i++ ) {
            morseShort( delayShortLocal ) ;
        }
        delay(delayLongLocal) ;

        // PAUSE
        delay(delayLongLocal) ;
        delay(delayLongLocal) ;
    
    } while ( true ) ; // Endless loop !!!
    exit(1);
 }


// TEST
void testOutToSerial()
{
    if ( DEBUG && TEST ) {
        int i ;
        int analogOutLocal ;
        
        for ( i = 0 ; i < analogResolution ; i++ ) {

            analogIn = i ; 
            
            analogOutLocal = analogChain( analogIn );
            printDebugValue( long( analogIn ) ) ;
            printDebugValue( long( analogOutLocal ) ) ;
            Serial.println();

            printLcd( analogOutLocal ) ;
        }
    }
}

// random blink of OnBoard LED, to show code is running
void ledRandomBusySignal()
{
    int randNumber = random(3) ;
    
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
    // to MIDI Calibration
    analogInToMidiCalibration = ( ( midiResolution - 1 ) / ( analogResolution - 1 ) );
    if ( DEBUG && TEST ) {
        analogInToMidiCalibration = 1 ;
    }
}


////////////////
// AUDIO FUNCTIONS
////////////////

//  Noise Gate, Compressor and Limiter (on analogSignal) [as Waterfall]
float noiseGateCompressorExpanderLimiter( int analogLocal )
{
    float linearLocal ;

    // NOISE GATE
    if ( noiseGateOn && analogLocal <= noiseGate ) {
        return 0. ;
    }

    linearLocal = analogLocal ;

    // COMPRESSOR
    if (  compressorOn && analogLocal > compressorThreshold  ) {
        linearLocal   = ( linearLocal - compressorThreshold ) / compressorRatioOverX + compressorThreshold ;
    }

    // EXPANDER
    if (  expanderOn   && analogLocal < expanderThreshold  ) {
        linearLocal   = ( linearLocal - expanderThreshold  ) / expanderRatioOverX + expanderThreshold ;
    }

    // LIMITER (Hard Limiter)
    if ( limiterOn && analogLocal >= limiter  ) {
        linearLocal = limiter ;
    }

    // Avoid Clipping
    if ( linearLocal >= analogResolution ) 
    {
        linearLocal = analogResolution -1 ; 
    }
    
    return linearLocal ;
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

//  apply Audio function and then calibrate for MIDI
int analogChain( int analogInLocal )
{ 
    float audioLocal     ;
    int   analogOutLocal ;
       
    audioLocal     = noiseGateCompressorExpanderLimiter( analogInLocal ) ;
    analogOutLocal = int( audioLocal * analogInToMidiCalibration ) ;

    return analogOutLocal ;
}


////////////////
// MIDI NOTE ON & OFF FUNCTIONS
////////////////

// Send Midi Note ON
void sampleAndSendNoteOn()
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


// Send Midi Note Off
void sendNoteOff()
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
    delay(1000);

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
    
    sampleAndSendNoteOn();
    delay( deadTime1 );
    
    sendNoteOff();
    delay( deadTime2 );
}


/*
 * END
 */
