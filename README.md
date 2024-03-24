# ladybug-mixer
Portable, low-cost audio mixer for square dance callers

<img src="https://github-production-user-asset-6210df.s3.amazonaws.com/6316003/316338628-c6d495b3-b948-4e52-81e3-188ed94bfdf8.png?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIAVCODYLSA53PQK4ZA%2F20240324%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20240324T200714Z&X-Amz-Expires=300&X-Amz-Signature=e15b8aaa5b1080501e38886ad3338c179a74f2bf7f6469c5f90276d2f5b8936e&X-Amz-SignedHeaders=host&actor_id=6316003&key_id=0&repo_id=769083190" width="324">

# Description

The Ladybug LB-102 Mixer is an inexpensive ($50), compact audio mixer design, focused on the needs of beginning and intermediate square dance callers. With the addition of a dynamic microphone ($80), 2 XLR cables ($10 ea.), and a powered speaker like a Alto TS-408 (2000W peak, $250), a caller can provide high-quality sound for 8-10 squares for about $400. 

A single 9 volt battery provides power for months or years of dances, and the Ladybug mixer’s compact size makes it easy to carry.
The entire Ladybug Mixer design is available under an Open Source License, allowing anybody to modify the design to fit their specific needs.

The Version 2 prototype is in active use at my weekly club, and it seems to work great!

# Specs
- 2 Music Inputs (1/8” stereo inputs)
- 2 Combo Mic Inputs (Lo-Z XLR or Hi-Z Hilton™-compatible 1/4“ plug) 
- Remote Volume Control Input (20KΩ Hilton™-compatible 1/8” plug)
- Low-noise Balanced Output (XLR)
- Separate Music (L) and Voice (R) out (1/8” stereo jack)
	- Music-only for a monitor speaker, or use Voice-only for hearing assist
- Uses a single 9V battery
	- Amazon Basics Alkaline ($1.50): 150 hours (est.)
	- Tencent Lithium ($7.00): 300 hours (est.)
	- USB-rechargeable Lithium ($7.50): 100 hours per charge (est.)
- Low noise: -80dBfs
- Wide Bandwidth: Music 20Hz-20KHz, Voice 50Hz-20KHz
- Small size: 6.9”W x 2.5”H x 3.75”D (175 x 65 x 95 mm) 
- Light weight (with battery): 9.1oz (260g)
- Open Source Hardware, Software, and Mechanical Design

# Prototype Version 2 files
- SCH = schematic
- GERBER = PC board layout
- BOM = Bill of Materials (specifies which parts to place onto the board)
- PickAndPlace = specifies where to place parts onto the board

# Licenses applying to all contents of this repository
- Hardware = CERN OHL-S (https://ohwr.org/cern_ohl_s_v2.txt)
- Software/Firmware = GNU General Public License version 3 (https://www.gnu.org/licenses/gpl-3.0.txt)
- Documentation = CC BY-SA License (https://creativecommons.org/licenses/by-sa/4.0/legalcode.txt)

# NOTES
- To keep the schematic symbols looking nice, the schematic uses a small number of parts that have the same PCB footprint as the real parts, but have better symbols.  What gets submitted for PCB fabrication is NOT the schematic, it's the GERBER files, the BOM, and the PickAndPlace file.
