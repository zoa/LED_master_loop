#include "SPI.h"
#include "Zoa_WS2801.h"
#include "Sine_generator.h"
#include "MsTimer2.h"
#include "Waveform_utilities.h"
#include "Audio_monitor.h"



//////// Globals //////////

#define dataPin 2  // Yellow wire on Adafruit Pixels
#define clockPin 3   // Green wire on Adafruit Pixels
#define stripLen 200

volatile unsigned long int interrupt_counter; // how many ms have elapsed since the interrupt timer was last reset
unsigned int switch_after; // swap routines after this many milliseconds
unsigned int active_routine; // matches the #s from the switch statement in the main loop
void (*update)(); // pointer to current led-updating function

// Set the first variable to the NUMBER of pixels. 25 = 25 pixels in a row
Zoa_WS2801 strip = Zoa_WS2801(stripLen, dataPin, clockPin, WS2801_GRB);

// Pointers to some waveform objects - currently they're reallocated each time the routine changes
#define WAVES 6
Waveform_generator* waves[WAVES]={};



//////// Setup //////////

void setup()
{
  Serial.begin(9600);
  strip.begin();
  
  switch_after = 10000;
  interrupt_counter = switch_after + 1;
  active_routine = 0;
  update = NULL;
  
  // update the counter every millisecond
  MsTimer2::set( 1, &update_interrupt_counter );
  MsTimer2::start();
}



//////// Main loop //////////

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
          // green and blue waves going in and out of phase
          update = update_simple;
          waves[0] = new Empty_waveform();
           // all the /3s are a quick way to get the speed looking right while maintaining prime number ratios
          waves[1] = new Sine_generator( 20, 255, 7/3, 0 );
          waves[2] = new Sine_generator( 20, 255, 13/3, 0 );
          break;
        case 1:
          // green and purple waves, same frequency but out of phase
          update = update_simple;
          waves[0] = new Sine_generator( 0, 4, 5/3, PI/2 );
          waves[1] = new Sine_generator( 0, 200, 5/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 5/3, PI/2 );  
          break;
        case 2:
          // similar to 0 but with a bit more blue and some red
          update = update_simple;
          waves[0] = new Sine_generator( 0, 10, 1, 0 ); // red appears way brighter than G/B so using tiny #s like 10 is good
          waves[1] = new Sine_generator( 0, 200, 13/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 7/3, 0 );
          break;
        case 3:
          // two waves multiplied together
          update = update_convolved; 
          waves[0] = new Sine_generator( 0, 10, 7, 0 );
          waves[1] = new Sine_generator( 30, 150, 7/3, 0 );
          waves[2] = new Sine_generator( 30, 255, 13/3, PI/2 );
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




//////// LED display routines //////////

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



//////// Utility functions //////////

// Called by the interrupt timer
void update_interrupt_counter()
{
  interrupt_counter += MsTimer2::msecs;
}

// free the memory in the waves array
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
