#!/usr/bin/env python3
"""
Plot RVC (Remote Volume Control) attenuation curves for both modes.
Faithfully models the integer arithmetic from the firmware in src/main.c.
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

# Traditional MA-220 lookup table (from firmware)
atten_lookup = [
    -12, -12, -12, -12, -12, -12, -12, -12, -12, -12,
    -12, -12, -12, -12, -12, -12, -12, -12, -11, -11,
    -11, -11, -11, -11, -11, -11, -11, -11, -11, -11,
    -11, -11, -11, -11, -11, -11, -11, -11, -11, -11,
    -11, -11, -10, -10, -10, -10, -10, -10, -10, -10,
    -10, -10, -10, -10, -10, -10, -10,  -9,  -9,  -9,
     -9,  -9,  -9,  -9,  -9,  -9,  -9,  -9,  -8,  -8,
     -8,  -8,  -8,  -8,  -8,  -8,  -7,  -7,  -7,  -7,
     -7,  -7,  -7,  -6,  -6,  -6,  -6,  -6,  -6,  -6,
     -6,  -5,  -5,  -5,  -5,  -5,  -5,  -5,  -5,  -5,
     -4,  -4,  -4,  -4,  -4,  -3,  -3,  -3,  -3,  -3,
     -3,  -2,  -2,  -2,  -2,  -2,  -2,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,   0,   0,
      0,   0,   0,   0
]

positions = []
trad_atten = []
default_atten = []

for RVCval in range(134):
    # Position mapping: CW (position 1.0) = loud = RVCval 0
    #                   CCW (position 0.0) = quiet = RVCval 133
    position = 1.0 - RVCval / 133.0
    positions.append(position)

    # --- Traditional MA-220 Mode ---
    rvc_reversed = 0 if RVCval >= 133 else (133 - RVCval)
    trad_res = -atten_lookup[rvc_reversed]  # positive attenuation
    trad_atten.append(-trad_res)            # store as negative dB

    # --- Default (Ladybug) Mode with Mute ---
    # Linearize the audio-taper pot: linearPotVal = (RVCval * 255) / (255 - RVCval)
    top = (RVCval << 8) - RVCval   # RVCval * 255 (matches firmware bit shift)
    bot = 255 - RVCval
    if bot == 0:
        linearPotVal = 255
    else:
        linearPotVal = top // bot  # integer division (firmware uses SW divide)

    if linearPotVal > 255:
        linearPotVal = 255

    # Piecewise attenuation curve
    A = 3 * 64   # 192 - knee at 3/4 of linearized travel
    B = 14        # attenuation at knee (dB)
    C = 48        # attenuation at end of active range (dB)
    D = 236       # end guard band threshold
    E = 6         # start guard band threshold

    if linearPotVal < E:
        res = 0                                               # guard band: no atten
    elif linearPotVal > D:
        res = 64                                              # MUTE
    elif linearPotVal <= A:
        res = ((linearPotVal - E) * B) // A                   # gentle slope
    else:
        res = B + ((linearPotVal - A) * (C - B)) // (D - A)   # steep slope to mute

    default_atten.append(-res)

# Sort by position (ascending) for clean plotting
data = sorted(zip(positions, trad_atten, default_atten))
positions     = [d[0] for d in data]
trad_atten    = [d[1] for d in data]
default_atten = [d[2] for d in data]

# --- Publication-quality plot ---
plt.rcParams.update({
    'font.family': 'sans-serif',
    'font.size': 12,
    'axes.linewidth': 1.2,
    'xtick.major.width': 1.0,
    'ytick.major.width': 1.0,
    'xtick.minor.width': 0.6,
    'ytick.minor.width': 0.6,
})

fig, ax = plt.subplots(figsize=(10.24, 8.0), dpi=100)

# Shaded MUTE region
ax.axhspan(-68, -63, color='#FFE0E0', zorder=1)
ax.text(0.50, -65.5, 'MUTE', fontsize=11, color='#C0392B', fontweight='bold',
        ha='center', va='center', zorder=6)

# Plot curves
ax.plot(positions, default_atten, color='#E63946', linewidth=2.5,
        label='Ladybug Mode (with Mute)', zorder=5)
ax.plot(positions, trad_atten, color='#1D3557', linewidth=2.5,
        label='Traditional MA-220 Mode', zorder=5)

# Reference lines
ax.axhline(y=0,   color='gray', linewidth=0.8, linestyle='--', alpha=0.5, zorder=2)
ax.axhline(y=-12, color='#1D3557', linewidth=0.6, linestyle=':', alpha=0.4, zorder=2)
ax.axhline(y=-14, color='#E63946', linewidth=0.6, linestyle=':', alpha=0.4, zorder=2)

# Labels
ax.set_xlabel('RVC Position   (0.0 = Fully CCW  \u2192  1.0 = Fully CW)',
              fontsize=14, fontweight='bold', labelpad=10)
ax.set_ylabel('Attenuation (dB)', fontsize=14, fontweight='bold', labelpad=10)
ax.set_title('Remote Volume Control\nAttenuation vs. Knob Position',
             fontsize=16, fontweight='bold', pad=18)

# Axis limits and ticks
ax.set_xlim(-0.02, 1.02)
ax.set_ylim(-68, 2)
ax.xaxis.set_major_locator(ticker.MultipleLocator(0.1))
ax.xaxis.set_minor_locator(ticker.MultipleLocator(0.05))
ax.yaxis.set_major_locator(ticker.MultipleLocator(10))
ax.yaxis.set_minor_locator(ticker.MultipleLocator(5))

# Grid
ax.grid(which='major', alpha=0.30, linestyle='-', linewidth=0.8)
ax.grid(which='minor', alpha=0.12, linestyle='-', linewidth=0.5)

# Annotations
# Ladybug knee point (linearPotVal = 192 => RVCval ~ 110 => position ~ 0.17)
knee_pos = 1.0 - 110/133.0
ax.annotate('Knee: \u221214 dB\n(3/4 of pot travel)',
            xy=(knee_pos, -13), fontsize=10, color='#E63946',
            xytext=(0.30, -24),
            arrowprops=dict(arrowstyle='->', color='#E63946', lw=1.5),
            fontweight='bold', ha='center')

# Mute threshold
ax.annotate('Mute threshold\n(\u221248 dB \u2192 \u221264 dB)',
            xy=(0.065, -50), fontsize=10, color='#C0392B',
            xytext=(0.20, -54),
            arrowprops=dict(arrowstyle='->', color='#C0392B', lw=1.5),
            fontweight='bold', ha='center')

# Traditional max attenuation
ax.annotate('Max: \u221212 dB',
            xy=(0.0, -12), fontsize=10, color='#1D3557',
            xytext=(0.12, -6),
            arrowprops=dict(arrowstyle='->', color='#1D3557', lw=1.5),
            fontweight='bold', ha='center')

# 0 dB at CW
ax.annotate('0 dB (both modes)',
            xy=(0.96, 0), fontsize=10, color='#555555',
            xytext=(0.80, -8),
            arrowprops=dict(arrowstyle='->', color='#555555', lw=1.2),
            ha='center')

# Legend
ax.legend(fontsize=12, loc='center left', framealpha=0.95,
          edgecolor='#CCCCCC', fancybox=True, shadow=False,
          bbox_to_anchor=(0.35, 0.45))

plt.tight_layout()
plt.savefig('/Users/mpogue/____MIXER_PROJECT/___NEXT GENERATION/mainboard1/rvc_curves.png',
            dpi=100, bbox_inches='tight', facecolor='white')
print("Saved: rvc_curves.png (1024x800)")
plt.close()
