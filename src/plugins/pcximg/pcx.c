///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Name:        pcximg/pcx.c
// Purpose:     Methods of the PCX Data
// Author:      z64555          ( z1281110@gmail.com )
// Created:     27/01/2016      (DD/MM/YYYY)
// Page width:  120 char
// Tab width:   4 spaces
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "pcx.h"

#include <cstdlib>
#include <inttypes.h>

// Private #defines
#define VGA_MAGIC 0x0C	// Byte value denoting the existance of a 256 color VGA palette.

// Function implementations (Alphabetical)
void pcx_deinit(PCX* img, PCX_info* info) {};

void pcx_init(PCX* img, PCX_info* info) {};

int pcx_has_vga(PCX* img, PCX_info* info)
{
	uint8_t magic;

	if (img->Version < 5) {
		// Was not set, or is not a version that has a VGA palette
		return 0;
	}

	if (info->Buffer_size < (PCX_VGA_PALETTE_SIZE + 1)) {
		// Too small for a palette
		return 0;
	}

	if (img->ImageData == NULL) {
		// ERROR, although we can't do anything about it...
		return 0;
	}

	magic = img->ImageData[info->Buffer_size - (PCX_VGA_PALETTE_SIZE + 1)];

	if (magic != VGA_MAGIC) {
		// No palette
		return 0;
	}

	// Else, we has palette
	return 1;
};