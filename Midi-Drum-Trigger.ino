/*   
 *    
 *    
 *    CD to Piezo to Midi
 *
 *    Stephan Streichhahn
 *           &
 * Nikolai von Krusenstiern
 * 
 *       Mai 2020
 * 
 * 
 */


////////////////////////////////
// INIT
////////////////////////////////


/*
 * SET TEST ENVIROMENT
 */


bool const HARDWARETEST       = false  ; // turns OFF everything, pure anlaogIn to midiCalibrated(out) as MIDI

bool TO_SERIAL_MONITOR        = false  ; // print DEBUG Mesages to SERIAL MONITOR
bool WITH_MILLIS              = false  ; // add millis() to Debug Message
bool WITH_LINE_NUMBERS        = false  ; // add __LINE__ to Debug Message
bool FORCE_SERIAL_MONITOR_OUT = false  ; // TO_SERIAL_MONITOR ON and MILLIS and __LINE__ anyway ;-)

// RUN ONLY one (1) of the below (4) TESTs, first one true will be done, others IGNORED. Continue for ever.
bool TEST_SOS                 = false  ; // Test the OnBoard LED SOS blinking

// if !FORCE_SERIAL_MONITOR_OUT then below (3) TESTs will set: TO_SERIAL_MONITOR = true; WITH_LINE_NUMBERS = false; WITH_MILLIS = false
bool TEST_AUDIO_CHAIN         = false  ; // Test the noiseGateCompressorExpanderLimiter() FUNCTION:
                                         // INPUT:  sweep analogIn: from [0.. (analogResolution -1)]
                                         // OUTPUT: [0.. (analogResolution -1)]
bool TEST_PEAK_FINDER         = false  ; // Test the peakFinder() FUNCTION:
                                         // INPUT: random analogIn [0.. (analogResolution -1)]
                                         // OUTPUT: [0.. (analogResolution -1)]
bool TEST_DECAY               = false  ; // Test decayFilter mechanism (AI NVK: TBD)
                                         // INPUT: random analogIn [0.. (analogResolution -1)]
                                         // OUTPUT: [0.. (analogResolution -1)]


/*
 * SET AUDIO PARAMETERS
 */

// Noise Gate & Limiter & Compressor
bool  noiseGateOn           = true  ;   // if True, NOISE GATE is ON
bool  compressorOn          = false  ;   // if True, COMPRESSOR is ON, its a Limiting Compressor
bool  expanderOn            = true  ;   // if True, regain dynamic range lost by LIMITER and / or COMPRTRESSOR : midiOut [(0|noiseGate)..1023] ; Expander
bool  limiterOn             = false  ;   // if True, LIMITER    is ON. Without COMPRESSOR ON, this is a hard Limiter 

bool  peakFinderOn          = true  ;   // if True, PEAK FILTER is ON
bool  decayFilterOn         = false  ;   // if True, DECAY FILTER is ON, next Note On, only, if Velocity is decayFactor of current Note On Velocity

float noiseGate             =   100. ;   // Noise Gate on Analog Sample Amplitude: if ( analogIn <= noiseGate      ) { midiOut = 0 }
float compressorThreshold   =   400. ;   // Compressor on Analog Sample Amplitude: if ( analogIn >  compressorThreshold ) { midiOut = compressor( analogIn) }
float compressorRatioOverX  =     5. ;   // Compressor Ratio 1/X
float expanderThreshold     =   400. ;   // Expander on Analog Sample Amplitude: if ( analogIn <  expanderThreshold ) { midiOut = compressor( analogIn) }
float expanderRatioOverX    =     5. ;   // Expander Ratio 1/X
float limiter               =   500. ;   // Limiter    on Analog Sample Amplitude: if ( analogIn >= limiter        ) { midiOut = limiter }

float decayFactor           =     0.5;   // Decay Filter Factor for dynamic pad noise gate

// Timings, non blocking
int const onToOffTime       =    50  ;   // between individual Midi Messages

int const morseDotDuration  =   200  ;   // Morse Code DOT
int const morseDashDuration =   600  ;   // Morse Code DASH ( 3 x DOT)

