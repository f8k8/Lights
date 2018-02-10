// Code for light controller board
// Serial protocol is:
// Out: 'H' - Hello packet. Sent from the board to test for connection to the PC. PC should response with an 'H' to indicate its presence.
// In: 'H' - Response hello packet - sent from the PC to the board in response to the board's Hello packet
// Out: 'C' - Config request packet. Sent from the board to request the config from the PC
// In: 'CXY' - Config response packet. Format is 'C' followed by a single byte for LED X count and a single byte for LED Y count
// In: 'A' - Light data availabel packet. Sent from the PC to indicate new light data is available
// Out: 'R' - Ready to receive light data. Sent from the board to the PC to indiciate it is ready for the light data.
// In: Light data - A stream of 3 bytes * number of LEDs (X * Y), indicating the RGB for each LED
// In: 'K' - Keep alive packet sent from the PC to indicate we're still here, but no light data is available
// In: 'D' - Output debug info
// Out: 'D' - A line of debug info

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

// Timeout in millis for the last received input from the serial
#define SERIAL_INPUT_TIMEOUT_MILLIS 2000

// Timeout in millis when waiting for a response over serial
#define SERIAL_RESPONSE_TIMEOUT_MILLIS 500

// Time between 'H'ello packets in millis
#define SERIAL_TIME_BETWEEN_HELLO_MILLIS 1000

// ------------------------------
// LED control
// ------------------------------

// Current and target LED values
CRGB CurrentLEDValues[MAX_NUM_LEDS];

// Number of LEDs running
int LEDCount = 0;

// -----------------------------
// Serial protocol
// -----------------------------

// For tracking our serial mode
enum ECurrentSerialMode
{
  Initialise,
  Waiting
};

ECurrentSerialMode CurrentSerialMode;

// Timer for timing serial events / handling timeouts
long SerialTimeoutTime = 0;

// Buffer for receiving serial packets
uint8_t SerialBuffer[16];

// Setup function
void setup()
{
  // Start the serial port
  Serial.begin(288000);

  // Power-up safety delay
  delay(3000);

  CurrentSerialMode = Initialise;

  fill_solid(CurrentLEDValues, MAX_NUM_LEDS, CRGB::Black);

  // Initialise FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(CurrentLEDValues, MAX_NUM_LEDS);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show();
}

bool waitForSerialData(int numBytesToWaitFor)
{
  long responseTimeout = millis() + SERIAL_RESPONSE_TIMEOUT_MILLIS;
  uint8_t serialBufferIndex = 0;

  while (serialBufferIndex < numBytesToWaitFor && millis() < responseTimeout)
  {
    if (Serial.available())
    {
      SerialBuffer[serialBufferIndex++] = Serial.read();
    }
  }

  return numBytesToWaitFor == serialBufferIndex;
}

void cleanSerialBuffer()
{
  while(Serial.available())
  {
    Serial.read();
  }
}

void loop()
{
  // If we're not in the initialise mode, and haven't heard anything over the serial for a while, timeout and reset
  if (CurrentSerialMode != Initialise && millis() >= SerialTimeoutTime)
  {
    // We've timed out. Reset the lights and go back to the initialise state
    if (LEDCount != 0)
    {
      fill_solid(CurrentLEDValues, MAX_NUM_LEDS, CRGB::Black);
      FastLED.show();
      LEDCount = 0;
    }

    CurrentSerialMode = Initialise;
    SerialTimeoutTime = millis() + SERIAL_TIME_BETWEEN_HELLO_MILLIS;
  }

  switch (CurrentSerialMode)
  {
    case Initialise:
      {
        cleanSerialBuffer();
        
        // Send a 'H'ello packet periodically until we get one back
        if (millis() >= SerialTimeoutTime)
        {
          SerialTimeoutTime = millis() + SERIAL_TIME_BETWEEN_HELLO_MILLIS;

          // Send a 'H'
          Serial.write('H');
          if (waitForSerialData(1))
          {
            if(SerialBuffer[0] == 'H')
            {
              // Got an 'H' back, so request our config
              Serial.write('C');
              if (waitForSerialData(3))
              {
                if(SerialBuffer[0] == 'C')
                {
                  // Read out our config
                  LEDCount = SerialBuffer[1] * SerialBuffer[2];
                  CurrentSerialMode = Waiting;
    
                  // Reset our timeout
                  SerialTimeoutTime = millis() + SERIAL_INPUT_TIMEOUT_MILLIS;
                }
              }
            }
          }
        }
      }
      break;

    case Waiting:
      {
        if (Serial.available())
        {
          int incomingByte = Serial.read();
          switch (incomingByte)
          {
            case 'A':
              {
                // Light data is available, so signal we're ready for it then receive it
                SerialTimeoutTime = millis() + SERIAL_RESPONSE_TIMEOUT_MILLIS;
                int numBytesToReceive = LEDCount * 3;
                uint8_t* currentByte = &CurrentLEDValues[0].r;
                uint8_t* endByte = currentByte + numBytesToReceive;
                Serial.write('R');
                while (currentByte < endByte && millis() < SerialTimeoutTime)
                {
                  if(Serial.available())
                  {
                    *currentByte = Serial.read();
                    ++currentByte;
                  }
                }
                if(currentByte == endByte)
                {
                  // We've received all our light data
                  FastLED.show();

                  // Reset our timeout
                  SerialTimeoutTime = millis() + SERIAL_INPUT_TIMEOUT_MILLIS;
                }
                else
                {
                  Serial.println("D Timed out receiving light data");
                }
              }
              break;

            case 'K':
              {
                // Reset our timeout
                SerialTimeoutTime = millis() + SERIAL_INPUT_TIMEOUT_MILLIS;
              }
              break;

            case 'D':
              {
                String debugInfo = String("D") + "SerialTimeoutTime: " + String(SerialTimeoutTime) + " | millis(): " + String(millis()) + " | CurrentSerialMode: " + String(CurrentSerialMode);
                Serial.println(debugInfo);
              }
              break;

            default:
              {
                // Unknown command, so ignore it
                uint8_t* currentByte = &CurrentLEDValues[0].r;
                String info = String("D Unknown command received: ") + String(incomingByte) + String(" Remaining buffer length: ") + String(Serial.available()) + String(" First light value: ") + String(*currentByte);
                Serial.println(info);
                cleanSerialBuffer();
              }
              break;
          }
        }
      }
      break;

    default:
      break;
  }
}

