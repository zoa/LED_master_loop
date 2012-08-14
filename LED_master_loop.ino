#include "SPI.h"
#include "Zoa_WS2801.h"
#include "Sine_generator.h"
#include "MsTimer2.h"
#include "Waveform_utilities.h"
#include "Audio_monitor.h"



//////// Globals //////////

#define dataPin 2  // Yellow wire on Adafruit Pixels
#define clockPin 3   // Green wire on Adafruit Pixels
#define stripLen 40

const byte update_frequency = 30; // how often to update the LEDs
volatile unsigned long int interrupt_counter; // updates every time the interrupt timer overflows
unsigned long int prev_interrupt_counter; // the main loop uses this to detect when the interrupt counter has changed 

unsigned int switch_after; // swap routines after this many milliseconds
unsigned int active_routine; // matches the #s from the switch statement in the main loop
void (*update)(); // pointer to current led-updating function within this sketch

// pointer to a function in the Zoa_WS2801 library that takes a color argument. The update functions in this sketch use this
// pointer to decide whether to call pushBack, pushFront or setAll.
void (Zoa_WS2801::* library_update)(rgbInfo_t); 

// Set the first variable to the NUMBER of pixels. 25 = 25 pixels in a row
Zoa_WS2801 strip = Zoa_WS2801(stripLen, dataPin, clockPin, WS2801_GRB);

// Pointers to some waveform objects - currently they're reallocated each time the routine changes
#define WAVES 6
Waveform_generator* waves[WAVES]={};

const Audio_monitor& audio = Audio_monitor::instance();

boolean transitioning = false;


boolean hiding = false;
const byte hiding_slowdown_factor = 8;




void allocate_simple_sines( boolean back_from_hiding=false )
{
  update = update_simple;
  waves[0] = new Sine_generator( 0, 15, 1 + back_from_hiding*hiding_slowdown_factor, PI/2 );
  // all the /3s are a quick way to get the speed looking right while maintaining prime number ratios
  waves[1] = new Sine_generator( 20, 255, 11/3 + back_from_hiding*hiding_slowdown_factor, 0 );
  waves[2] = new Sine_generator( 20, 255, 17/3 + back_from_hiding*hiding_slowdown_factor, 0 );
}

//////// Setup //////////

void setup()
{
  Serial.begin(9600);
  strip.begin();
  strip.setAll(rgbInfo_t(0,0,0));
  
  switch_after = 5000;
  interrupt_counter = switch_after + 1;
  prev_interrupt_counter = interrupt_counter;
  active_routine = 0;
  update = NULL;
  library_update = NULL;
  
  // update the interrupt counter (and thus the LEDs) every 30ms. The strip updating takes ~0.1ms 
  // for each LED in the strip, and we are assuming a maximum strip length of 240, plus some extra wiggle room.
  MsTimer2::set( update_frequency, &update_interrupt_counter );
  MsTimer2::start();
}



//////// Main loop //////////

void loop()
{  
  if ( audio.get_amplitude_float() > 0.95 )
  {
    hide_in_ground();
  }
  if ( interrupt_counter > switch_after )
  {
    //byte i = random(0,4);
    byte i = (active_routine+1) % 4;
    if ( i != active_routine )
    {
      deallocate_waveforms();
      
      // Decide how to show the current pattern (chasing up, chasing down, changing the whole strip at once )
      byte update_func = random(0,2);
      switch ( update_func )
      {
        case 1:
          library_update = &Zoa_WS2801::pushBack;
          break;
        case 2:
          library_update = &Zoa_WS2801::setAll;
          break;
        default:
          library_update = &Zoa_WS2801::pushFront;
      }
      
      // Decide which routine to show next
      switch (i)
      {
        case 0:
          // green and blue waves going in and out of phase
          update = update_simple;
          waves[0] = new Sine_generator( 0, 15, 1, PI/2 );
           // all the /3s are a quick way to get the speed looking right while maintaining prime number ratios
          waves[1] = new Sine_generator( 20, 255, 11/3, 0 );
          waves[2] = new Sine_generator( 20, 255, 17/3, 0 );
          break;
        case 1:
          // green and purple waves, same frequency but out of phase
          update = update_simple;
          waves[0] = new Sine_generator( 0, 5, 5/3, 0 );
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
          waves[0] = new Linear_generator( Linear_generator::TRIANGLE, 0, 30, 1, 75 );
          waves[1] = new Sine_generator( 30, 100, 1, 0 );
          waves[2] = new Linear_generator( Linear_generator::TRIANGLE, 0, 100, 1, 128 );
          break;
          /*
        case 4:
          // sets whole strip at once to blink between 2 different morphing purple-blues
          // blinking is bad
          update = update_all;
          waves[0] = new Sine_generator( 0, 100, 2, 0 );
          waves[1] = new Square_generator( 100, 200, 1, 100, 100, 1 );
          waves[2] = new Square_generator( 100, 255, 1, 100, 100, 1 );
          */
      }
      active_routine = i;
      interrupt_counter = 0;
      linear_transition();
    }
  }
  // only update once every tick of the timer
  if ( interrupt_counter != prev_interrupt_counter )
  {
    prev_interrupt_counter = interrupt_counter;
    update_audio();
    update();
  }
}