int const busyDuration      =  1000  ;   // Duration Busy Signal Loop
int const busySignal        =   200  ;   // Duration Busy Signal


/*
 * SET Arduino PARAMETERS
 */

// Arduino PADs
int const     padMax        =    1    ;
int           pad[]         = {A3}    ;  // First element is pad[0] !!!
byte          midiKey[]     = {38}    ;  // The Midi Channel for this Pad

// Arduino Midi Serial Out Baud Rate
long const midiRate         = 115200L ;
long const serialRate       =   9600L ;


////////////////
// SET ANALOG to MIDI Calibration
////////////////

// Calibration AD to MIDI
float const analogResolution = 1024. ;  // Analog Digital Amplitude Resolution
float const midiResolution   =  128. ;  //   Midi Digital Amplitude Resolution


/*
 * SET Midi PARAMETERS
 */

// Midi Channel & Note On / Off
byte const midiChannel =   0 ; // 0-15 for 1 to 16
byte const noteOn      = 144 + midiChannel; 
byte const noteOff     = 128 + midiChannel;


/*
 * SET & Prepare GLOBAL Audio Variables & Arrays
 */

// for AUDIO handling
int analogIn ;

// for PeakFinder
unsigned int peakPointerLocal      =  0 ;
unsigned int historyPoint[ 3 ][ padMax ]     = {} ;

// for DECAY FILTER
unsigned int sentOn[ padMax ]      = {} ;  // remember, when we did sent our Midi On
float        decayFilter[ padMax ] = {} ;  // for each pad: dynamic noise gate (decay Filter), init ARRAY with "0"

// for ANALOG to MIDI CALIBRATION
float analogInToMidiCalibration ;


/*
 * SET other GLOBAL Variables
 */

// for OnBoard LED & LCD
unsigned int triggerLedMillis ;
long         dataForLcdTemp   ;


/*
 * LCD INIT
 */

// include the LCD library code:
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);



////////////////////////////////
// FUNCTIONS
////////////////////////////////

/*
 * Configure Test Enviroment
 */

void configureTestEnviroment()
{
    // HARDWARETEST
    if ( HARDWARETEST ) {

        TO_SERIAL_MONITOR = FORCE_SERIAL_MONITOR_OUT ;

        WITH_LINE_NUMBERS = false ;
        WITH_MILLIS       = false ;

        TEST_AUDIO_CHAIN  = false ;
        TEST_PEAK_FINDER  = false ;
        TEST_DECAY        = false ;

        TEST_SOS          = false ;
        
        noiseGateOn       = false ;
        compressorOn      = false ;
        expanderOn        = false ;
        limiterOn         = false ;
        
        peakFinderOn      = false ;
        decayFilterOn     = false ;
        
    }

    // Turn on TO_SERIAL_MONITOR for: TEST_AUDIO_CHAIN or TEST_PEAK_FINDER or TEST_DECAY
    if ( !TEST_SOS && ( TEST_AUDIO_CHAIN || TEST_PEAK_FINDER || TEST_DECAY ) ) {
        TO_SERIAL_MONITOR     = true  ;
        if ( !FORCE_SERIAL_MONITOR_OUT ) {
            WITH_LINE_NUMBERS = false ;
            WITH_MILLIS       = false ;
        }
    }

    // FORCE_SERIAL_MONITOR_OUT
    if ( FORCE_SERIAL_MONITOR_OUT ) {
        TO_SERIAL_MONITOR     = true  ;        
    }

    // TEST_PEAK_FINDER
    if ( TEST_PEAK_FINDER ) {
        peakFinderOn = true ;
    }
        
    // TEST_DECAY
    if ( TEST_DECAY ) {
       decayFilterOn = true ;
    }
    
}


/*
 * INIT HARDWARE FUNCTIONS
 */

// INIT all PADs
void initPads()
{
      int i;

      // INIT ALL PADs as INPUT
      for ( i = 0 ; i < padMax ; i++ ) {
          pinMode( pad[i], INPUT );
      }

      // prepare Builtin LED for Status and DEBUG messages
      pinMode(LED_BUILTIN, OUTPUT);
}


