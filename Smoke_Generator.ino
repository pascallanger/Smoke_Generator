// ******************************************************************************
// Smoke Generator Code by Pascal Langer
// ******************************************************************************

// ***********
//  Validate
// ***********
#if not (defined(ARDUINO_AVR_NANO) or defined(ARDUINO_AVR_UNO))
  #error You must select Tools->Board->"Arduino/Genuino Nano"
#endif

// ***********
//    PINS
// ***********
#define CHANNEL_0_PIN	2	// PORT D.2 => Receiver input
#define SMOKE_PIN		A0	// PORT C.0 => Smoke output
#define SMOKE_LED_PIN	A1	// PORT C.1 => Smoke LED output
#define LED_PIN			13	// PORT B.5 => Onboard LED

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
volatile uint16_t Channel0_Start=0, Channel0_Value=0;
volatile boolean Channel0_Read=false, Channel0_Ok=false;

/****************************/
/*****       INIT       *****/
/****************************/
void setup()
{
	noInterrupts();
	//Init pins
    pinMode(CHANNEL_0_PIN,	INPUT);		// We don't want INPUT_PULLUP as the 5v may damage some receivers!
    attachInterrupt(0, readChannel0, CHANGE);

	pinMode(SMOKE_PIN,		INPUT);		// We want to emulate the button so INPUT when not pressed and ouput 0 when pressed
	digitalWrite(SMOKE_PIN,	LOW);

	pinMode(SMOKE_LED_PIN,	OUTPUT);	// Smoke LED
	SMOKE_LED_OFF;

	pinMode(LED_PIN,		OUTPUT);	// Onboard LED
	LED_OFF;

	// Setup timer 1
	// Mode 0, Clock prescaler /8 => Increment every 0.5µs, rollover every 32.768ms @ 16Mhz Clock
	TCCR1A = 0;
	TCCR1B = 2 << CS10;						// CLK/8. 
	TCCR1C = 0;
	OCR1A  = 0;								// Timer compare register.
	TIFR1  = 0xff;							// Reset flags.
	TCNT1  = 1;

	// Interrupt lockdown.
	TIFR0  = 0xff;	// Reset flags.
	TIMSK0 = 0;		// Turn off timer 0 interrupts ( micros() function etc ).
	TIFR2  = 0xff;	// Reset flags.
	TIMSK2 = 0;		// Turn off timer 2 interrupts.

	interrupts();
}

/****************************/
/*****     MAIN PROG    *****/
/****************************/
void loop()
{
	static uint8_t tmr1_ovf_cnt=0;

	//Check Channel
	if(Channel0_Ok)
	{
		//Data is being received from the RX
		LED_ON;
		tmr1_ovf_cnt=0;
		failsafe=false;

		//Read channel 0
		Channel0_Read=true;
		uint16_t value=Channel0_Value;
		Channel0_Ok=false;
		Channel0_Read=false;

		//Act based on channel 0 value
		smoke_request=value>1650;
	}

	//Failsafe check
	if(TIFR1 & _BV(TOV1))
	{
		TIFR1 |= _BV(TOV1);		// clear flag
		tmr1_ovf_cnt++;
		if(tmr1_ovf_cnt>3)
		{	//If no data is received from the RX after 96ms activate failsafe
			LED_TOGGLE;			// blink LED every 96ms
			failsafe=true;
			tmr1_ovf_cnt=0;

			//Turn off
			smoke_request=false;
		}
	}

	Update_millisec();

	Update_Smoke();
} // end of loop()

//------ Generate smoke ------
void Update_Smoke()
{
	static boolean smoke_state=false;
	static uint16_t smoke_last_update=millisec,min_smoke=600;

	if(smoke_request)
	{
		SMOKE_LED_ON;
		if(smoke_state==false)
		{ // Press button
			DDRC |= _BV(0);	// C0 output	=0
			smoke_state=true;
			smoke_last_update=millisec;
		}
	}
	else
	{
		SMOKE_LED_OFF;
		if(smoke_state==true && millisec-smoke_last_update>=min_smoke)
		{ // Press button
			DDRC |= _BV(0);	// C0 output	=0
			smoke_state=false;
			smoke_last_update=millisec;
			min_smoke=600;
		}
	}
	if(millisec-smoke_last_update>=200)
	{ // Release button
		DDRC &= ~_BV(0);	// C0 input		=1
	}
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
	uint16_t current = TCNT1;

	if(PIND & (1<<PIND2))
	{ // Rising edge of CH0
		start=current;
	}
	else
	{ // Falling edge of CH0
		if(!Channel0_Read)	// Do not update if main is reading the value
		{
			uint16_t val=(current - start)>>1;
			if(val>900 && val<2100)
			{
				Channel0_Value = (current - start)>>1;
				Channel0_Ok=true;
			}
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

	cli(); // Need to be atomic.
	elapsed = TCNT1;
	sei();
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
