/* rom_browser.h — full-screen file browser overlay for loading ROMs/disks/xex.

   Activated by Fn+L (KM_ACT_LOAD_XEX) via input action handler. While open,
   the main loop suspends Atari core stepping and routes keyboard polling
   here. Selecting a file calls AFILE_OpenFile(), which auto-detects format
   from magic bytes and dispatches to BINLOAD_Loader / SIO_Mount /
   CARTRIDGE_Insert / CASSETTE_Insert with reboot=1.

   Default open dir: walks up from "/sd/atari800/roms" to the deepest
   existing prefix. Last-used dir is remembered in RAM until reboot. */

#pragma once

namespace rom_browser {

/* Open the browser. Reads SD, draws the list. Idempotent. */
void open();

/* Close the browser without loading anything. */
void close();

/* True while the browser owns the screen + keyboard. Main loop polls
   this every iteration to decide whether to step the Atari core. */
bool is_open();

/* Pump keyboard + redraw if needed. Call from main loop while is_open(). */
void poll();

} /* namespace rom_browser */
