#include <bitswap.h>
#include <chipsets.h>
#include <color.h>
#include <colorpalettes.h>
#include <colorutils.h>
#include <controller.h>
#include <cpp_compat.h>
#include <dmx.h>
#include <FastLED.h>
#include <fastled_config.h>
#include <fastled_delay.h>
#include <fastled_progmem.h>
#include <fastpin.h>
#include <fastspi.h>
#include <fastspi_bitbang.h>
#include <fastspi_dma.h>
#include <fastspi_nop.h>
#include <fastspi_ref.h>
#include <fastspi_types.h>
#include <hsv2rgb.h>
#include <led_sysdefs.h>
#include <lib8tion.h>
#include <noise.h>
#include <pixelset.h>
#include <pixeltypes.h>
#include <platforms.h>
#include <power_mgt.h>

// LED type config
#define LED_TYPE WS2812B
#define LED_PIN 5
#define COLOR_ORDER GRB

// Maximum allowed LEDs
#define MAX_NUM_LEDS 300

// Default brightness (0 - 255)
#define BRIGHTNESS  64

// How fast to update the LEDs
#define UPDATES_PER_SECOND 100

// Current and target LED values
CRGB CurrentLEDValues[MAX_NUM_LEDS];

// Number of LEDs running
int LEDCount = 0;

// The current LED being indexed
int CurrentReceivingLED = 0;

// For tracking our serial input
enum ECurrentSerialInputMode
{
  None,
  Config,
  LightValues
};
ECurrentSerialInputMode CurrentSerialInputMode;
uint8_t SerialBuffer[16];
uint8_t SerialBufferIndex;

// Setup function
void setup()
{
  // Start the serial port
  CurrentSerialInputMode = None;
  Serial.begin(115200, SERIAL_8E1);

  fill_solid(CurrentLEDValues, MAX_NUM_LEDS, CRGB::Black);

  // Power-up safety delay
  delay(3000);

  // Initialise FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(CurrentLEDValues, MAX_NUM_LEDS);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show();
}


void loop()
{
  bool lightsUpdated = false;
  while (Serial.available() > 0)
  {
    int incomingByte = Serial.read();
    switch (CurrentSerialInputMode)
    {
      case None:
        {
          SerialBufferIndex = 0;
          switch (incomingByte)
          {
            case 'C':
              {
                // Switch to config mode
                CurrentSerialInputMode = Config;
              }
              break;

            case 'L':
              {
                // Switch to light value mode
                CurrentReceivingLED = 0;
                CurrentSerialInputMode = LightValues;
              }
              break;

            // Ignore new line characters
            case '\r':
            case '\n':
              break;

            default:
              break;
          }
        }
        break;

      case Config:
        {
          // Append the config value to the buffer
          SerialBuffer[SerialBufferIndex++] = incomingByte;
          if (SerialBufferIndex == 2)
          {
            // We've read the config (which is W:H)
            LEDCount = SerialBuffer[0] * SerialBuffer[1];

            //Serial.println("Config received: " + String(SerialBuffer[0]) + "x" + String(SerialBuffer[1]) + " - NumLEDs: " + String(LEDCount));

            CurrentSerialInputMode = None;
          }
        }
        break;

      case LightValues:
        {
          bool moreAvailable = true;
          while(moreAvailable)
          {
            // Buffer until we've got an RGB triplet
            SerialBuffer[SerialBufferIndex++] = incomingByte;
            if (SerialBufferIndex == 3)
            {
              lightsUpdated = true;
  
              //Serial.println("Received value for LED " + String(CurrentReceivingLED) + " - " + String(SerialBuffer[0], HEX) + " " + String(SerialBuffer[1], HEX) + " " + String(SerialBuffer[2], HEX));
              Serial.println(String(CurrentReceivingLED));
              CurrentLEDValues[CurrentReceivingLED++] = CRGB(SerialBuffer[0], SerialBuffer[1], SerialBuffer[2]);
              if (CurrentReceivingLED >= LEDCount)
              {
                //Serial.println("All LEDs done");
  
                // Go back to idle
                CurrentSerialInputMode = None;

                moreAvailable = false;
              }
              else
              {
                // Read the next LED
                SerialBufferIndex = 0;
              }
            }

            if(moreAvailable)
            {
                // Read in a tight loop
                if(Serial.available() > 0)
                {
                  incomingByte = Serial.read();
                }
                else
                {
                  moreAvailable = false;
                }
            }
          }
        }
        break;
    }
  }

  if (lightsUpdated)
  {
    //Serial.println("");
    
    // If our lights have been updated, show them
    FastLED.show();
  }
}