// Prepare SERIAL Out Baud Rate
void prepareSerial()
{
    long rate ; 
 
    if ( TO_SERIAL_MONITOR ) {
        rate = serialRate ;
    } else {
        rate = midiRate ;      
    }
    
    Serial.begin( rate );    
}


/*
 * TO_SERIAL_MONITOR FUNCTIONS
 */

// add Serial Monito Optinals
void addSerialOptions( int lineLocal )
{
    if ( WITH_MILLIS ) {
        Serial.print( millis()      ); \
        Serial.print( ": "          ); \
    }

    if ( WITH_LINE_NUMBERS ) {
        Serial.print( lineLocal     ); \
        Serial.print( ": "          ); \
    }  
}

// Print to Serial Monitor with Name and Value
void printSerialMonitorNameValue( int lineLocal, String nameLocal, long valueLocal )
{
    if ( TO_SERIAL_MONITOR && !HARDWARETEST) {
        addSerialOptions( lineLocal ) ;

        Serial.print( nameLocal       ); \
        Serial.print( ": "            ); \

        Serial.print( valueLocal, DEC ); \
        Serial.println(               );      
    }
}

// Print to Serial Monitor with Value only
void printSerialMonitorValue( int lineLocal , long valueLocal )
{
    if ( TO_SERIAL_MONITOR ) {
        addSerialOptions( lineLocal ) ;

        Serial.print( valueLocal, DEC ); \
        Serial.println(               );      
    }
}

// Print to Serial Monitor some Text
void printSerialMonitorText( int lineLocal , String textLocal )
{
    if ( TO_SERIAL_MONITOR ) {
        addSerialOptions( lineLocal ) ;

        Serial.print( textLocal       ); \
        Serial.println(               );      
    }
}

void printSerialNewLine()
{
    if ( TO_SERIAL_MONITOR ) {
        Serial.println();
    }
}

/*
 * LCD FUNCTIONS
 */


// print to line on LCD
void printToLineLcd( int lineLocal, String stringLocal )
{
    // Clear Line
    lcd.setCursor(0, lineLocal) ;
    lcd.print( "                " ) ; // Clear second line

    // Print Line
    lcd.setCursor(0, lineLocal) ;
    lcd.print( stringLocal) ; 

    delay(busyDuration) ;
}

// setup LCD
void setupLcd()
{
    // Setup LCD screen.
    lcd.begin(16, 2);
    
    // Print a message to the LCD
        printToLineLcd( 0, "Setup... " ) ;
}

// print COMPLETE on LCD
void showCompleteOnLcd()
{
    // Print a message to the LCD.
    lcd.print("Ready");
    delay(morseDashDuration);
}

// toHex
String toHex( int intLocal )
{
      String stringLocal =  String(intLocal, HEX) ;
      return stringLocal ;
}

// hundreds to HEX
String hundredsToHex( long longLocal )
{
      int hundred = int( ( longLocal +50 ) / 100  ) ;
      String stringLocal = toHex( hundred ) ;
      return stringLocal ;
}


// print Status via LCD
void showStatusOnLcd()
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
        lcd.print( "D" );
    } else {
        lcd.print( "d" );
    }
    lcd.print( toHex( decayFactor * 10 ) ) ;

     // TO_SERIAL_MONITOR status
    if ( TO_SERIAL_MONITOR ) {
        lcd.print( "-M" );
    } else {
        lcd.print( "-m" );
    }

    // HARDWARETEST status
    if ( HARDWARETEST ) {
        lcd.print( "H" );
    } else {

        if ( TEST_SOS ) {
            lcd.print( "S" );
        } else {

            if ( TEST_AUDIO_CHAIN ) {
                lcd.print( "A" );

                TEST_PEAK_FINDER = false ;
                peakFinderOn     = false ;

            } else {
            
                if ( TEST_PEAK_FINDER ) {
                    lcd.print( "P" );
                    TEST_DECAY = false ;
                    decayFilterOn    = false ;

                } else {
                
                    if ( TEST_DECAY ) {
                        lcd.print( "D" );
                    }
                }
            }
        }        
    }      
}


