/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2017 Matthew Howkins

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint32_t	native_scancode;	// Native Scancode (X11)
	uint8_t		set_2[8];		// PS/2 Set 2 make code
} KeyMapInfo;

static const KeyMapInfo key_map[] = {
	{ 0x45, { 0x76 } },		// Escape
	{ 0x01, { 0x16 } },		// 1
	{ 0x02, { 0x1e } },		// 2
	{0x03, { 0x26 } },		// 3
	{ 0x04, { 0x25 } },		// 4
	{ 0x05, { 0x2e } },		// 5
	{ 0x06, { 0x36 } },		// 6
	{ 0x07, { 0x3d } },		// 7
	{ 0x08, { 0x3e } },		// 8
	{ 0x09, { 0x46 } },		// 9
	{ 0x0a, { 0x45 } },		// 0
	{ 0x0b, { 0x4e } },		// -
	{ 0x0c, { 0x55 } },		// =
	{ 0x41, { 0x66 } },		// Backspace

	{ 0x42, { 0x0d } },		// Tab
	{ 0x10, { 0x15 } },		// Q
	{ 0x11, { 0x1d } },		// W
	{ 0x12, { 0x24 } },		// E
	{ 0x13, { 0x2d } },		// R
	{ 0x14, { 0x2c } },		// T
	{ 0x15, { 0x35 } },		// Y
	{ 0x16, { 0x3c } },		// U
	{ 0x17, { 0x43 } },		// I
	{ 0x18, { 0x44 } },		// O
	{ 0x19, { 0x4d } },		// P
	{ 0x1a, { 0x54 } },		// [
	{ 0x1b, { 0x5b } },		// ]
	{ 0x43, { 0x5a } },		// Return
	{ 0x44, { 0x5a } },		// Return
	{ 0x63, { 0x14 } },		// Left Ctrl
	{ 0x20, { 0x1c } },		// A
	{ 0x21, { 0x1b } },		// S
	{ 0x22, { 0x23 } },		// D
	{ 0x23, { 0x2b } },		// F
	{ 0x24, { 0x34 } },		// G
	{ 0x25, { 0x33 } },		// H
	{ 0x26, { 0x3b } },		// J
	{ 0x27, { 0x42 } },		// K
	{ 0x28, { 0x4b } },		// L
	{ 0x29, { 0x4c } },		// ;
	{ 0x2a, { 0x52 } },		// '
	{ 0x2b, { 0x0e } },		// `

	{ 0x60, { 0x12 } },		// Left Shift
	{ 0x31, { 0x1a } },		// Z
	{ 0x32, { 0x22 } },		// X
	{ 0x33, { 0x21 } },		// C
	{ 0x34, { 0x2a } },		// V
	{ 0x35, { 0x32 } },		// B
	{ 0x36, { 0x31 } },		// N
	{ 0x37, { 0x3a } },		// M
	{ 0x38, { 0x41 } },		// ,
	{ 0x39, { 0x49 } },		// .
	{53, { 0x4a } },		// /
	{ 0x61, { 0x59 } },		// Right Shift
	{ 0x5d, { 0x7c } },		// Keypad *

	{ 0x64, { 0x11 } },		// Left Alt
	{ 0x40, { 0x29 } },		// Space
	{ 0x62, { 0x58 } },		// Caps Lock

	{ 0x50, { 0x05 } },		// F1
	{ 0x51, { 0x06 } },		// F2
	{ 0x52, { 0x04 } },		// F3
	{ 0x53, { 0x0c } },		// F4
	{ 0x54, { 0x03 } },		// F5
	{ 0x55, { 0x0b } },		// F6
	{ 0x56, { 0x83 } },		// F7
	{ 0x57, { 0x0a } },		// F8
	{ 0x58, { 0x01 } },		// F9
//	{ 68, { 0x09 } },		F10
#if 0
	{ 69, { 0x77 } },		// Keypad Num Lock
	{ 70, { 0x7e } },		// Scroll Lock
	{ 71, { 0x6c } },		// Keypad 7
	{ 72, { 0x75 } },		// Keypad 8
	{ 73, { 0x7d } },		// Keypad 9
	{ 74, { 0x7b } },		// Keypad -
	{ 75, { 0x6b } },		// Keypad 4
	{ 76, { 0x73 } },		// Keypad 5
	{ 77, { 0x74 } },		// Keypad 6
	{ 78, { 0x79 } },		// Keypad +
	{ 79, { 0x69 } },		// Keypad 1
	{ 80, { 0x72 } },		// Keypad 2
	{ 81, { 0x7a } },		// Keypad 3
	{ 82, { 0x70 } },		// Keypad 0
	{ 83, { 0x71 } },		// Keypad .

	{ 87, { 0x78 } },		// F11
#endif
	
	{ 0x59, { 0x07 } },		// F12
#if 0
	{ 96, { 0xe0, 0x5a } },	// Keypad Enter
	{ 97, { 0xe0, 0x14 } },	// Right Ctrl
	{ 98, { 0xe0, 0x4a } },	// Keypad /

	{ 102, { 0xe0, 0x6c } },	// Home
	{ 103, { 0xe0, 0x75 } },	// Up
	{ 104, { 0xe0, 0x7d } },	// Page Up
	{ 105, { 0xe0, 0x6b } },	// Left
	{ 106, { 0xe0, 0x74 } },	// Right
	{ 107, { 0xe0, 0x69 } },	// End
	{ 108, { 0xe0, 0x72 } },	// Down
	{ 109, { 0xe0, 0x7a } },	// Page Down
	{ 110, { 0xe0, 0x70 } },	// Insert
	{ 111, { 0xe0, 0x71 } },	// Delete
	{ 119, { 0xe1, 0x14, 0x77, 0xe1, 0xf0, 0x14, 0xf0, 0x77 } },	// Break

	{ 125, { 0xe0, 0x1f } },	// Left Win
	{ 126, { 0xe0, 0x27 } },	// Right Win
	{ 155, { 0xe0, 0x2f } },	// Appication (Win Menu)
	#endif
	{ 0, { 0, 0 } },
};

const uint8_t *
keyboard_map_key(uint32_t native_scancode)
{
	size_t k;

	for (k = 0; key_map[k].native_scancode != 0; k++) {
		if (key_map[k].native_scancode == native_scancode) {
			
			return key_map[k].set_2;
		}
	}
	return NULL;
}
