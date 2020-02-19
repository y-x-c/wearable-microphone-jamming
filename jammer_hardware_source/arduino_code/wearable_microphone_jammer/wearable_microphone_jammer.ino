/*
    wearable microphone jammer
    --------
    This is the code for 
    Circuit buildup and wiring explanation added in comments by Steven Nagels and Shan-Yuan Teng.
   
    HARDWARE
    --------
    - Pro Trinket 5V board. https://www.adafruit.com/product/2000
    - AD9833 Waveform Module https://www.amazon.com/gp/product/B076LF3LFC
   
    CONNECTION
    --------
    AD9833 PCB to Pro Trinket board:
    DAT --> pin 11 (MOSI) [note: pin 12 is MISO]
    CLK --> pin 13 (SCK)
    FSY --> pin A0 (AD9833 SS)
    CS --> pin A1 (MCP41010i SS)
    
    AD9833 PCB OVERVIEW
    --------
    PCB has following INPUTS (in order): CS, DAT, CLK, FSY, GND, VCC
                  and OUTPUTS (in order): GND, PGA, (footprint for SMA connector), VOUT, GND

    LOOKING AT PCB WITH INPUTS FACING YOU: left side = PGA, right side = waveform generator

    (left side)
    PGA stands for Programmable Gain Amplifier and is constructed of a MCP41010i digital potentiometer (addressed via SPI)
    and an AD8051 high speed operational amplifier. By changing the MCP41010i resistance value, you set the amplifier's gain.
    The SMA connector footprint also connects to the PGA output.

    MCP41010i is on the SPI bus, thus has a shared connection to DAT and CLK with the AD9833. Its chip select is the CS input.

    MCP41010i datasheet: http://ww1.microchip.com/downloads/en/devicedoc/11195c.pdf
    AD8051 datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/AD8051_8052_8054.pdf

    (right side)
    A large 25 MHz xtal on top half of board. It determines the reference frequency used in the code below. AD9833 situated
    on bottom half of board. This part is completely controlled via SPI.

    AD9833 is on the SPI bus, thus has a shared connection to DAT and CLK with the MCP41010i. Its chip select is the FSY input.

    AD9833 datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/AD9833.pdf
*/

/*
   ApplySignal.ino
   2018 WLWilliams

   This sketch demonstrates the basic use of the AD9833 DDS module library.
   Using the ApplySignal to generate and/or change the signal.

   This program is free software: you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free Software Foundation,
   either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details. You should have received a copy of
   the GNU General Public License along with this program.
   If not, see <http://www.gnu.org/licenses/>.

   This example code is in the public domain.

   Library code found at: https://github.com/Billwilliams1952/AD9833-Library-Arduino
*/

#include "AD9833.h"

#define FNC_PIN A0
#define SEL_PIN A1

int gen_freq = 25000;

int RXLED = 17;  // The RX LED has a defined Arduino pin

//--------------- Create an AD9833 object ----------------
// Note, SCK and MOSI must be connected to CLK and DAT pins on the AD9833 for SPI
AD9833 gen(FNC_PIN);       // Defaults to 25MHz internal reference frequency

void setup() {
  //start gen
  gen.Begin();
  pinMode(SEL_PIN, OUTPUT);
  //set gen to SINE
  gen.ApplySignal(SINE_WAVE, REG0, gen_freq);
  //start gens
  gen.EnableOutput(true);
  //put both PGAs at max output
  MCP41010Write(255, SEL_PIN);
  SPI.setDataMode(SPI_MODE2);
}

void loop() {
  gen.ApplySignal(SINE_WAVE, REG0, random(24000, 26000));
  MCP41010Write(255, SEL_PIN);
}

//  function below adapted from http://henrysbench.capnfatz.com/henrys-bench/arduino-output-devices/mcp41010-digital-potentiometer-arduino-user-manual/
//  8bit value to write --> 256 levels --> value 0 to 255
void MCP41010Write(byte value, int CS)
{
  SPI.setDataMode(SPI_MODE0);
  // Note that the integer vale passed to this subroutine
  // is cast to a byte
  digitalWrite(CS, LOW);
  SPI.transfer(B00010001); // This tells the chip to set the pot
  SPI.transfer(value);     // This tells it the pot position
  digitalWrite(CS, HIGH);
}
