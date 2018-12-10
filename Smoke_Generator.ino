/************************************************************/
/**** Smoke_Generator code pour Bateau modèle numéro 144 ****/
/************************************************************/

//!!!!!!!!!!!!!!!
//!!!ATTENTION!!! Vous devez utiliser le schéma rectificatif paru dans le Bateau Modèle hors série 39 de Décembre 2018
//!!!!!!!!!!!!!!!

// ***********
//  Validate
// ***********
#if not (defined(ARDUINO_AVR_NANO) or defined(ARDUINO_AVR_UNO))
  #error You must select Tools->Board->"Arduino/Genuino Nano"
#endif

// ***********
//    PINS
// ***********
#define CHANNEL_0_PIN	2					// PORT D.2 => Receiver input
#define SMOKE_PIN		A0					// PORT C.0 => Smoke output
#define SMOKE_LED_PIN	A1					// PORT C.1 => Smoke LED output
#define LED_PIN			13					// PORT B.5 => Onboard LED

// ***********
//   MACROS
// ***********
//SMOKE LED
#define SMOKE_LED_ON	(PORTC |=  (1<<PORTC1))
#define SMOKE_LED_OFF	(PORTC &= ~(1<<PORTC1))

//ONBOARD LED
#define LED_ON			(PORTB |=  (1<<PORTB5))
#define LED_OFF			(PORTB &= ~(1<<PORTB5))
#define LED_TOGGLE		(PORTB ^=  (1<<PORTB5))

// ***********
// Global variables and constants.
// ***********
//General
uint16_t millisec=0;
boolean failsafe=true;

//Smoke
boolean smoke_request=false;

//Channels
volatile uint16_t Channel0_Value=0;
volatile boolean Channel0_Ok=false;

/****************************/
/*****       INIT       *****/
/****************************/
void setup()
{
	noInterrupts();
	//Init pins
    pinMode(CHANNEL_0_PIN,	INPUT);			// We don't want INPUT_PULLUP as the 5v may damage some receivers!
    attachInterrupt(0, readChannel0, CHANGE);

	pinMode(SMOKE_PIN,		INPUT);			// We want to emulate the button so INPUT when not pressed and OUTPUT 0 when pressed
	digitalWrite(SMOKE_PIN,	LOW);

	pinMode(SMOKE_LED_PIN,	OUTPUT);		// Smoke LED
	SMOKE_LED_OFF;

	pinMode(LED_PIN,		OUTPUT);		// Onboard LED
	LED_OFF;

	// Setup timer 1
	// Mode 0, Clock prescaler /8 => Increment every 0.5µs, rollover every 32.768ms @ 16Mhz Clock
	TCCR1A = 0;
	TCCR1B = 2 << CS10;						// CLK/8
	TCCR1C = 0;
	OCR1A  = 0;								// Timer compare register
	TIFR1  = 0xff;							// Reset flags
	TCNT1  = 1;

	// Interrupt lockdown
	TIFR0  = 0xff;							// Reset flags
	TIMSK0 = 0;								// Turn off timer 0 interrupts ( micros() function etc )
	TIFR2  = 0xff;							// Reset flags
	TIMSK2 = 0;								// Turn off timer 2 interrupts

	interrupts();
}

/****************************/
/*****     MAIN PROG    *****/
/****************************/
void loop()
{// Main
	#define SMOKE_CH_TH 1650				// Above this value in microseconds the smoke will be requested
	static uint8_t tmr1_ovf_cnt=0;

	//Check Channel
	if(Channel0_Ok)
	{// Data received from the RX
		//Read channel 0
		cli();								// Disable interrupts to read the 16 bits value
		uint16_t value=Channel0_Value;
		Channel0_Ok=false;					// Read done
		sei();								// Enable interrupts

		//Rquest smoke based on channel 0 value
		smoke_request=value>SMOKE_CH_TH;

		//Data is being received from the RX
		LED_ON;								// Turn LED on
		tmr1_ovf_cnt=0;						// Reset Timer 1 overflow counter
		failsafe=false;						// Turn failsafe off
	}

	//Failsafe check
	if(TIFR1 & _BV(TOV1))
	{// Timer 1 has an overflow
		TIFR1 |= _BV(TOV1);					// Clear flag
		tmr1_ovf_cnt++;						// Increment Timer 1 overflow counter
		if(tmr1_ovf_cnt>3)
		{	//If no data is received from the RX after 96ms activate failsafe
			LED_TOGGLE;						// Blink LED every 96ms
			failsafe=true;					// Set failsafe
			tmr1_ovf_cnt=0;					// Reset Timer 1 overflow counter

			//Request to turn the smoke off
			smoke_request=false;
		}
	}

	Update_millisec();						// Keep track of time

	Update_Smoke();							// Turn on/off smoke based on request
} // end of loop()