// LCD Warning
void lcdWarning()
{
    lcd.setCursor(0, 1) ; 
    lcd.print( "                " ); // Clear second line

    lcd.setCursor(0, 1);
    if ( TEST_SOS ) {
        lcd.print( "SOS Test" );  
    } else {
        lcd.print( "Check Audio Para" );  
    }
}


// print to LCD
void printLcd( long dataLocal ) {

    // only update LCD, if we have new data and not every time
    if ( millis() % 5 == 0  && dataLocal != 0 && dataLocal != dataForLcdTemp ) {

        // set the cursor to column 0, line 1
        // (note: line 1 is the second row, since counting begins with 0):
        lcd.setCursor(0, 1) ; 
        lcd.print( "                " ); // Clear second line
    
        lcd.setCursor(0, 1);
        lcd.print( analogIn );
    
        lcd.setCursor(5, 1);
        lcd.print( dataLocal );
        
        dataForLcdTemp = dataLocal ;
    }
}


/*
 * LED FUNCTIONS
 */

// LED MORSE SHORT
void morseDot() {
    digitalWrite( LED_BUILTIN, HIGH ) ;
    delay( morseDotDuration ) ; 
    digitalWrite(LED_BUILTIN, LOW ) ;
    delay( morseDotDuration ) ;
}


// LED MORSE LONG
void morseDash() {
    digitalWrite( LED_BUILTIN, HIGH ) ;
    delay( morseDashDuration ) ; 
    digitalWrite( LED_BUILTIN, LOW ) ;
    delay( morseDotDuration ) ;
}


// send three MORSE items in a row
void threeMorse( bool dotLocal )
{
    int i ;
    
    for ( i = 1 ; i <= 3 ; i++ ) {
        if ( dotLocal ) {
            morseDot() ;
        } else {
            morseDash() ;
        }
    }   
    delay( morseDashDuration - morseDotDuration ) ;
}


// Morse SOS vie OnBoard LED
void ledMorseSOS()
{
    do {
        // S      
        threeMorse ( true) ;

        // O      
        threeMorse ( false ) ;

        // S      
        threeMorse ( true ) ;

        // PAUSE for next Word
        delay(morseDashDuration) ;
        delay(morseDashDuration) ;
    
    } while ( true ) ; // Endless loop !!!
    
    exit(1);
 }


// busy blink of OnBoard LED, to show code is running
void ledBusySignal()
{
    bool offLocal = digitalRead(LED_BUILTIN) ;  

    if ( millis() >= triggerLedMillis ) {
        digitalWrite(LED_BUILTIN, !offLocal );

        if ( offLocal ) {
            triggerLedMillis += busyDuration - busySignal;
        } else {
            triggerLedMillis += busySignal ;
        }
    }
}


/*
 * INIT AUDIO FUNCTIONS
 */

// calculate audio settings
void prepareAudioSettings()
{   
    int i ;
       
    // init pad array for onToOffTime 
    for ( i = 0 ; i < padMax ; i++ ) {
        sentOn[i] = 0 ;
    }

    // to MIDI Calibration
    analogInToMidiCalibration = ( ( midiResolution - 1 ) / ( analogResolution - 1 ) );
    //if ( TO_SERIAL_MONITOR ) {
      //  analogInToMidiCalibration = 1 ;
    //}
 
}


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

    if ( TEST_SOS || !checkIfAudioPara() ) {
        lcdWarning();
        ledMorseSOS();
        exit(1);
    } else {
        printToLineLcd( 1, "Audio Para Check" ) ;
    }
}


/*
 * AUDIO CHAIN TEST FUNCTIONS
 */

