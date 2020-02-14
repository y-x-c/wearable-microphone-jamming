/*
 * AD9833.cpp
 * 
 * Copyright 2016 Bill Williams <wlwilliams1952@gmail.com, github/BillWilliams1952>
 *
 * Thanks to john@vwlowen.co.uk for his work on the AD9833. His web page
 * is: http://www.vwlowen.co.uk/arduino/AD9833-waveform-generator/AD9833-waveform-generator.htm
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */

#include "AD9833.h"

/*
 * Create an AD9833 object
 */
AD9833 :: AD9833 ( uint8_t FNCpin, uint32_t referenceFrequency ) {
	// Pin used to enable SPI communication (active LOW)--> MARKED FSY on board
#ifdef FNC_PIN
	pinMode(FNC_PIN,OUTPUT);
#else
	this->FNCpin = FNCpin;
	pinMode(FNCpin,OUTPUT);
#endif
	WRITE_FNCPIN(HIGH);

	/* TODO: The minimum resolution and max frequency are determined by
	 * by referenceFrequency. We should calculate these values and use
	 * them during setFrequency. The problem is if the user programs a
	 * square wave at refFrequency/2, then changes the waveform to sine.
	 * The sine wave will not have enough points?
	 */
	refFrequency = referenceFrequency;
	
	// Setup some defaults
	DacDisabled = false;
	IntClkDisabled = false;
	outputEnabled = false;
	waveForm0 = waveForm1 = SINE_WAVE;
	frequency0 = frequency1 = 1000;		// 1 KHz sine wave to start
	phase0 = phase1 = 0.0;				// 0 phase
	activeFreq = REG0; activePhase = REG0;
}

/*
 * This MUST be the first command after declaring the AD9833 object
 * Start SPI and place the AD9833 in the RESET state
 */
void AD9833 :: Begin ( void ) {
	SPI.begin();
	delay(100);
	Reset();	// Hold in RESET until first WriteRegister command
}

/*
 * Setup and apply a signal. phaseInDeg defaults to 0.0 if not supplied.
 * phaseReg defaults to value of freqReg if not supplied.
 * Note that any previous calls to EnableOut,
 * SleepMode, DisableDAC, or DisableInternalClock remain in effect.
 */
void AD9833 :: ApplySignal ( WaveformType waveType,
		Registers freqReg, float frequencyInHz,
		Registers phaseReg, float phaseInDeg ) {
	SetFrequency ( freqReg, frequencyInHz );
	SetPhase ( phaseReg, phaseInDeg );
	SetWaveform ( freqReg, waveType );
	SetOutputSource ( freqReg, phaseReg );
}

/***********************************************************************
						Control Register
------------------------------------------------------------------------
D15,D14(MSB)	10 = FREQ1 write, 01 = FREQ0 write,
 				11 = PHASE write, 00 = control write
D13	If D15,D14 = 00, 0 = individual LSB and MSB FREQ write,
 					 1 = both LSB and MSB FREQ writes consecutively
	If D15,D14 = 11, 0 = PHASE0 write, 1 = PHASE1 write
D12	0 = writing LSB independently
 	1 = writing MSB independently
D11	0 = output FREQ0,
	1 = output FREQ1
D10	0 = output PHASE0
	1 = output PHASE1
D9	Reserved. Must be 0.
D8	0 = RESET disabled
	1 = RESET enabled
D7	0 = internal clock is enabled
	1 = internal clock is disabled
D6	0 = onboard DAC is active for sine and triangle wave output,
 	1 = put DAC to sleep for square wave output
D5	0 = output depends on D1
	1 = output is a square wave
D4	Reserved. Must be 0.
D3	0 = square wave of half frequency output
	1 = square wave output
D2	Reserved. Must be 0.
D1	If D5 = 1, D1 = 0.
	Otherwise 0 = sine output, 1 = triangle output
D0	Reserved. Must be 0.
***********************************************************************/

/*
 * Hold the AD9833 in RESET state.
 * Resets internal registers to 0, which corresponds to an output of
 * midscale - digital output at 0.
 * 
 * The difference between Reset() and EnableOutput(false) is that
 * EnableOutput(false) keeps the AD9833 in the RESET state until you
 * specifically remove the RESET state using EnableOutput(true).
 * With a call to Reset(), ANY subsequent call to ANY function (other
 * than Reset itself and Set/IncrementPhase) will also remove the RESET
 * state.
 */
