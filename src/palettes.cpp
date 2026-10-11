// Copyright (c) 2017 @jake-b, @russp81, @toblum
// Griswold LED Lighting Controller

// Griswold is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as 
// published by the Free Software Foundation, either version 3 of 
// the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Griswold v3. v2 was the async rewrite of Griswold 1.0.

// Griswold-v1 is a fork of the LEDLAMP project at 
//        https://github.com/russp81/LEDLAMP_FASTLEDs

// The LEDLAMP project is a fork of the McLighting Project at
//        https://github.com/toblum/McLighting

#include <FS.h>
#include "globals.h"
//int paletteCount = 0;

int getPaletteCount() {
  Dir dir = SPIFFS.openDir("/palettes");
  int palette_count = 0;
  while (dir.next()) {
    palette_count++;
  }
  return palette_count;
}

bool openPaletteFileWithIndex(int index, File* file) {
  Dir dir = SPIFFS.openDir("/palettes");

  int palette_count = 0;
  while (dir.next()) {
    if (palette_count == index) break;
    palette_count++;
  }
  
  if (palette_count != index) {
    Console.println("Error, unable to open palette");
    return false;
  }

  *file = dir.openFile("r");
  return true; //success
}

bool loadPaletteFromFile(int index, CRGBPalette16* palette) {
  File paletteFile;
  if (!openPaletteFileWithIndex(index, &paletteFile)) {
      Console.printf("Error loading paletteFile at index %d\n", index);
      return false;
  }

  int paletteFileSize = paletteFile.size();
  if (paletteFileSize == 0) {
    Console.println("Unable to load pallete of zero file size.");
    return false;
  }
  uint8_t* bytes = new uint8_t[paletteFileSize];
  if (!bytes) {
    Console.println("Unable to allocate memory for palette");
    return false;
  }

  paletteFile.readBytes((char*)bytes, paletteFileSize);  
  Console.printf("Load palette named %s (%d bytes)\n", paletteFile.name(), paletteFileSize);
  paletteFile.close();

  palette->loadDynamicGradientPalette(bytes);

  delete[] bytes;
  return true;
}


String getPaletteNameWithIndex(int index) {
  Dir dir = SPIFFS.openDir("/palettes");

  int ndx = 0;
  while (dir.next()) {
    if (ndx == index) return dir.fileName();
    ndx++;
  }
  return "[unknown]";
}

//void sendPalletList(ESP8266WebServer* server) {
//  // manually send header
//  server->sendContent_P(PSTR("HTTP/1.0 200 OK\n"));
//  server->sendContent_P(PSTR("Content-Type: text/html\n"));
//  server->sendContent_P(PSTR("Connection: close\n"));
//  server->sendContent_P(PSTR("\n"));
//
//  // Open the palettes directory
//  Dir dir = SPIFFS.openDir("/palettes");
//
//  // Loop through the palettes
//  int ndx = 0;
//  Console.println("StartLoop");
//  while (dir.next()) {
//    Console.println("Item in loop");
//    server->sendContent_P(PSTR("<a href='/palette_anims?p="));
//    server->sendContent(String(ndx, DEC));
//    server->sendContent_P(PSTR("'>"));
//    server->sendContent(dir.fileName());
//    server->sendContent_P(PSTR("</a><br/>"));
//    ndx++;
//  }
//}