//////// LED display routines //////////

void update_simple()
{
  (strip.*library_update)( get_next_rgb( waves[0], waves[1], waves[2] ) );
  if ( !transitioning )
  {
    strip.show();
  }
}


void update_convolved()
{
  (strip.*library_update)( rgbInfo( next_convolved_value(waves[0],waves[3]), next_convolved_value(waves[1],waves[4]), next_convolved_value(waves[2],waves[5]) ) );
  if ( !transitioning )
  {
    strip.show();
  }
}

void hide_in_ground()
{
  for ( int i = stripLen - 1; i >= 0; --i )
  {
    strip.pushFront( rgbInfo_t(0,0,0) );
    strip.show();
    delay(5); // if we use the interrupt timer for this it'll be too slow
  }
  delay(2000);
  deallocate_waveforms();
  allocate_simple_sines();
  library_update = &Zoa_WS2801::pushBack;
  MsTimer2::msecs = MsTimer2::msecs * hiding_slowdown_factor;
  linear_transition();
  for ( int i = 0; i < stripLen; ++i )
  {
    while( prev_interrupt_counter == interrupt_counter ) {}
    update();
    prev_interrupt_counter = interrupt_counter;
  }
  MsTimer2::msecs = update_frequency;
  interrupt_counter = switch_after + 1;
}



//////// Transition functions //////////

void linear_transition()
{  
  transitioning = true;
  uint16_t pixel = (library_update == &Zoa_WS2801::pushBack) ? stripLen-1 : 0;
  // this is a total hack to get the first value of the next routine without actually displaying it (or having to change the update functions).
  // cache the current first value, update, grab the new first value, then reset the first pixel.
  // this will fall apart if the update routine updates all the pixels and not just the first one!!! check the transitioning flag in all
  // update functions to keep this from happening.
  rgbInfo_t temp_first_value = strip.getPixelRGBColor(pixel);
  update();
  rgbInfo_t next_value = strip.getPixelRGBColor(pixel);
  strip.setPixelColor( pixel, temp_first_value.r, temp_first_value.g, temp_first_value.b );
  transitioning = false;
  
  linear_transition(temp_first_value,next_value,500);
}

void linear_transition( const rgbInfo& start_value, const rgbInfo& target_value, uint16_t ms )
{
  float stop_cnt = interrupt_counter + ms;
  while ( interrupt_counter < stop_cnt )
  {
    while ( interrupt_counter == prev_interrupt_counter ) {}
    prev_interrupt_counter = interrupt_counter;
    
    float multiplier = 1 - (stop_cnt - interrupt_counter)/ms;
    rgbInfo_t c( 
     transitional_value( start_value.r, target_value.r, multiplier ),
     transitional_value( start_value.g, target_value.g, multiplier ),
     transitional_value( start_value.b, target_value.b, multiplier )
     );    
    (strip.*library_update)(c);
    strip.show();
  }
}


//////// Utility functions //////////

// Called by the interrupt timer
void update_interrupt_counter()
{
  interrupt_counter += MsTimer2::msecs;
  audio.update_amplitude();
}

// Used for transitions between routines (hypothetically)
float transitional_value( const float& from, const float& to, float multiplier )
{
  if ( multiplier < 0 )
  {
    multiplier = 0;
  }
  else if ( multiplier > 1 )
  {
    multiplier = 1;
  }
  float val = from * (1-multiplier) + to * multiplier;
  return val;
}

// free the memory in the waves array and sets the update modes to 0
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


void update_audio()
{
  float level = audio.get_amplitude_float();
  Serial.println(level);
  for ( byte i = 0; i < WAVES; ++i )
  {
    if ( waves[i] != NULL )
    {
      waves[i]->set_audio_level( level );
    }
  }
}
