// main/ppt_keys.h — HID Usage IDs for the PPT remote.
//
// The numeric values come from the USB HID Usage Tables (Keyboard/Keypad
// page 0x07) and are required by esp_hid_dev_input_set() to construct a
// keyboard report. Modifier bit masks come from the same spec.
#pragma once

#include <stdint.h>

#define HID_KEY_LEFT_ARROW   0x50u
#define HID_KEY_RIGHT_ARROW  0x4Fu
#define HID_KEY_ESCAPE       0x29u
#define HID_KEY_F5           0x3Eu
#define HID_KEY_RETURN       0x28u
#define HID_KEY_P            0x13u

#define HID_MOD_LEFT_SHIFT   0x02u
#define HID_MOD_LEFT_ALT     0x04u
#define HID_MOD_LEFT_GUI     0x08u
