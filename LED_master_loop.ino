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
unsigned long int prev_interrupt_counter; // the main loop uses this to detect when the interrupt counter has changed 
unsigned int switch_after; // swap routines after this many milliseconds
unsigned int active_routine; // matches the #s from the switch statement in the main loop
void (*update)(); // pointer to current led-updating function

// Set the first variable to the NUMBER of pixels. 25 = 25 pixels in a row
Zoa_WS2801 strip = Zoa_WS2801(stripLen, dataPin, clockPin, WS2801_GRB);

// Pointers to some waveform objects - currently they're reallocated each time the routine changes
#define WAVES 6
Waveform_generator* waves[WAVES]={};

boolean transitioning = false;


//////// Setup //////////

void setup()
{
  Serial.begin(9600);
  strip.begin();
  strip.setAll(rgbInfo_t(0,0,0));
  
  switch_after = 10000;
  interrupt_counter = switch_after + 1;
  prev_interrupt_counter = interrupt_counter;
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
    //byte i = random(0,4);
    byte i = (active_routine+1) % 4;
    if ( i != active_routine )
    {
      deallocate_waveforms();
      switch (i)
      {
        case 0:
          // green and blue waves going in and out of phase
          update = update_simple;
          waves[0] = new Sine_generator( 0, 15, 1, 0 );
           // all the /3s are a quick way to get the speed looking right while maintaining prime number ratios
          waves[1] = new Sine_generator( 20, 255, 11/3, 0 );
          waves[2] = new Sine_generator( 20, 255, 17/3, 0 );
          break;
        case 1:
          // green and purple waves, same frequency but out of phase
          update = update_simple;
          waves[0] = new Sine_generator( 0, 5, 5/3, PI/2 );
          waves[1] = new Sine_generator( 0, 200, 5/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 5/3, PI/2 );  
          break;
          /*
        case 2:
          // similar to 0 but with a bit more blue and some red
          update = update_simple;
          waves[0] = new Sine_generator( 0, 15, 1, 0 ); // red appears way brighter than G/B so using tiny #s is good
          waves[1] = new Sine_generator( 0, 200, 13/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 7/3, 0 );
          break;*/
        case 2:
          // two waves multiplied together
          update = update_convolved; 
          waves[0] = new Sine_generator( 0, 100, 7, PI/2 );
          waves[1] = new Sine_generator( 30, 150, 7/3, 0 );
          waves[2] = new Sine_generator( 30, 255, 13/3, PI/2 );
          waves[3] = new Sine_generator( 0, 100, 7, PI/4 );
          waves[4] = new Sine_generator( 30, 150, 7/12, 0 );
          waves[5] = new Sine_generator( 30, 250, 13/12, PI/2 );
          break;
        case 3:
          // mostly light blue/turquoise/purple with occasional bright green
          update = update_simple;
          waves[0] = new Linear_generator( Linear_generator::TRIANGLE, 0, 30, 75 );
          waves[1] = new Sine_generator( 30, 100, 1, 0 );
          waves[2] = new Linear_generator( Linear_generator::TRIANGLE, 0, 100, 128 );
          break;
      }
      active_routine = i;
      interrupt_counter = 0;
      //linear_transition();
    }
  }
  // only update once every tick of the timer
  if ( interrupt_counter != prev_interrupt_counter )
  {
    prev_interrupt_counter = interrupt_counter;
    update();
  }
}




//////// LED display routines //////////

void update_simple()
{
  strip.pushFront( get_next_rgb( waves[0], waves[1], waves[2] ) );
  if ( !transitioning )
  {
    strip.show();
  }
}


void update_convolved()
{
  strip.pushFront( rgbInfo( next_convolved_value(waves[0],waves[3]), next_convolved_value(waves[1],waves[4]), next_convolved_value(waves[2],waves[5]) ) );
  if ( !transitioning )
  {
    strip.show();
  }
}


//////// Transition functions //////////

// This is messy and broken right now - needs to be debugged later
void linear_transition()
{  
  transitioning = true;
  // this is a total hack to get the first value of the next routine without actually displaying it (or having to change the update functions).
  // cache the current first value, update, grab the new first value, then reset the first pixel.
  // this will fall apart if the update routine updates all the pixels and not just the first one!!! check the transitioning flag in all
  // update functions to keep this from happening.
  rgbInfo_t temp_first_value= strip.getPixelRGBColor(0);
  update();
  rgbInfo_t next_value = strip.getPixelRGBColor(0);
  //strip.setPixelColor( 0, temp_first_value.r, temp_first_value.g, temp_first_value.b );
  transitioning = false;
/*
  linear_transition( rgbInfo_t(), rgbInfo_t(20,180,130), 200 );
  linear_transition( rgbInfo_t(20,180,130), rgbInfo_t(), 200 );*/
//linear_transition( rgbInfo_t(0,0,255), rgbInfo_t(0,255,255), 1000);
linear_transition(temp_first_value,next_value,500);
}

void linear_transition( const rgbInfo& start_value, const rgbInfo& target_value, uint16_t ms )
{
  float stop_cnt = interrupt_counter + ms;
  while ( interrupt_counter <= stop_cnt )
  {
    while ( interrupt_counter == prev_interrupt_counter ) {}
    prev_interrupt_counter = interrupt_counter;
    
    float multiplier = 1 - (stop_cnt - interrupt_counter)/ms;
    rgbInfo_t c( 
     transitional_value( start_value.r, target_value.r, multiplier ),
     transitional_value( start_value.g, target_value.g, multiplier ),
     transitional_value( start_value.b, target_value.b, multiplier )
     );    
    strip.pushFront(c);
    Serial.println(c.r);
    strip.show();
  }
}


//////// Utility functions //////////

// Called by the interrupt timer
void update_interrupt_counter()
{
  interrupt_counter += MsTimer2::msecs;
}

// Used for transitions between routines (hypothetically)
float transitional_value( const float& from, const float& to, const float& multiplier )
{
  float val = from * (1-multiplier) + to * multiplier;
  return val;
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