void AD9833 :: Reset ( void ) {
	WriteRegister(RESET_CMD);
	delay(15);
}

/*
 *  Set the specified frequency register with the frequency (in Hz)
 */
void AD9833 :: SetFrequency ( Registers freqReg, float frequency ) {
	// TODO: calculate max frequency based on refFrequency.
	// Use the calculations for sanity checks on numbers.
	// Sanity check on frequency: Square - refFrequency / 2
	//							  Sine/Triangle - refFrequency / 4
	if ( frequency > 12.5e6 )	// TODO: Fix this based on refFreq
		frequency = 12.5e6;
	if ( frequency < 0.0 ) frequency = 0.0;
	
	// Save frequency for use by IncrementFrequency function
	if ( freqReg == REG0 ) frequency0 = frequency;
	else frequency1 = frequency;
	
	int32_t freqWord = (frequency * pow2_28) / (float)refFrequency;
	int16_t upper14 = (int16_t)((freqWord & 0xFFFC000) >> 14), 
			lower14 = (int16_t)(freqWord & 0x3FFF);

	// Which frequency register are we updating?
	uint16_t reg = freqReg == REG0 ? FREQ0_WRITE_REG : FREQ1_WRITE_REG;
	lower14 |= reg;
	upper14 |= reg;   

	// I do not reset the registers during write. It seems to remove
	// 'glitching' on the outputs.
	WriteControlRegister();
	// Control register has already been setup to accept two frequency
	// writes, one for each 14 bit part of the 28 bit frequency word
	WriteRegister(lower14);			// Write lower 14 bits to AD9833
	WriteRegister(upper14);			// Write upper 14 bits to AD9833
}

/*
 * Increment the specified frequency register with the frequency (in Hz)
 */
void AD9833 :: IncrementFrequency ( Registers freqReg, float freqIncHz ) {
	// Add/subtract a value from the current frequency programmed in
	// freqReg by the amount given
	float frequency = (freqReg == REG0) ? frequency0 : frequency1;
	SetFrequency(freqReg,frequency+freqIncHz);
}

/*
 * Set the specified phase register with the phase (in degrees)
 * The output signal will be phase shifted by 2π/4096 x PHASEREG
 */
void AD9833 :: SetPhase ( Registers phaseReg, float phaseInDeg ) {
	// Sanity checks on input
	phaseInDeg = fmod(phaseInDeg,360);
	if ( phaseInDeg < 0 ) phaseInDeg += 360;
	
	// Phase is in float degrees ( 0.0 - 360.0 )
	// Convert to a number 0 to 4096 where 4096 = 0 by masking
	uint16_t phaseVal = (uint16_t)(BITS_PER_DEG * phaseInDeg) & 0x0FFF;
	phaseVal |= PHASE_WRITE_CMD;
	
	// Save phase for use by IncrementPhase function
	if ( phaseReg == REG0 )	{
		phase0 = phaseInDeg;
	}
	else {
		phase1 = phaseInDeg;
		phaseVal |= PHASE1_WRITE_REG;
	}
	WriteRegister(phaseVal);
}

/*
 * Increment the specified phase register by the phase (in degrees)
 */
void AD9833 :: IncrementPhase ( Registers phaseReg, float phaseIncDeg ) {
	// Add/subtract a value from the current phase programmed in
	// phaseReg by the amount given
	float phase = (phaseReg == REG0) ? phase0 : phase1;
	SetPhase(phaseReg,phase + phaseIncDeg);
}

/*
 * Set the type of waveform that is output for a frequency register
 * SINE_WAVE, TRIANGLE_WAVE, SQUARE_WAVE, HALF_SQUARE_WAVE
 */
void AD9833 :: SetWaveform (  Registers waveFormReg, WaveformType waveType ) {
	// TODO: Add more error checking?
	if ( waveFormReg == REG0 )
		waveForm0 = waveType;
	else
		waveForm1 = waveType;
	WriteControlRegister();
}

/*
 * EnableOutput(false) keeps the AD9833 is RESET state until a call to
 * EnableOutput(true). See the Reset function description.
 */
void AD9833 :: EnableOutput ( bool enable ) {
	outputEnabled = enable;
	WriteControlRegister();
}

/*
 * Set which frequency and phase register is being used to output the
 * waveform. If phaseReg is not supplied, it defaults to the same
 * register as freqReg.
 */