// TEST_AUDIO_CHAIN
void testAudioChain()
{
    if ( TEST_AUDIO_CHAIN && TO_SERIAL_MONITOR ) {
      
        int i ;
        unsigned int midiOutLocal ;
        float audioLocal ;
        
        for ( i = 0 ; i < analogResolution ; i++ ) {

            analogIn = i ; 
            
            audioLocal = noiseGateCompressorExpanderLimiter( analogIn ) ; 
            midiOutLocal = int( audioLocal * analogInToMidiCalibration ) ;
            
            printSerialMonitorValue( __LINE__ , long( analogIn ) ) ;
            printSerialMonitorValue( __LINE__ , long( midiOutLocal ) ) ;
            printSerialNewLine();

            printLcd( midiOutLocal ) ;
        }
    }
}


// TEST_PEAK_FINDER
void testPeakFinder()
{
    if ( TEST_PEAK_FINDER && TO_SERIAL_MONITOR ) {

        int i ;
        unsigned int analogTestLocal ;
        unsigned int audioLocal ;
        
        for ( i = 0 ; i < analogResolution ; i++ ) {
          
            analogTestLocal = i % 10 ;
            audioLocal  = peakFinder( analogTestLocal, 0 ) * 10 ;

            printSerialMonitorValue( __LINE__ , long( analogTestLocal ) ) ;
            printSerialMonitorValue( __LINE__ , long( audioLocal ) ) ;
            printSerialNewLine();

            printLcd( audioLocal ) ;           
        }
    }
}

// TEST_DECAY
void testDecay()
{
    if ( TEST_DECAY && TO_SERIAL_MONITOR ) {
      
        int i ;
        unsigned int analogTestLocal ;
        unsigned int audioLocal ;
        
        for ( i = 0 ; i < analogResolution ; i++ ) {
          
            analogTestLocal = random( analogResolution ) ;
            audioLocal = peakFinderDecayFilter( analogTestLocal, 0 ) ;
         
            if ( audioLocal > 0 ) {
                sendNoteOnOnePad( audioLocal, 0 ) ;

                printSerialMonitorValue( __LINE__ , long( i ) ) ;
                printSerialMonitorValue( __LINE__ , long( audioLocal ) ) ;
                printSerialNewLine();
            }
            sendNoteOffAllPads();
            
            printLcd( audioLocal ) ;
        }
    }
}


// do Tests
void doAudioTests()
{
    if ( TEST_AUDIO_CHAIN || TEST_PEAK_FINDER || TEST_DECAY ) {
      
        printSerialMonitorText( __LINE__ , "doAudioTests()") ;

        do {
            // TEST_AUDIO_CHAIN
            testAudioChain();
            
            // TEST_PEAK_FINDER
            testPeakFinder();
          
            // TEST_DECAY
            testDecay();
            
        } while ( true ) ; // ENDLESS LOOP !!!
        
        exit(1) ;
    } 
}


/*
 * AUDIO CHAIN FUNCTIONS
 */

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


// Peak Finder
unsigned int peakFinder( int sampleLocal, int padLocal )
{    
    unsigned int peakLocal ;

    // advance pointer
    peakPointerLocal++ ;

    // calc pointers
    unsigned int rise = ( peakPointerLocal + 1 ) % 3 ;
    unsigned int peak = ( peakPointerLocal + 2 ) % 3 ;
    unsigned int fall = ( peakPointerLocal + 3 ) % 3 ;
    
    // overwite oldest with current sample
    historyPoint[ padLocal ][ fall ] =  sampleLocal ;

    // name data points
    unsigned int risePoint = historyPoint[ padLocal ][ rise ] ;
    unsigned int peakPoint = historyPoint[ padLocal ][ peak ] ;
    unsigned int fallPoint = historyPoint[ padLocal ][ fall ] ;

    // find Peek
    if ( risePoint < peakPoint && peakPoint > fallPoint ) {
        peakLocal = peakPoint ;
    } else {
        peakLocal = 0 ;
    }

    return peakLocal ;
}


