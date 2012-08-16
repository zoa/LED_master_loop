#include "SPI.h"
#include "Zoa_WS2801.h"
#include "Sine_generator.h"
#include "MsTimer2.h"
#include "Waveform_utilities.h"
#include "Audio_monitor.h"



//////// Globals //////////

#define dataPin 2  // Yellow wire on Adafruit Pixels
#define clockPin 3   // Green wire on Adafruit Pixels
#define stripLen 80

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
  
  switch_after = 200000;
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
    byte i = (active_routine+1) % 7;
    if ( i != active_routine )
    {
      deallocate_waveforms();
      
      // Decide which routine to show next
      switch (i)
      {
        case 0:
          // green and blue waves going in and out of phase
          update = update_simple;
          set_library_update(true);
          waves[0] = new Sine_generator( 0, 8, 1, PI/2 );
           // all the /3s are a quick way to get the speed looking right while maintaining prime number ratios
          waves[1] = new Sine_generator( 0, 255, 11/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 17/3, 0 );
          break;
        case 1:
          // green and purple waves, same frequency but out of phase
          update = update_simple;
          set_library_update(true);
          waves[0] = new Sine_generator( 0, 5, 5/3, 0 );
          waves[1] = new Sine_generator( 0, 200, 5/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 5/3, PI/2 );  
          break;
        case 2:
          // two waves multiplied together
          update = update_convolved; 
          set_library_update(true);
          waves[0] = new Sine_generator( 0, 100, 7, PI/2 );
          waves[1] = new Sine_generator( 30, 255, 11/3, PI/2 );
          waves[2] = new Sine_generator( 30, 150, 7/3, 0 );
          waves[3] = new Sine_generator( 0, 100, 7, PI/4 );
          waves[4] = new Sine_generator( 30, 250, 11/12, PI/2 );
          waves[5] = new Sine_generator( 30, 150, 7/12, 0 );
          break;
        case 3:
          // mostly light blue/turquoise/purple with occasional bright green
          update = update_simple;
          set_library_update(true);
          waves[0] = new Linear_generator( Linear_generator::SAWTOOTH, 0, 30, 1, 75 );
          waves[1] = new Sine_generator( 0, 30, 1, 0 );
          waves[2] = new Linear_generator( Linear_generator::TRIANGLE, 0, 255, 5, 128 );
          break;
        case 4:
          // purple-blue with bright blue twinkles
          // this could be a startle routine later
          update = update_simple;
          set_library_update(false); // this one looks weird if it updates the whole thing at once
          waves[0] = new Sine_generator( 0, 8, 7/3, PI/2 );
          waves[1] = new Sine_generator( 0, 10, 7/3, 0 );
          waves[2] = new White_noise_generator( 230, 255, 20, 120, 20, 5 );
          break;
        case 5:
          // blue with pink-yellow bits and occasional white twinkles
          update = update_twinkle_white;
          set_library_update(false); // has to be a chase
          waves[0] = new Sine_generator( 5, 15, 5, PI/2 );
          waves[1] = new Linear_generator( Linear_generator::TRIANGLE, 0, 30, 1, 30 ); //Sine_generator( 0, 30, PI/2 );//Empty_waveform();//Sine_generator( 0, 255, 5/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 5, 0 );
          waves[3] = new White_noise_generator( 255, 255, 20, 150, 0, 2 );  
          break;
        case 6:/*
        // alternating r/g/b bands - this is for testing
          update = update_simple;
          waves[0] = new Square_generator( 0, 255, 1, 20, 40, 20 );
          waves[1] = new Square_generator( 0, 255, 1, 20, 40, 40 );
          waves[2] = new Square_generator( 0, 255, 1, 20, 40, 0 );
          */
          // dim sine waves with occasional flares of bright colors - could be adapted into a startle routine
          update = update_scaled_sum;
          set_library_update(false); // has to be a chase
          waves[0] = new Sine_generator( 0, 5, 7/2, PI/2 );
          waves[1] = new Sine_generator( 0, 10, 7/2, 0 );
          waves[2] = new Sine_generator( 0, 10, 13/2, 0 );
          waves[3] = new Linear_generator( Linear_generator::TRIANGLE, 0, 255, 100, 0, 31 );
          
          break;
      }
      active_routine = i;
      interrupt_counter = 0;
      linear_transition(500);
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


// just show the first 3 waves in the R, G and B channels
void update_simple()
{
  (strip.*library_update)( get_next_rgb( waves[0], waves[1], waves[2] ) );
  if ( !transitioning )
  {
    strip.show();
  }
}

// multiply waves[0:2] by waves[3:5]
void update_convolved()
{
  (strip.*library_update)( rgbInfo_t( next_convolved_value(waves[0],waves[3]), next_convolved_value(waves[1],waves[4]), next_convolved_value(waves[2],waves[5]) ) );
  if ( !transitioning )
  {
    strip.show();
  }
}

// simply sum the first 3 and next 3 waves (can't remember if this is tested yet)
void update_summed()
{
  (strip.*library_update)( rgbInfo_t( next_summed_value(waves[0],waves[3]), next_summed_value(waves[1],waves[4]), next_summed_value(waves[2],waves[5]) ) );
  if ( !transitioning )
  {
    strip.show();
  }
}

// add the 4th wave to the first 3 waves, making sure the library_update function is set to pushBack. Used to
// superimpose white twinkles.
void update_twinkle_white()
{
  // it's a bit seizure-inducing if you make the whole thing flash white at once
  if ( library_update != &Zoa_WS2801::pushBack )
  {
    library_update = &Zoa_WS2801::pushBack;
  }
  // advance the first three (the base waves) plus the fourth (the white noise)
  for ( byte i = 0; i < 4; ++i )
  {
    waves[i]->next_value();
  }
  // add the twinkles to all 3 base waves
  (strip.*library_update)( rgbInfo_t( summed_value(waves[0], waves[3]), summed_value(waves[1],waves[3]), summed_value(waves[2],waves[3]) ) );
  if ( !transitioning )
  {
    strip.show();
  }
}

// NOT TESTED
void update_greyscale()
{
  (strip.*library_update)( next_greyscale_value( waves[0], waves[1], waves[2] ) );
  if ( !transitioning )
  {
    strip.show();
  }
}

// add waves[3] to waves[0:2], increasing the brightnesses of all 3 waves proportionally
void update_scaled_sum()
{
  (strip.*library_update)( rgb_scaled_summed_value( waves[0], waves[1], waves[2], waves[3]->next_raw_value() ) );
  if ( !transitioning )
  {
    strip.show();
  }
}



/// Startle routines

// rapidly twinkle white on top of some sines with more red than usual, then slowly stop twinkling and eventually shift
// back to simple sines
void spastic_flicker()
{
  update = update_twinkle_white;
  set_library_update(false);
  White_noise_generator* twinkles = new White_noise_generator( 255, 255, 5, 8, 0 ); 
  waves[0] = new Sine_generator( 0, 15, 7, 0 );
  waves[1] = new Empty_waveform();//Sine_generator( 5, 20, 11 );//Empty_waveform();//Sine_generator( 0, 255, 5/3, 0 );
  waves[2] = new Sine_generator( 5, 20, 11, PI/2 ); //Empty_waveform();//Sine_generator( 0, 255, 5, 0 );
  waves[3] = twinkles; 
  
  // ideally all these numbers shouldn't be hard-coded, but in the meantime, i've set it up so that it'll only throw
  // the overall timing off if the switch interval is less than 30 seconds which it never will be on the real sculpture.
  unsigned long int stop_time = interrupt_counter + 25000;
  unsigned long int slow_time = interrupt_counter + 3000;
  uint16_t steps = 0;
  while ( interrupt_counter < stop_time )
  {
    update();
    if ( interrupt_counter > slow_time && steps%8 == 0 )
    {
      twinkles->increase_spacing(1);
    }
    pause_for_interrupt();
    ++steps;
  }
  update = update_simple;
  deallocate_waveforms();
  allocate_simple_sines();
  linear_transition(2000);
}

// rapidly go black from top to bottom, then pause, then come back out at a reduced speed
void hide_in_ground()
{
  for ( int i = stripLen - 1; i >= 0; --i )
  {
    strip.pushFront( rgbInfo_t(0,0,0) );
    strip.show();
    delay(5); // if we use the interrupt timer for this it'll be too slow
  }
  delay(2000);
  
  // it'll always do the same thing when it first comes back [for now]
  deallocate_waveforms();
  allocate_simple_sines();
  library_update = &Zoa_WS2801::pushBack;
  
  // come slowly back up
  MsTimer2::msecs *= hiding_slowdown_factor;
  linear_transition(750);
  for ( int i = 0; i < stripLen*2; ++i )
  {
    pause_for_interrupt();
    update();
  }
  
  // revert to initial speed
  MsTimer2::msecs = update_frequency;
}

// NOT DONE
void spike_intensities()
{
  Linear_generator triangle( Linear_generator::TRIANGLE, 0, 255, 5 );
  
}


//////// Transition functions //////////

void linear_transition(uint16_t duration)
{  
  transitioning = true;
  // this is a total hack to get the first value of the next routine without actually displaying it (or having to change the update functions).
  // cache the current first value, update, grab the new first value, then reset the first pixel.
  // this will fall apart if the update routine updates all the pixels and not just the first one!!! check the transitioning flag in all
  // update functions to keep this from happening.
  boolean was_set_all = library_update == &Zoa_WS2801::setAll;
  if ( was_set_all )
  {
    library_update = &Zoa_WS2801::pushBack;
  }
  uint16_t pixel = (library_update == &Zoa_WS2801::pushBack) ? stripLen-1 : 0;
  rgbInfo_t temp_first_value = strip.getPixelRGBColor(pixel);
  update();
  rgbInfo_t next_value = strip.getPixelRGBColor(pixel);
  strip.setPixelColor( pixel, temp_first_value.r, temp_first_value.g, temp_first_value.b );
  if ( was_set_all )
  {
    library_update = &Zoa_WS2801::setAll;
  }
  transitioning = false;
  
  linear_transition(temp_first_value,next_value,(float)duration/update_frequency);
}

void linear_transition( const rgbInfo& start_value, const rgbInfo& target_value, byte steps )
{
  for ( byte i = 0; i < steps; ++i )
  {    
    float multiplier = (float)i/steps;
    rgbInfo_t c( 
     interpolated_value( start_value.r, target_value.r, multiplier ),
     interpolated_value( start_value.g, target_value.g, multiplier ),
     interpolated_value( start_value.b, target_value.b, multiplier )
     );    
    (strip.*library_update)(c);
    strip.show();
    pause_for_interrupt();
  }
}


//////// Utility functions //////////

// allow_set_all option is currently disabled pending making the transition not look choppy
void set_library_update( boolean allow_set_all )
{
  // Decide how to show the current pattern (chasing up, chasing down, changing the whole strip at once )
  byte update_func = random(0,2);//random(0,2+allow_set_all);
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
}

// Called by the interrupt timer
void update_interrupt_counter()
{
  interrupt_counter += MsTimer2::msecs;
  audio.update_amplitude();
}

// Returns after the next interrupt
void pause_for_interrupt()
{
  while ( interrupt_counter == prev_interrupt_counter ) {}
  prev_interrupt_counter = interrupt_counter;
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

// Pass the latest audio value to the waveforms.
void update_audio()
{
  float level = audio.get_amplitude_float();
  for ( byte i = 0; i < WAVES; ++i )
  {
    if ( waves[i] != NULL )
    {
      waves[i]->set_audio_level( level );
    }
  }
}
