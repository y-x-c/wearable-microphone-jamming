# Wearable Microphone Jamming


<p align="center"><img alt="teaser" src="http://sandlab.cs.uchicago.edu/jammer/imgs/teaser.png" width='500px'></p>


Repository for our paper [Wearable Microphone Jamming](http://sandlab.cs.uchicago.edu/jammer/).

This repo provides simulation source code, hardware design, firmware and schematics to replicate our results and prototypes in the paper.

## Structure of this repo

`simulation` contains MATLAB simulation code
- `doc`: documentation for the simulator
- `simulation.m`: main file of the simulator

`jammer_hardware_source` contains all sources related to hardware
- `3d_printing_models`: 3D model of our final prototype
- `arduino_code`: code for ATMEGA32U4 microprocessor, which controlls an AD9833 signal generator
