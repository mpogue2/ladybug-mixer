# ladybug-mixer
Second generation, portable, low-cost audio mixer for square dance callers and round dance cuers

**First pics of the LB-202 enclosure (powder-coated, laser-engraved lettering), and inside components**:

<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/refs/heads/main/LB-202/graphics/LB202.jpg" width="500">

<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/refs/heads/main/pics/LB202_inside.jpeg" width="500">

# Description

The Ladybug LB-202 Mixer is an inexpensive ($100 parts cost) compact audio mixer, focused on the needs of beginning and intermediate square dance callers. With the addition of a dynamic microphone like a [Sennheiser e845 and cable, $100](https://www.amazon.com/Sennheiser-Cardioid-Handheld-Dynamic-Microphone/dp/B07CVK32VV), a [standard XLR mic cable for low-noise output, $10](https://www.amazon.com/dp/B01JNLTTKS), and a powered speaker like an [Alto TS-408, 2000W peak, $250](https://www.amazon.com/Alto-Professional-TS408-Bluetooth-Loudspeaker/dp/B0BB15J4TZ), a caller can provide high-quality sound for 8-10 squares for about $450.

#### Compatibility with Hilton™-compatible mic cables
For compatibility with existing Hilton™-compatible microphone cables, the Ladybug Mixer provides two features:

1. Two "microphone combo jacks" that let you plug in a Hilton™-compatible 1/4" plug, OR a standard XLR cable (allows for a longer mic cable and best noise suppression, with no "dongle" required).
2. A 1/8" (3.5mm) jack for remote volume control (RVC).  The Ladybug RVC provides two volume-control curves:
    - a "Traditional" curve that matches the volume curve you get with a Hilton™ MA-220, or
    - a "Ladybug" curve that allows you to bring the volume all the way down to zero (MUTE).
   Which curve is in use is selected by using a SIM card tool to press an internal mode-switch button.
   
<img src="https://raw.githubusercontent.com/mpogue2/ladybug-mixer/refs/heads/main/LB-202/graphics/rvc_curves.png" width="500" />

#### RGB LED modes
The Ladybug LB-202 also provides several LED modes (selectable by using a SIM card tool to press an internal button) for use with the built-in RGB LED:

1. Battery Monitor (normally GREEN, changes to YELLOW, then dims and flashes RED when the battery is low)
2. VU Meter (pulses GREEN with the audio output, and changes to RED when clipping is present)
3. Solid colors (RED, GREEN, BLUE, or WHITE)

#### Long Battery Life
A single 9-volt alkaline battery provides power for months of dances, and the Ladybug Mixer’s compact size makes it easy to carry. An off-the-shelf 9V alkaline battery ($1.50 from Amazon) will power the Ladybug mixer for about 90 hours (that's a lot of dances!), while a 9V lithium battery ($6.50 from Amazon) will probably last about 150 hours.

#### Open Source Design
The entire Ladybug Mixer design is available under an Open Source License, allowing anybody to modify the design to fit their specific needs. If anybody wants to build these commercially, they can do so, AS LONG AS any modifications they make are passed back to the community under the same licenses.  ***Let the creativity begin!***

> NOTE: The Ladybug LB-202 is a "mixer", and not a "mixer/amplifier".  A Hilton™ MA-220 is both a mixer and an amplifier, so it is able to send 220W of power to UN-powered speakers.  The Ladybug mixer is designed to send a "line-level" signal to speakers that are "powered", that is, the speaker has an amplifier built-in.  There are many popular "powered speakers" available nowadays, like the [Kustom PA-50](https://www.musiciansfriend.com/pro-audio/kustom-pa-pa50-personal-pa-system) (50 watts peak), the [Alto TS-408](https://www.sweetwater.com/store/detail/TS408--alto-ts408-2000-watt-8-inch-powered-speaker) (2000 watts peak), the [Bose L1 column speaker](https://www.bestbuy.com/site/bose-l1-pro8-portable-line-array-system-black) (300 watts peak), and many others.

The LB-202 mixer (shown above) uses a high-quality aluminum case (powder-coated in gloss red, and engraved with a 20W fiber laser), with an aluminum base panel, and black 3D-printed side and bottom panels.

#### Differences from the First-generation LB-102 Mixer
|   |  LB-102 |  LB-202 |
|---|:-:|:-:|
|  Music 1 vs 2 switch |  ✅ |  ❌ |
|  Music L/R Balance controls |  ❌ |  ✅ for cuers and split tracks |
|  LED |  white |  RGB |
|          - VU Meter capability          |                ❌                |                    ✅                    |
|  Mic tone controls |  12 o'clock position = FLAT EQ |  12 o'clock position matches MA-220 EQ (bass cut + treble boost) |
|          Feature control          |                2 internal jumpers                |                    2 user-accessible switches                    |
|                  Bottom panel                 |                        Black PLA                        |                                5052 Aluminum                                |
|                        Firmware flash                       |                             Internal 0.1" pin header                            |                                       Internal JST-SH connector                                      |
|                               Future experimental features                              |                                      ❌                                     |                                                   I2C via internal Stemma-QT connector                                                   |

# Specs
- 2 Music Inputs (1/8” stereo inputs), with a slide switch to select one or the other
- 2 Combo Mic Inputs (Lo-Z XLR or Hi-Z [Hilton™-compatible](https://www.hiltonaudio.com/store/c3/Microphone_Cables.html) 1/4“ plug) 
- Remote Volume Control Input (20KΩ [Hilton™-compatible](https://www.hiltonaudio.com/store/c3/Microphone_Cables.html) 1/8” plug)
- Low-noise Balanced Output to powered speaker (XLR, +/- 5 volt differential swing at max volume)
- Separate Music (L) and Voice (R) out (1/8” stereo jack, +/- 2.5 volts peak-to-peak)
	- can use Music-only to a monitor speaker
    - can use Voice-only for hearing assist
- Uses a single 9V battery (no power supply to carry around, doesn't need a nearby power outlet)
	- Amazon Basics Alkaline ($1.50 ea. @ AMZN): about 90 hours
	- Tenergy Lithium ($7.00 ea. @ AMZN): about 150 hours
	- USB-rechargeable Lithium ($7.50 ea. @ AMZN): about 50 hours per charge
- RGB LED indicating power ON, battery level (pulses RED when low), and/or audio output level (VU Meter)
- Low noise: -80dBfs
- Wide 3dB Bandwidth: Music 20Hz-20KHz, Voice 50Hz-20KHz
- Small size: about 6.2”W x 2.75”H x 3.8”D (157 x 70 x 97 mm)
- Light weight (with Alkaline battery): about 12.5oz (355g)
- Open Source Hardware, Software, and Mechanical Design (industry-standard open source licenses) = _anybody_ can build, customize, or service devices

# Files available under Open Source licenses
- SCH = schematic
- GERBER = PC board layout
- BOM = Bill of Materials (specifies which parts to place onto the board)
- CPL = "PickAndPlace" file, specifies where to place parts onto the board
- .xcs = xTool Studio files for the laser-engraved top/bottom/back/front
- .f3dz = compressed Fusion 360 files for enclosure and side panels
- .stl = 3D model for the 3D-printed side panels
- .asc/.asy/.pdf = simulation files for LTspice

# Open Source Licenses applying to all contents of this repository
- Hardware = CERN OHL-S (https://ohwr.org/cern_ohl_s_v2.txt)
- Software/Firmware = GNU General Public License version 3 (https://www.gnu.org/licenses/gpl-3.0.txt)
- Documentation = CC BY-SA License (https://creativecommons.org/licenses/by-sa/4.0/legalcode.txt)

