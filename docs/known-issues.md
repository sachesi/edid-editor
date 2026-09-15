# Known issues

CTA-861 support follows CTA-861-H (January 2021). Most of these notes come from wxEDID
and describe gaps and contradictions in the standards and how the editor deals with
them.

1. **CTA-861 data block collection.** The layout of a data block depends on
   combinations of its field values, so changing a single value, even a single bit, can
   change the layout of the whole block. Often more than one value has to change to get
   a valid block of another type: setting the audio format code of a Short Audio
   Descriptor to 15 (Audio Coding Extension Type) is an error until byte 3 also holds a
   valid extension type code. Such blocks are shown as unknown or damaged data. The Add
   menu and the group context menu offer templates for the common LPCM and extended
   audio layouts, which set the format code and the data that depends on it together.

2. **Short Video Descriptors.** Their layout depends on the CTA-861 revision and on the
   VIC. For VICs 1 to 64 bit 7 is the native flag; the VIC field shows the VIC without
   it, and the native flag is a separate switch.

3. **VESA Display Device Data Block, Device Native Pixel Format.** VESA DDDB version 1
   (September 25, 2006), section 2.7, stores 16-bit pixel counts minus one, which gives
   a range of 1 to 65536, yet says the maximum is 65535x65535. The editor uses 1 to
   65536.

4. **CTA-861 Video Data Block.** The decoding pseudo-code treats VICs 193 to 253 as
   valid, while the highest VIC in table 3 (Video Formats) is 219. VICs 220 to 255 are
   reported as reserved.

5. **Type X Video Timing Data Block.** The unit of the vertical refresh rate is not
   stated; Hz is assumed. The range depends on the layout: 0 to 255 Hz when T10_M is 0,
   0 to 1023 Hz when it is 1.

6. **Block lengths in the InfoFrame Data Block** (extended tag 32, section 7.5.9). For a
   Short InfoFrame Descriptor (InfoFrame type other than 0x00 or 0x01) the payload
   length counts from the first byte after the header. For a Short Vendor-Specific
   InfoFrame Descriptor (type 0x01) it leaves out the 3 bytes of the IEEE OUI. The
   length of the header therefore depends on the InfoFrame type.

7. Payload data that CTA-861-H does not define is shown as unknown.

8. **Speaker Location and Speaker Allocation data blocks.** Some speakers of CTA-861-G
   were removed in CTA-861-H.

9. **Vendor-Specific Data Blocks.** The payload is decoded for 00-0C-03 (HDMI
   Licensing, LLC), C4-5D-D8 (HDMI Forum) and 00-00-1A (AMD). The payload of other
   IEEE OUIs is shown as data bytes.

10. **Room Configuration Data Block, speaker location descriptors.** CTA-861-H mostly
    agrees with CTA-861-G, but defines default values for Xmax, Ymax and Zmax, stating,
    apparently in error, that 0x10 (16) corresponds to 32 decimeters.

11. **DisplayID 2.x Adaptive Sync Data Block** (tag 0x2B). Its descriptors are decoded
    as `edid-decode` reads them, which the tests check; bit 4 of the first byte set means
    that changing between refresh rates is not seamless. Other DisplayID 2.x blocks
    without a decoder of their own, such as the Display Parameters, are shown as data
    bytes.