void AD9833 :: SetOutputSource ( Registers freqReg, Registers phaseReg ) {
	// TODO: Add more error checking?
	activeFreq = freqReg;
	if ( phaseReg == SAME_AS_REG0 )	activePhase = activeFreq;
	else activePhase = phaseReg;
	WriteControlRegister();
}

//---------- LOWER LEVEL FUNCTIONS NOT NORMALLY NEEDED -------------

/*
 * Disable/enable both the internal clock and the DAC. Note that square
 * wave outputs are avaiable if using an external Reference.
 * TODO: ?? IS THIS TRUE ??
 */
void AD9833 :: SleepMode ( bool enable ) {
	DacDisabled = enable;
	IntClkDisabled = enable;
	WriteControlRegister();
}

/*
 * Enables / disables the DAC. It will override any previous DAC
 * setting by Waveform type, or via the SleepMode function
 */
void AD9833 :: DisableDAC ( bool enable ) {
	DacDisabled = enable;
	WriteControlRegister();	
}

/*
 * Enables / disables the internal clock. It will override any 
 * previous clock setting by the SleepMode function
 */
void AD9833 :: DisableInternalClock ( bool enable ) { 
	IntClkDisabled = enable;
	WriteControlRegister();	
}

// ------------ STATUS / INFORMATION FUNCTIONS -------------------
/*
 * Return actual frequency programmed
 */
float AD9833 :: GetActualProgrammedFrequency ( Registers reg ) {
	float frequency = reg == REG0 ? frequency0 : frequency1;
	int32_t freqWord = (uint32_t)((frequency * pow2_28) / (float)refFrequency) & 0x0FFFFFFFUL;
	return (float)freqWord * (float)refFrequency / (float)pow2_28;
}

/*
 * Return actual phase programmed
 */
float AD9833 :: GetActualProgrammedPhase ( Registers reg ) {
	float phase = reg == REG0 ? phase0 : phase1;
	uint16_t phaseVal = (uint16_t)(BITS_PER_DEG * phase) & 0x0FFF;
	return (float)phaseVal / BITS_PER_DEG;
}

/*
 * Return frequency resolution
 */
float AD9833 :: GetResolution ( void ) {
	return (float)refFrequency / (float)pow2_28;
}

// --------------------- PRIVATE FUNCTIONS --------------------------

/*
 * Write control register. Setup register based on defined states
 */
void AD9833 :: WriteControlRegister ( void ) {
	uint16_t waveForm;
	// TODO: can speed things up by keeping a writeReg0 and writeReg1
	// that presets all bits during the various setup function calls
	// rather than setting flags. Then we could just call WriteRegister
	// directly.
	if ( activeFreq == REG0 ) {
		waveForm = waveForm0;
		waveForm &= ~FREQ1_OUTPUT_REG;
	}
	else {
		waveForm = waveForm1;
		waveForm |= FREQ1_OUTPUT_REG;
	}
	if ( activePhase == REG0 )
		waveForm &= ~PHASE1_OUTPUT_REG;
	else
		waveForm |= PHASE1_OUTPUT_REG;
	if ( outputEnabled )
		waveForm &= ~RESET_CMD;
	else
		waveForm |= RESET_CMD;
	if ( DacDisabled )
		waveForm |= DISABLE_DAC;
	else
		waveForm &= ~DISABLE_DAC;
	if ( IntClkDisabled )
		waveForm |= DISABLE_INT_CLK;
	else
		waveForm &= ~DISABLE_INT_CLK;

	WriteRegister ( waveForm );
}

void AD9833 :: WriteRegister ( int16_t dat ) {
	/*
	 * We set the mode here, because other hardware may be doing SPI also
	 */
	SPI.setDataMode(SPI_MODE2);

	/* Improve overall switching speed
	 * Note, the times are for this function call, not the write.
	 * digitalWrite(FNCpin)			~ 17.6 usec
	 * digitalWriteFast2(FNC_PIN)	~  8.8 usec
	 */
	WRITE_FNCPIN(LOW);		// FNCpin low to write to AD9833

	//delayMicroseconds(2);	// Some delay may be needed

	// TODO: Are we running at the highest clock rate?
	SPI.transfer(highByte(dat));	// Transmit 16 bits 8 bits at a time
	SPI.transfer(lowByte(dat));

	WRITE_FNCPIN(HIGH);		// Write done
}
