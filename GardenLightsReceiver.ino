/*
  Receiver for the Garden Lights
  Communication is by NRF24L01. Designed for a Arduino Pro Mini
  Using: https://www.sparkfun.com/products/705 for communication
  Based on:
  nrf24_reliable_datagram_server.pde
  -*- mode: C++ -*-

  Connections:

    Pin  3 -> Channel One to MOSFET Gate
    Pin  5 -> Channel Two to MOSFET Gate
    Pin  8 -> NRF24L01 CE
    Pin 10 -> NRF24L01 CSN (Chip select in)
    Pin 11 -> NRF24L01 SDI (SPI MOSI)
    Pin 12 -> NRF24L01 SDO (SPI MOSI)
    Pin 13 -> NRF24L01 SCK (SPI Clock)


  Commands are expected as 15 byte strings. There are three sections:

  CODE CHNL VALU

  CODE is the operation to be performend. Values are:
    MODE - button is pressed to change the channel mode (turn off, turn to pulsing etc)
  POTS - a potentiometer value has changed

  CHNL is the channel, an integer value

  VALU is the value being sent - an integer value. Currently this is only used for POTS messages


*/

#include <RHReliableDatagram.h>
#include <RH_NRF24.h>
#include <SPI.h>

#define TRANSMITTER_ADDRESS 1
#define RECEIVER_ADDRESS 2


#define CHANNEL_ONE_PIN 3
#define CHANNEL_TWO_PIN 5

// Modes that the channels can be in:
const int MODE_OFF = 0;
const int MODE_ON = 1;
const int MODE_PWM = 2;
const int MODE_PULSE = 3;

const char* MODE_TEXT = "MODE";
const char* POTS_TEXT = "POTS";
const int CHANNEL_OFFSET = 5;
const int VALUE_OFFSET = 10;

const int NUM_CHANNELS = 2;
int channel_mode[NUM_CHANNELS];
int channel_pwm[NUM_CHANNELS];
int channel_pulse[NUM_CHANNELS];
int channel_pins[NUM_CHANNELS];
unsigned long channel_time[NUM_CHANNELS];
int channel_pulse_value[NUM_CHANNELS];
int channel_pulse_inc[NUM_CHANNELS];

const int COMMAND_BUF_LEN = 16;
char command_buf[COMMAND_BUF_LEN];

// Singleton instance of the radio driver
RH_NRF24 driver;

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(driver, RECEIVER_ADDRESS);


void setup()
{
  Serial.begin(9600);
  Serial.println("Setting up Garden Lights Receiver" );
  if (!manager.init())
  {
    Serial.println("init of NRF24L01 failed");
    while (true) {
      delay(1000);
    }
  }

  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm
  if (!driver.setRF(RH_NRF24::DataRate250kbps, RH_NRF24::TransmitPower0dBm))
  {
    Serial.println("setRF for power and data rate failed");
    while (true)
      delay(1000);
  }

  // Test each channel for a second:
  pinMode( CHANNEL_ONE_PIN, OUTPUT );
  digitalWrite( CHANNEL_ONE_PIN, HIGH );
  delay(1000);
  digitalWrite( CHANNEL_ONE_PIN, LOW );

  pinMode( CHANNEL_TWO_PIN, OUTPUT );
  digitalWrite( CHANNEL_TWO_PIN, HIGH );
  delay(1000);
  digitalWrite( CHANNEL_TWO_PIN, LOW );


  for ( int i = 0; i < NUM_CHANNELS; i++ ) {
    channel_mode[i] = MODE_OFF;
    channel_pwm[i] = 255;
    channel_pulse[i] = 1024;
  }

  channel_pins[0] = CHANNEL_ONE_PIN;
  channel_pins[1] = CHANNEL_TWO_PIN;

  Serial.println("Garden Lights Receiver setup Ccmplete" );
}


// Dont put this on the stack:
uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];

void loop()
{

  // Check for a message from the transmitter
  if (manager.available())
  {
    // Wait for a message addressed to us from the transmitter
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager.recvfromAck(buf, &len, &from))
    {
      Serial.print("got request from : 0x");
      Serial.print(from, HEX);
      Serial.print(": ");
      Serial.println((char*)buf);
      // Dispatch the message:
      processCommand( (const char*)buf );
    }
  }

  // for each channel if we are in pulse mode then update the value
  // if necessary:
  for( int c = 0; c < NUM_CHANNELS; c++ ) {
    if( channel_mode[c] == MODE_PULSE) {
      if( (millis() - channel_time[c]) > channel_pulse[c] ) {
          channel_pulse_value[c] += channel_pulse_inc[c];
          if( channel_pulse_value[c] > 255 ) {
              channel_pulse_value[c] = 255;
              channel_pulse_inc[c] = -1;
          }
          if( channel_pulse_value[c] < 0 ) {
            channel_pulse_value[c] = 0;
            channel_pulse_inc[c] = 1; 
          }
        analogWrite(channel_pins[c], channel_pulse_value[c]);
        channel_time[c] = millis();
      }
    }
  }



}

void processCommand( const char* buf )
{
  strncpy( command_buf, buf, COMMAND_BUF_LEN );
  int value = atoi( command_buf + VALUE_OFFSET );
  command_buf[VALUE_OFFSET] = '\0';
  int channel = atoi( command_buf + CHANNEL_OFFSET );
  channel = channel - 1; // Zero based indexed

  if ( strncmp(buf, MODE_TEXT , 4) == 0 ) {
    Serial.print("Mode change; channel =" );
    Serial.println(channel);

    channel_mode[channel] ++;


    if ( channel_mode[channel] > MODE_PULSE ) {
      channel_mode[channel] = MODE_OFF;
    }

    Serial.print("    Channel ");
    Serial.print(channel);
    Serial.print(" mode is ");
    Serial.println( channel_mode[channel]);

    if ( channel_mode[channel] == MODE_OFF ) {
      digitalWrite( channel_pins[channel], LOW );
    }

    if (  channel_mode[channel]  == MODE_ON ) {
      digitalWrite( channel_pins[channel], HIGH );
    }

    if (  channel_mode[channel]  == MODE_PWM ) {
      analogWrite( channel_pins[channel], channel_pwm[channel] );
    }

    if (  channel_mode[channel]  == MODE_PULSE ) {
      channel_time[channel] = millis();
      channel_pulse_value[channel] = 0;
      channel_pulse_inc[channel] = 1;
    }

  }

  if ( strncmp( buf, POTS_TEXT, 4) == 0 ) {
    Serial.print("Pot change; channel =" );
    Serial.print(channel);
    Serial.print("; value = ");
    Serial.println(value);
    if ( channel_mode[channel] == MODE_PWM ) {
      channel_pwm[channel] = value / 4;
      analogWrite( channel_pins[channel], channel_pwm[channel] );
    }
    if ( channel_mode[channel] == MODE_PULSE ) {
      channel_pulse[channel] = value/4;
    }

  }

  return;
}