// peakFinderDecayFilter
float peakFinderDecayFilter(float audioLocal, int padLocal )
{
    unsigned int peakLocal = 0 ;
    float outLocal = 0 ;

    if ( peakFinderOn ) {
      
        // PEAK FINDER      
        peakLocal = peakFinder( audioLocal, padLocal );    
        if ( peakLocal > 0 ) {

            // AND DECAY Filter?
            if ( decayFilterOn && audioLocal < decayFilter[ padLocal ] )  {
                outLocal = 0 ;        
            } else {
                outLocal = noiseGateCompressorExpanderLimiter( peakLocal ) ;                  
            }
        }
        
    } else {

        // DECAY FILTER
        if ( !decayFilterOn || audioLocal >= decayFilter[ padLocal ] )  {
            outLocal = audioLocal ;
        } else {
            outLocal = 0 ;        
        }
    }

    return outLocal ;
}


/*
 * MIDI OUT FUNCTIONS
 */

// Write a MIDI Message
void MIDImessage( byte commandLocal, byte dataLocal1, byte dataLocal2 )
{    
    if ( commandLocal != noteOff ) {
        printSerialMonitorValue( __LINE__ , long( dataLocal2 ) );
        printSerialNewLine();  
    }
    
    printLcd(   long( dataLocal2 ) );
    
    if ( !TO_SERIAL_MONITOR ) {
        Serial.write( commandLocal );
        Serial.write( dataLocal1   );
        Serial.write( dataLocal2   );
    }
}


// Sample and Send Midi Note On
void sampleAllPads()
{
    int i ;
    
    float audioLocal ;
    float   outLocal ;
    
    for ( i = 0 ; i < padMax ; i++ ) {

        // If Note was OFF
        if ( sentOn[i] == 0 ) {
      
            // Get Analog reading
            analogIn   = analogRead( pad[i] ) ;
            audioLocal = noiseGateCompressorExpanderLimiter( analogIn ) ;
            outLocal   = peakFinderDecayFilter( audioLocal, i ) ;

            //printSerialMonitorValue( __LINE__ , analogIn ) ;
                    
            // Send Note ON
            if ( outLocal > 0 ) {
                sendNoteOnOnePad( outLocal, i ) ;
            }
        }
    }  
}


// Send Note ON
void sendNoteOnOnePad( float outLocal, int padLocal )
{
    int midiOutLocal ;
 
    //printSerialMonitorNameValue(__LINE__, "analogInToMidiCalibration", analogInToMidiCalibration ) ;
    
    midiOutLocal = int( outLocal * analogInToMidiCalibration ) ;

    MIDImessage( noteOn, midiKey[ padLocal ], midiOutLocal ) ;   // turn note on               
    sentOn[ padLocal ] = millis() ;

    if ( decayFilterOn ) {
        decayFilter[ padLocal ] = outLocal ;             // set decay filter
    }
} 


// Send Midi Note Off
void sendNoteOffAllPads()
{
    int i;
    
    for ( i = 0 ; i < padMax ; i++ ) {
        if (  sentOn[i] > 0 && millis() > sentOn[i] + onToOffTime ) {
            
            MIDImessage( noteOff, midiKey[i], 0 ) ;     // turn note off
            sentOn[i] = 0 ;
        
        } else {
          
            if ( decayFilterOn ) {
                decayFilter[i] = decayFactor * decayFilter[i] ; // reduce decay Filter value
            }
        }
    }
}



////////////////////////////////
// ARDUINO Setup and Loop
////////////////////////////////



// Arduino SETUP
void setup() 
{
    // configure Test Enviroment
    configureTestEnviroment() ;
    
    // setup LCD
    setupLcd() ;
    
    // INIT ALL PADs
    initPads() ;

    // Prepare Serial
    prepareSerial() ;
    
    // Calculate Audio Settings
    prepareAudioSettings() ;

    // Show Complete on LCD
    showCompleteOnLcd() ;

    // show Audio Setup
    showStatusOnLcd() ;
    
    // Check Audio Parameter Configuration
    checkIfAudioParaOK() ;

    // do Audio Function Tests, if any
    doAudioTests() ;

    // CopyLeft
    printToLineLcd( 1, "Nikolai  Stephan" ) ;

}

// Arduino MAIN LOOP
void loop()
{
    // OnBoard LED Busy blink
    ledBusySignal() ;

    // AUDIO
    sampleAllPads();    
    sendNoteOffAllPads();
}


/*
 * END
 */
