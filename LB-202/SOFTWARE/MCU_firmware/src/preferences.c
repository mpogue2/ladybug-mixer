#include "globals.h"

#include "preferences.h"
#include "fw_hal.h"

#ifdef INCLUDE_PREFERENCES

// State
static uint16_t s_next_write_offset = 0;
static bool s_initialized = false;

// =========================================================
// Helper to read a byte
static uint8_t ReadByte(uint16_t addr) {
  IAP_SetEnabled(HAL_State_ON);

  IAP_CmdRead(addr);
  if (IAP_IsCmdFailed()) {
    IAP_ClearCmdFailFlag();
#ifdef DEBUG
    // UART1_TxString("ReadByte: IAP Read Failed at ");
    // UART1_TxHex(addr >> 8);
    // UART1_TxHex(addr & 0xFF);
    // UART1_TxString("\r\n");
#endif    
    IAP_SetEnabled(HAL_State_OFF);
    return 0xFF; // Return 0xFF on failure (erased state)
  }
  uint8_t data = IAP_ReadData();
  IAP_SetEnabled(HAL_State_OFF);
  return data;
}

// =========================================================
void Pref_Dump() {
#ifdef DEBUG
  IAP_SetEnabled(HAL_State_ON);
  UART1_TxString("Preferences EEPROM Dump:\r\n");

  // dump preferences for debug
#define NUM_DUMP_ROWS 8  
  for (uint8_t i = 0; i < NUM_DUMP_ROWS; i++) {
    for (uint8_t j = 0; j < 16; j++) {
      uint16_t addr = PREF_START_ADDR + (i << 4) + j;
      if (j == 0) {
        UART1_TxHex(addr >> 8);
        UART1_TxHex(addr & 0xFF);
        UART1_TxChar(':');
      }
      UART1_TxHex(ReadByte(addr));
      if (j < 15) {
        UART1_TxChar(' ');
      } else {
        UART1_TxString("\r\n");
      }
    }
  }
    IAP_SetEnabled(HAL_State_OFF);
#endif
}

// =========================================================
void Pref_Init() {
  IAP_SetEnabled(HAL_State_ON);

  // Write default value to first slot
  IAP_WriteData(PREF_DEFAULT_VALUE); // must have bit 7 = 0 (indicates valid entry)
  IAP_CmdWrite(PREF_START_ADDR);

  if (IAP_IsCmdFailed()) {
    IAP_ClearCmdFailFlag();
#ifdef DEBUG
    // UART1_TxString("IAP Write Failed during Pref_Init\r\n");
#endif  
  }

  s_next_write_offset = 1; // Next write goes to slot 1
  s_initialized = true; 
  IAP_SetEnabled(HAL_State_OFF);
}

// =========================================================
void Pref_Read(Preferences_t *prefs) {
  uint16_t i;
  Preferences_t temp;

  IAP_SetEnabled(HAL_State_ON);

  // Scan backwards to find the latest valid entry (valid_marker == 0)
  // We start from the last byte of the sector
  #ifdef DEBUG
    // UART1_TxString("SCANNING FOR PREFS...\r\n");
  #endif
  for (i = PREF_SECTOR_SIZE; i > 0; i--) {
    uint16_t addr = PREF_START_ADDR + (i - 1);
    temp.value = ReadByte(addr);
#ifdef DEBUG
    // UART1_TxHex(addr >> 8);
    // UART1_TxHex(addr & 0xFF);
    // UART1_TxChar(':');
    // UART1_TxHex(temp.value);
    // UART1_TxString(" vm:");
    // UART1_TxHex(temp.valid_marker);
    // UART1_TxString("\r\n");
#endif
    // Check if valid_marker bit is 0 (valid entry)
    if (temp.valid_marker == 0) {
      s_next_write_offset = i; // Next write goes after this one
      s_initialized = true;
      IAP_SetEnabled(HAL_State_OFF);
      *prefs = temp;
#ifdef DEBUG
      // UART1_TxString("returning...\r\n");
#endif
      return;
    }
  }

  // No valid entry found, sector is empty or invalid
#ifdef DEBUG
  // UART1_TxString("sector empty/invalid...\r\n");
#endif
  s_next_write_offset = 0;
  s_initialized = true;
  IAP_SetEnabled(HAL_State_OFF);

  prefs->value = PREF_DEFAULT_VALUE;
}

// =========================================================
bool Pref_Write(const Preferences_t *prefs) {
  Preferences_t new_prefs;
  Preferences_t current_prefs;
  bool result;
  Preferences_t temp_prefs;

  // Copy and ensure valid_marker is 0
  new_prefs = *prefs;
  new_prefs.valid_marker = 0;

  // Initialize if needed (to find s_next_write_offset)
  if (!s_initialized) {
    Pref_Read(&temp_prefs);
  }

  IAP_SetEnabled(HAL_State_ON);

#ifdef DEBUG
  // UART1_TxString("Pref_Write: next_offset=");
  // UART1_TxHex(s_next_write_offset);
  // UART1_TxString(", new_value=");
  // UART1_TxHex(new_prefs.value);
  // UART1_TxString("\r\n");
#endif  

  // Check current value to avoid unnecessary writes
  if (s_next_write_offset > 0) {
    current_prefs.value = ReadByte(PREF_START_ADDR + s_next_write_offset - 1);
    current_prefs.valid_marker = 0;  // Ensure it's treated as valid
  } else {
    current_prefs.value = PREF_DEFAULT_VALUE;
  }

  if (current_prefs.value == new_prefs.value) {
#ifdef DEBUG
    // UART1_TxString("Pref_Write: value unchanged, no write needed.\r\n");
#endif    
    result = true; // Nothing to do
    goto cleanup;
  }

  // Check if sector is full
  if (s_next_write_offset >= PREF_SECTOR_SIZE) {
#ifdef DEBUG
    // UART1_TxString("Pref_Write: sector full, erasing...\r\n");
#endif
    // *** ADD THIS: Cycle IAP after erase ***
    IAP_SetEnabled(HAL_State_OFF);
    IAP_SetEnabled(HAL_State_ON);

    IAP_CmdErase(PREF_START_ADDR);
    if (IAP_IsCmdFailed()) {
      #ifdef DEBUG
        // UART1_TxString("Pref_Write: IAP Erase Failed\r\n");
      #endif
      IAP_ClearCmdFailFlag();
      result = false;
      goto cleanup;
    }

    // *** ADD THIS: Cycle IAP after erase ***
    IAP_SetEnabled(HAL_State_OFF);
    IAP_SetEnabled(HAL_State_ON);

    s_next_write_offset = 0;
  }

  // Re-enable IAP before write (ReadByte disabled it)
  IAP_SetEnabled(HAL_State_OFF);
  IAP_SetEnabled(HAL_State_ON);

  // Write new value
  IAP_WriteData(new_prefs.value);
  IAP_CmdWrite(PREF_START_ADDR + s_next_write_offset);

  if (IAP_IsCmdFailed()) {
#ifdef DEBUG
    // UART1_TxString("Pref_Write: IAP WriteData Failed\r\n");
#endif    
    IAP_ClearCmdFailFlag();
    result = false;
    goto cleanup;
  }

  s_next_write_offset++;
  result = true;

cleanup:
  IAP_SetEnabled(HAL_State_OFF);

  Pref_Dump(); // debug dump

  return result;
}

#endif // INCLUDE_PREFERENCES
