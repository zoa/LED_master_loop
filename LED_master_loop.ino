#include "SPI.h"
#include "Zoa_WS2801.h"
#include "Sine_generator.h"
#include "MsTimer2.h"
#include "Waveform_utilities.h"
//#include "Audio_monitor.h"



#define dataPin 2  // Yellow wire on Adafruit Pixels
#define clockPin 3   // Green wire on Adafruit Pixels
#define stripLen 200

volatile unsigned long int interrupt_counter;
unsigned int switch_after;
unsigned int active_routine;
void (*update)(); // pointer to current led-updating function

// Set the first variable to the NUMBER of pixels. 25 = 25 pixels in a row
Zoa_WS2801 strip = Zoa_WS2801(stripLen, dataPin, clockPin, WS2801_GRB);

#define WAVES 6
Waveform_generator* waves[WAVES]={};

void setup()
{
  Serial.begin(9600);
  strip.begin();
  
  switch_after = 5000;
  interrupt_counter = switch_after + 1;
  active_routine = 0;
  update = NULL;
  
  MsTimer2::set( 1, &update_interrupt_counter );
  MsTimer2::start();
}


void update_interrupt_counter()
{
  interrupt_counter += MsTimer2::msecs;
}

void reset_interrupt( unsigned int ms, void (*f)() )
{
  MsTimer2::msecs = ms;
  MsTimer2::func = f;
}


void loop()
{  
  if ( interrupt_counter > switch_after )
  {
    byte i = random(0,3);
    if ( i != active_routine )
    {
      deallocate_waveforms();
      switch (i)
      {
        case 0:
          update = update_simple;
          waves[0] = new Empty_waveform();
          waves[1] = new Sine_generator( 0, 255, 7/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 13/3, 0 );
          break;
        case 1:
          update = update_simple;
          waves[0] = new Sine_generator( 0, 4, 5/3, PI/2 );
          waves[1] = new Sine_generator( 0, 200, 5/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 5/3, PI/2 );  
          break;
        case 2:
          update = update_simple;
          waves[0] = new Sine_generator( 0, 10, 1, 0 );
          waves[1] = new Sine_generator( 0, 200, 13/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 7/3, 0 );
          break;
        case 3:
          update = update_convolved;
          //waves[0] = new Empty_waveform(), 
          waves[0] = new Sine_generator( 0, 10, 7, 0 );
          waves[1] = new Sine_generator( 30, 150, 7/3, 0 );
          waves[2] = new Sine_generator( 30, 255, 13/3, PI/2 );
          //waves[3] = new Empty_waveform();
          waves[3] = new Sine_generator( 0, 10, 7, PI/4 );
          waves[4] = new Sine_generator( 30, 150, 7/12, 0 );
          waves[5] = new Sine_generator( 30, 250, 13/12, PI/2 );
          break;
      }
      active_routine = i;
    }
    interrupt_counter = 0;
  }
  update();
}


void deallocate_waveforms()
{
  for ( byte i = 0; i < WAVES; ++i )
  {
    if ( waves[i] != NULL )
    {
      delete waves[i];
      waves[i] = NULL;
    }
  }
}

void update_simple()
{
  strip.pushFront( get_next_rgb( waves[0], waves[1], waves[2] ) );
  strip.show();
  interrupt_counter += MsTimer2::msecs;
}


void update_convolved()
{
  strip.pushFront( rgbInfo( next_convolved_value(waves[0],waves[3]), next_convolved_value(waves[1],waves[4]), next_convolved_value(waves[2],waves[5]) ) );
  strip.show();
  interrupt_counter += MsTimer2::msecs;    
}