//------ Generate smoke ------
void Update_Smoke()
{
	#define SMOKE_MIN_MS 600				// Must be at least 600ms for the module to accept the command
	#define SMOKE_BUTTON_RELEASE_MS 200		// Must be at least 200ms between press and release of the button
	static boolean smoke_state=false;
	static uint16_t smoke_last_update=millisec;

	//Make smoke based on smoke_request from the RX
	if(smoke_request)
	{
		if(smoke_state==false && millisec-smoke_last_update>=SMOKE_MIN_MS)	// Need to wait SMOKE_MIN_MS between each button press
		{ // Press the button to start the smoke
			DDRC |= _BV(0);					// C0 output	=0
			smoke_state=true;
			smoke_last_update=millisec;
		}
	}
	else
	{
		if(smoke_state==true && millisec-smoke_last_update>=SMOKE_MIN_MS)	// Need to wait SMOKE_MIN_MS between each button press
		{ // Press the button to stop the smoke
			DDRC |= _BV(0);					// C0 output	=0
			smoke_state=false;
			smoke_last_update=millisec;
		}
	}
	if(millisec-smoke_last_update>=SMOKE_BUTTON_RELEASE_MS)
	{ // Release the button after SMOKE_BUTTON_RELEASE_MS it has been pressed
		DDRC &= ~_BV(0);					// C0 input		=1
	}

	//Toggle the SMOKE LED with the smoke
	if(smoke_state)
		SMOKE_LED_ON;
	else
		SMOKE_LED_OFF;
}

/****************************/
/*****    Interrupts    *****/
/****************************/

//---------------------------
//-------- CHANNELS ---------
//---------------------------
void readChannel0()
{
	static uint16_t start=0;
	//Capture timer value
	uint16_t current = TCNT1;				// Capture timer1 immediately

	if(PIND & (1<<PIND2))
	{ // Rising edge of CH0
		start=current;						// Save current timer1 as the start of the pulse
	}
	else
	{ // Falling edge of CH0
		uint16_t val=(current - start)>>1;	// Pulse width
		if(val>900 && val<2100)
		{//If the pulse is valid
			Channel0_Value = val;			// Update the value
			Channel0_Ok=true;				// Inform main that a new value has been received
		}
	}
}

//------ Keep track of time ------
void Update_millisec()
{
	// Update_millisec() is not based interrupts and therefore MUST be called at least once every 32 milliseconds
	static uint16_t last=0;
	uint16_t elapsed=0;
	uint8_t millisToAdd=0;

	cli(); // Disable interrupts
	elapsed = TCNT1;
	sei(); // Enable interrupts
	elapsed -= last;

	if ( elapsed  > 31999 )
	{
		millisToAdd = 16 ;
		last+=32000;
		elapsed-=32000;
	}
	if ( elapsed  > 15999 )
	{
		millisToAdd = 8 ;
		last+=16000;
		elapsed-=16000;
	}
	if ( elapsed  > 7999 )
	{
		millisToAdd += 4 ;
		last+=8000;
		elapsed-=8000;
	}
	if ( elapsed  > 3999 )
	{
		millisToAdd += 2 ;      
		last+=4000;
		elapsed-=4000;
	}
	if ( elapsed  > 1999 )
	{
		millisToAdd += 1 ;
		last+=2000;
	}
	millisec+=millisToAdd;
}
