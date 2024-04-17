# ladybug-mixer
Portable, low-cost audio mixer for square dance callers

**Version 3 prototype (no control labels yet)**:

<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/main/pics/Case_V3_with_knobs.jpeg" width="500">

<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/main/pics/Main_PCB_V2.2.jpeg" width="500">

# Description

The Ladybug LB-102 Mixer is an inexpensive ($75) compact audio mixer, focused on the needs of beginning and intermediate square dance callers. With the addition of a dynamic microphone like a [Sennheiser e845 and cable, $100](https://www.amazon.com/Sennheiser-Cardioid-Handheld-Dynamic-Microphone/dp/B07CVK32VV), a [standard XLR mic cable for low-noise output, $10](https://www.amazon.com/dp/B01JNLTTKS), and a powered speaker like an [Alto TS-408, 2000W peak, $250](https://www.amazon.com/Alto-Professional-TS408-Bluetooth-Loudspeaker/dp/B0BB15J4TZ), a caller can provide high-quality sound for 8-10 squares for less than $450. 

A single 9-volt alkaline battery provides power for months (or years!) of dances, and the Ladybug mixer’s compact size makes it easy to carry.
The entire Ladybug Mixer design is available under an Open Source License, allowing anybody to modify the design to fit their specific needs.

The Version 3 prototype (in progress now) uses a high-quality aluminum case (brushed, painted, powder-coated, or anodized), with 3D-printed or wood (e.g. walnut or ebonized ash) side panels. I expect that V3 kits will also be available, allowing anybody to design and build their own custom case.  **Let the creativity begin!**

# Specs
- 2 Music Inputs (1/8” stereo inputs), with a slide switch to select one or the other
- 2 Combo Mic Inputs (Lo-Z XLR or Hi-Z [Hilton™-compatible](https://www.hiltonaudio.com/store/c3/Microphone_Cables.html) 1/4“ plug) 
- Remote Volume Control Input (20KΩ [Hilton™-compatible](https://www.hiltonaudio.com/store/c3/Microphone_Cables.html) 1/8” plug)
- Low-noise Balanced Output (XLR)
- Separate Music (L) and Voice (R) out (1/8” stereo jack)
	- Use Music-only for a monitor speaker, or use Voice-only for hearing assist
- Uses a single 9V battery
	- Amazon Basics Alkaline ($1.50 ea. @ AMZN): 150 hours
	- Tencent Lithium ($7.00 ea. @ AMZN): 300 hours
	- USB-rechargeable Lithium ($7.50 ea. @ AMZN): 90 hours per charge
- White front-panel LED indicates power ON and battery level
- Low noise: -80dBfs
- Wide Bandwidth: Music 20Hz-20KHz, Voice 50Hz-20KHz
- Small size: 6.9”W x 2.5”H x 3.75”D (175 x 65 x 95 mm) 
- Light weight (with Alkaline battery): 12.5oz (355g)
- Open Source Hardware, Software, and Mechanical Design (industry-standard open source licenses)

# Prototype Version 2 files
- SCH = schematic
- GERBER = PC board layout
- BOM = Bill of Materials (specifies which parts to place onto the board)
- PickAndPlace = specifies where to place parts onto the board
- .lbrn2 = Lightburn 2 files for the laser-cut top/bottom/back/front
- .f3d = Fusion file for the side panels
- .stl = 3D model for the 3D-printed side panels
- .asc/.asy/.pdf = simulation files for LTspice

# Licenses applying to all contents of this repository
- Hardware = CERN OHL-S (https://ohwr.org/cern_ohl_s_v2.txt)
- Software/Firmware = GNU General Public License version 3 (https://www.gnu.org/licenses/gpl-3.0.txt)
- Documentation = CC BY-SA License (https://creativecommons.org/licenses/by-sa/4.0/legalcode.txt)

# PICS OF THE V2 PROTOTYPE

ENCLOSURE:

<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/d039de7adba558213b8dbf10dc7b3b2c49956114/V2/pics/PrototypeEnclosure_V2.jpg" width="500">

FRONT PANEL (laser cut wood):

<img src="https://github.com/mpogue2/ladybug-mixer/blob/main/V2/pics/V2_FRONT.jpg" width="500">

REAR PANEL (laser cut wood):

<img src="https://github.com/mpogue2/ladybug-mixer/blob/main/V2/pics/V2_BACK.jpg" width="500">

PCB:

<img src="https://github.com/mpogue2/ladybug-mixer/blob/main/V2/pics/V2_PCB.jpeg" width="500">

