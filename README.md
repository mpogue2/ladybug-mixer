# ladybug-mixer
Portable, low-cost audio mixer for square dance callers

**Version 3 enclosure (powder-coated, laser-engraved lettering)**:

<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/main/pics/V3_All.jpeg" width="500">

<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/main/pics/V3_Top.jpeg" width="500">
<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/main/pics/V3_Front.jpeg" width="500">
<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/main/pics/V3_Back.jpeg" width="500">

<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/main/pics/Main_PCB_V2.2.jpeg" width="500">

# Description

The Ladybug LB-102 Mixer is an inexpensive ($75 parts cost) compact audio mixer, focused on the needs of beginning and intermediate square dance callers. With the addition of a dynamic microphone like a [Sennheiser e845 and cable, $100](https://www.amazon.com/Sennheiser-Cardioid-Handheld-Dynamic-Microphone/dp/B07CVK32VV), a [standard XLR mic cable for low-noise output, $10](https://www.amazon.com/dp/B01JNLTTKS), and a powered speaker like an [Alto TS-408, 2000W peak, $250](https://www.amazon.com/Alto-Professional-TS408-Bluetooth-Loudspeaker/dp/B0BB15J4TZ), a caller can provide high-quality sound for 8-10 squares for less than $450.  For compatibility with existing Hilton(TM) microphone cables, the Ladybug Mixer also provides a jack for remote volume control.

A single 9-volt alkaline battery provides power for months (or years!) of dances, and the Ladybug Mixer’s compact size makes it easy to carry.
Here's a graph of what battery life looks like, for 3 different types of batteries.  The most commonly-used 9V alkaline battery ($1.50 from Amazon) 
ran continuously for about 170 hours in my tests (that's a lot of dances!).

<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/main/tests/BatteryLife/V2_batteryLife_noRVC.png" width="500">

The entire Ladybug Mixer design is available under an Open Source License, allowing anybody to modify the design to fit their specific needs.
If anybody wants to build these commercially, then can do so, as long as any modifications they make are passed back to the community under
the same licenses.

> NOTE: This is a "mixer", and not a "mixer/amplifier".  A Hilton(TM) MA-220 is both a mixer and an amplifier, so it will be able
to send 220W of power to UN-powered speakers.  The Ladybug mixer is designed to send a "line-level" signal to speakers that are "powered",
that is, the speaker has the amplifier built-in.  There are many popular "powered speakers" available nowadays, like the [Kustom PA-50](https://www.musiciansfriend.com/pro-audio/kustom-pa-pa50-personal-pa-system) (50 watts peak), the [Alto TS-408](https://www.sweetwater.com/store/detail/TS408--alto-ts408-2000-watt-8-inch-powered-speaker) (2000 watts peak), the [Bose L1 column speaker](https://www.bestbuy.com/site/bose-l1-pro8-portable-line-array-system-black) (300 watts peak), and many others.

The Version 3 mixer (shown above) uses a high-quality aluminum case (powder-coated in gloss red, and engraved with a 20W fiber laser), with black 3D-printed side and bottom panels. I expect that V3 kits will also be available, allowing anybody to design and build their own custom case.  **Let the creativity begin!**

# Specs
- 2 Music Inputs (1/8” stereo inputs), with a slide switch to select one or the other
- 2 Combo Mic Inputs (Lo-Z XLR or Hi-Z [Hilton™-compatible](https://www.hiltonaudio.com/store/c3/Microphone_Cables.html) 1/4“ plug) 
- Remote Volume Control Input (20KΩ [Hilton™-compatible](https://www.hiltonaudio.com/store/c3/Microphone_Cables.html) 1/8” plug)
- Low-noise Balanced Output (XLR, +/- 5 volt differential swing at max volume)
- Separate Music (L) and Voice (R) out (1/8” stereo jack, +/- 2.5 volts peak-to-peak)
	- Use Music-only for a monitor speaker, or use Voice-only for hearing assist
- Uses a single 9V battery
	- Amazon Basics Alkaline ($1.50 ea. @ AMZN): about 170 hours
	- Tenergy Lithium ($7.00 ea. @ AMZN): about 300 hours
	- USB-rechargeable Lithium ($7.50 ea. @ AMZN): about 100 hours per charge
- White front-panel LED indicates power ON and battery level (flashes when low)
- Low noise: -80dBfs
- Wide 3dB Bandwidth: Music 20Hz-20KHz, Voice 50Hz-20KHz
- Small size: 6.2”W x 2.75”H x 3.8”D (157 x 70 x 97 mm)
- Light weight (with Alkaline battery): 12.5oz (355g)
- Open Source Hardware, Software, and Mechanical Design (industry-standard open source licenses)

# Files available under Open Source licenses
- SCH = schematic
- GERBER = PC board layout
- BOM = Bill of Materials (specifies which parts to place onto the board)
- PickAndPlace = specifies where to place parts onto the board
- .lbrn2 = Lightburn 2 files for the laser-cut top/bottom/back/front
- .f3d = Fusion file for the side panels
- .stl = 3D model for the 3D-printed side panels
- .asc/.asy/.pdf = simulation files for LTspice

# Open Source Licenses applying to all contents of this repository
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

