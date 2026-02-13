#ifndef __PREFERENCES_H__
#define __PREFERENCES_H__

#include <stdint.h>
#include <stdbool.h>

// Configuration
#define PREF_START_ADDR                                                        \
  0x0E00 // Last 512-byte sector of 4KB EEPROM (0x0000-0x0FFF)

#define PREF_SECTOR_SIZE 512
// #define PREF_SECTOR_SIZE 8 // for testing SECTOR ERASE, smaller size

#define PREF_DEFAULT_VALUE 0x00

/**
 * @brief User preferences structure.
 * 
 * This union allows access to preferences either as individual bit fields
 * or as a raw byte value for EEPROM storage.
 * 
 * Bit layout (LSB to MSB):
 *   Bit 0: rvc_direction (0=right-handed, 1=left-handed)
 *   Bit 1: vu_meter_disabled (0=enabled, 1=disabled)
 *   Bits 2-6: unused (reserved for future use)
 *   Bit 7: valid_marker (always 0 for valid entries)
 * 
 * NOTE: Default values for all fields are all zeros (0x00).
 *   Bit 7 is 0 to indicate a valid entry.  Entries are written sequentially into the
 *   last sector of EEPROM, with unused bytes set to 0xFF.  Reads will scan backwards to find
 *   the latest valid entry.  Writes will append a new entry, and erase the sector if full.
 *   This design minimizes EEPROM wear by reducing unnecessary writes.  Bits that are set to 0
 *   can only be set back to 1 by performing a sector erase.
 */
typedef union {
  struct {
    uint8_t rvc_direction_pref : 1;  // Bit 0: 0=right-handed [default], 1=left-handed
    uint8_t vu_meter_mode_pref : 2;  // Bits 1-2: 00 = battery monitor [default], 01 = VU meter, 10 = solid white, 11 = reserved
    uint8_t rvc_curve_pref : 1;      // Bit 3: 0=right-handed [default], 1=left-handed
    uint8_t unused : 3;              // Bits 4-6: Reserved for future use
    uint8_t valid_marker : 1;        // Bit 7: Always 0 (marks valid entry)
    // WARNING: changing the bitfield layout might invalidate existing EEPROM data!
    // WARNING: the sum of the bitfield widths must equal 8!
  };
  uint8_t value;  // Raw byte value for EEPROM access
} Preferences_t;

/**
 * @brief Initializes the EEPROM preferences system with default value in slot 0.
 *
 * This function should be called once at system startup to ensure that the preferences
 * storage is initialized. It writes a default Preferences_t value to the first slot  
 * of the designated EEPROM sector. That means setting all bits to 0, including the valid_marker bit.
 * This ensures that there is always a valid entry to read from.
 * 
 */
void Pref_Init();

/**
 * @brief Dumps the first N bytes of the preferences EEPROM sector for debugging.
 *
 * This function can be called anytime after Pref_Init() to output the contents
 * of the preferences EEPROM sector to the UART for debugging purposes.
 * 
 */
void Pref_Dump();


/**
 * @brief Reads the latest user preferences from EEPROM.
 * 
 * Scans the assigned EEPROM sector for the latest valid entry (MSB == 0).
 * If no valid entry is found, returns default values.
 * 
 * @param prefs Pointer to Preferences_t structure to populate.
 */
void Pref_Read(Preferences_t *prefs);

/**
 * @brief Writes new user preferences to EEPROM.
 * 
 * Appends the new value to the next available slot in the sector.
 * If the sector is full, it performs a sector erase and writes to the first slot.
 * Only writes if the value has actually changed.
 * 
 * @param prefs Pointer to Preferences_t structure containing values to save.
 * @return true if the write was successful (or unnecessary), false on failure.
 */
bool Pref_Write(const Preferences_t *prefs);

#endif // __PREFERENCES_H__
