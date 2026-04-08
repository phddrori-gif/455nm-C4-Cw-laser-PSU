# 455 nm 151 W Laser Array

## Overview
This repository documents the design, assembly, and testing of a 455 nm, 151 W diode laser array system built for high-power laser experimentation and materials research. The project combines optical, electrical, thermal, and safety engineering to create a controlled platform for evaluating laser performance and exploring laser-heated crystal growth.

## Project Objectives
- Achieve the full 151 W output of the diode array
- Explore laser-based growth of synthetic ruby
- Evaluate whether the system can be adapted for additional materials, including sapphire

## Current Status
The laser platform is operational in several core areas, with major progress completed in the safety, electrical, and mechanical subsystems. Remaining work is focused on characterization, documentation, and material-processing experiments.

## Safety Systems
Because this project involves a high-power 455 nm laser source, safety controls were treated as a primary design requirement.

Implemented safety features:
- OD6+ laser safety eyewear rated for 455 nm
- Hall-effect current sensing with ESP32-based monitoring
- Temperature-based automatic shutdown
- Emergency stop (E-stop)

## Mechanical and System Assembly
Completed design and integration work includes:
- 3D CAD model for the laser and circuit assembly
- Circuit design and schematic development
- PCB layout
- Custom lens holder
- Hybrid cooling system using both water cooling and forced-air cooling

## Electrical and Control Design
Completed electrical work:
- Tested PWM control and mechanical switch logic
- Verified full-wave rectifier suitability for 120 V AC input
- Integrated an IRM-05-3.3 power supply for the Teensy 4.0 controller
- Selected a 5.6 kOhm thermistor reference resistor based on a maximum expected operating temperature of 75 C

In progress:
- Fully label and reorganize the circuit diagram into clearly separated functional blocks

## Optical and Thermal Work
Completed:
- Selected a candidate focal length for the lens system (approximately +30 cm)

In progress:
- Determine the diode array's maximum allowable thermal load
- Measure and verify the optical power output of the diode array

## Materials and Crystal Growth Experiments
Completed:
- Acquired crucible for high-temperature material tests
- Acquired aluminum powder and chromium oxide for ruby experiments
- Planned work to be conducted in a fume hood

Planned:
- Acquire aluminum powder and iron oxide for sapphire experiments
- Evaluate the addition of titanium oxide for blue sapphire trials

## Next Steps
- Complete final circuit documentation
- Characterize thermal limits of the diode array
- Measure output power under controlled operating conditions
- Begin laser-heated ruby growth experiments
- Expand testing to additional materials if ruby trials are successful

## Resume Highlights
This project demonstrates experience in:
- High-power laser system integration
- Embedded monitoring and safety interlocks
- PCB and circuit design
- Thermal management
- Experimental hardware development
- Applied materials research

## Disclaimer
This repository documents work involving high-power laser equipment and high-temperature material processing. Testing should only be performed with proper protective equipment, controlled procedures, and appropriate lab safety infrastructure.
