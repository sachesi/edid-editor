/***************************************************************
 * Name:      EDID_text.h
 * Purpose:   EDID hexadecimal text import/export and text reports
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_TEXT_H
#define EDID_TEXT_H 1

#include <string>
#include <vector>

#include "EDID_class.h"

//Decode hexadecimal EDID text. Bytes are read as pairs of hex digits;
//whitespace, commas, semicolons, and brackets separate tokens, "0x"
//prefixes are dropped, and tokens ending in ':' are skipped as offset
//labels. On failure, error describes the first rejected token.
bool edid_hex_decode(const char* text, size_t length,
                     std::vector<u8_t>& bytes, std::string& error);

//Encode bytes as upper-case hex, 16 bytes per line, with a blank line
//after each 128-byte block.
std::string edid_hex_encode(const u8_t* data, size_t size);

//Describe every parsed group, field, value, and unit of the document,
//followed by the raw data in hex. Unparsed extension blocks appear as
//hex only.
std::string edid_text_report(EDID_cl& EDID, const char* source,
                             const char* version);

#endif
