/*
 * Interface to Baofeng UV-B5 and compatibles.
 *
 * Copyright (C) 2013 Serge Vakulenko, KK6ABQ
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The name of the author may not be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "radio.h"
#include "util.h"

#define NCHAN 99

static const int CTCSS_TONES[50] = {
     670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
     948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
    1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
    2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
};

static const int DCS_CODES[104] = {
     23,  25,  26,  31,  32,  36,  43,  47,  51,  53,
     54,  65,  71,  72,  73,  74, 114, 115, 116, 122,
    125, 131, 132, 134, 143, 145, 152, 155, 156, 162,
    165, 172, 174, 205, 212, 223, 225, 226, 243, 244,
    245, 246, 251, 252, 255, 261, 263, 265, 266, 271,
    274, 306, 311, 315, 325, 331, 332, 343, 346, 351,
    356, 364, 365, 371, 411, 412, 413, 423, 431, 432,
    445, 446, 452, 454, 455, 462, 464, 465, 466, 503,
    506, 516, 523, 526, 532, 546, 565, 606, 612, 624,
    627, 631, 632, 654, 662, 664, 703, 712, 723, 731,
    732, 734, 743, 754,
};

static const char CHARSET[] = "0123456789- ABCDEFGHIJKLMNOPQRSTUVWXYZ/_+*";

static const char *PTTID_NAME[] = { "-", "Beg", "End", "Both" };

//static const char *STEP_NAME[] = { "5.0",  "6.25", "10.0", "12.5",
//                                   "20.0", "25.0", "????", "????" };

static const char *SAVER_NAME[] = { "Off", "1", "2", "3", "4", "?5?", "?6?", "?7?" };

static const char *VOX_NAME[] = { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                           "10", "?11?", "?12?", "?13?", "?14?", "?15?" };

static const char *ABR_NAME[] = { "Off", "1", "2", "3", "4", "5", "?6?", "?7?" };

static const char *DTMF_SIDETONE_NAME[] = { "Off", "DTMF Only", "ANI Only", "DTMF+ANI" };

static const char *SCAN_RESUME_NAME[] = { "After Timeout", "When Carrier Off", "Stop On Active", "??" };

static const char *DISPLAY_MODE_NAME[] = { "Channel", "Name", "Frequency", "??" };

static const char *COLOR_NAME[] = { "Off", "Blue", "Orange", "Purple" };

static const char *ALARM_NAME[] = { "Site", "Tone", "Code", "??" };

static const char *RPSTE_NAME[] = { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                             "10", "?11?", "?12?", "?13?", "?14?", "?15?" };

//
// Print a generic information about the device.
//
static void uvb5_print_version (FILE *out)
{
}

//
// Read block of data, up to 16 bytes.
// Halt the program on any error.
//
static void read_block (int fd, int start, unsigned char *data, int nbytes)
{
    unsigned char cmd[4], reply[4];
    int addr, len;

    // Send command.
    cmd[0] = 'R';
    cmd[1] = start >> 8;
    cmd[2] = start;
    cmd[3] = nbytes;
    if (write (fd, cmd, 4) != 4) {
        perror ("Serial port");
        exit (-1);
    }

    // Read reply.
    if (read_with_timeout (fd, reply, 4) != 4) {
        fprintf (stderr, "Radio refused to send block 0x%04x.\n", start);
        exit(-1);
    }
    addr = reply[1] << 8 | reply[2];
    if (reply[0] != 'W' || addr != start || reply[3] != nbytes) {
        fprintf (stderr, "Bad reply for block 0x%04x of %d bytes: %02x-%02x-%02x-%02x\n",
            start, nbytes, reply[0], reply[1], reply[2], reply[3]);
        exit(-1);
    }

    // Read data.
    len = read_with_timeout (fd, data, 0x10);
    if (len != nbytes) {
        fprintf (stderr, "Reading block 0x%04x: got only %d bytes.\n", start, len);
        exit(-1);
    }

    // Get acknowledge.
    if (write (fd, "\x06", 1) != 1) {
        perror ("Serial port");
        exit (-1);
    }
    if (read_with_timeout (fd, reply, 1) != 1) {
        fprintf (stderr, "No acknowledge after block 0x%04x.\n", start);
        exit(-1);
    }
    if (reply[0] != 0x74 && reply[0] != 0x78 && reply[0] != 0x1f) {
        fprintf (stderr, "Bad acknowledge after block 0x%04x: %02x\n", start, reply[0]);
        exit(-1);
    }
    if (verbose) {
        printf ("# Read 0x%04x: ", start);
        print_hex (data, nbytes);
        printf ("\n");
    } else {
        ++radio_progress;
        if (radio_progress % 8 == 0) {
            fprintf (stderr, "#");
            fflush (stderr);
        }
    }
}

//
// Write block of data, up to 16 bytes.
// Halt the program on any error.
//
static void write_block (int fd, int start, const unsigned char *data, int nbytes)
{
    unsigned char cmd[4], reply;

    // Send command.
    cmd[0] = 'W';
    cmd[1] = start >> 8;
    cmd[2] = start;
    cmd[3] = nbytes;
    if (write (fd, cmd, 4) != 4) {
        perror ("Serial port");
        exit (-1);
    }
    if (write (fd, data, nbytes) != nbytes) {
        perror ("Serial port");
        exit (-1);
    }

    // Get acknowledge.
    if (read_with_timeout (fd, &reply, 1) != 1) {
        fprintf (stderr, "No acknowledge after block 0x%04x.\n", start);
        exit(-1);
    }
    if (reply != 0x06) {
        fprintf (stderr, "Bad acknowledge after block 0x%04x: %02x\n", start, reply);
        exit(-1);
    }

    if (verbose) {
        printf ("# Write 0x%04x: ", start);
        print_hex (data, nbytes);
        printf ("\n");
    } else {
        ++radio_progress;
        if (radio_progress % 8 == 0) {
            fprintf (stderr, "#");
            fflush (stderr);
        }
    }
}

//
// Read memory image from the device.
//
static void uvb5_download()
{
    int addr;

    for (addr=0; addr<0x1000; addr+=0x10)
        read_block (radio_port, addr, &radio_mem[addr], 0x10);
}

//
// Write memory image to the device.
//
static void uvb5_upload()
{
    int addr;

    for (addr=0; addr<0x1000; addr+=0x10)
        write_block (radio_port, addr, &radio_mem[addr], 0x10);
}

static void decode_squelch (uint8_t index, int pol, int *ctcs, int *dcs)
{
    if (index == 0) {
        // Squelch disabled.
        return;
    }
    if (index <= 50) {
        // CTCSS value is Hz multiplied by 10.
        *ctcs = CTCSS_TONES[index - 1];
        *dcs = 0;
        return;
    }
    // DCS mode.
    *dcs = DCS_CODES[index - 51];
    if (pol)
        *dcs = - *dcs;
    *ctcs = 0;
}

typedef struct {
    uint32_t    rxfreq;     // binary coded decimal, 8 digits
    uint32_t    txoff;      // binary coded decimal, 8 digits
    uint8_t     _u1       : 3,
                compander : 1,
                rxpol     : 1,
                txpol     : 1,
                _u2       : 2;
    uint8_t     rxtone;
    uint8_t     txtone;
    uint8_t     duplex    : 2,
                revfreq   : 1,
                highpower : 1,
                bcl       : 1,
                isnarrow  : 1,
                scanadd   : 1,
                pttid     : 1;
    uint8_t     _u3 [4];
} memory_channel_t;

static void decode_channel (int i, char *name, int *rx_hz, int *txoff_hz,
    int *rx_ctcs, int *tx_ctcs, int *rx_dcs, int *tx_dcs,
    int *lowpower, int *wide, int *scan, int *pttid, int *bcl,
    int *compander, int *duplex, int *revfreq)
{
    memory_channel_t *ch = i + (memory_channel_t*) radio_mem;

    *rx_hz = *txoff_hz = *rx_ctcs = *tx_ctcs = *rx_dcs = *tx_dcs = 0;
    *lowpower = *wide = *scan = *pttid = *bcl = *compander = 0;
    *duplex = *revfreq = 0;
    if (name)
        *name = 0;
    if (ch->rxfreq == 0 || ch->rxfreq == 0xffffffff)
        return;

    // Extract channel name; strip trailing FF's.
    if (name && i >= 1 && i <= NCHAN) {
        unsigned char *p = (unsigned char*) &radio_mem[0x0A00 + (i-1)*5];
        int n;
        for (n=0; n<5; n++) {
            name[n] = (*p < 42) ? CHARSET[*p++]: 0;
        }
        name[5] = 0;
    }

    // Decode channel frequencies.
    *rx_hz = bcd_to_int (ch->rxfreq) * 10;
    *txoff_hz = bcd_to_int (ch->txoff) * 10;

    // Decode squelch modes.
    decode_squelch (ch->rxtone, ch->rxpol, rx_ctcs, rx_dcs);
    decode_squelch (ch->txtone, ch->txpol, tx_ctcs, tx_dcs);

    // Other parameters.
    *lowpower = ! ch->highpower;
    *wide = ! ch->isnarrow;
    *scan = ch->scanadd;
    *pttid = ch->pttid;
    *bcl = ch->bcl;
    *compander = ch->compander;
    *duplex = ch->duplex;
    *revfreq = ch->revfreq;
}

typedef struct {
    uint8_t     lower_lsb;  // binary coded decimal, 4 digits
    uint8_t     lower_msb;
    uint8_t     upper_lsb;  // binary coded decimal, 4 digits
    uint8_t     upper_msb;
} limits_t;

//
// Looks like limits are not implemented on old firmware
// (prior to version 291).
//
static void decode_limits (char band, int *lower, int *upper)
{
    int offset = (band == 'V') ? 0xF00 : 0xF04;

    limits_t *limits = (limits_t*) (radio_mem + offset);
    *lower = ((limits->lower_msb >> 4) & 15) * 1000 +
             (limits->lower_msb        & 15) * 100 +
             ((limits->lower_lsb >> 4) & 15) * 10 +
             (limits->lower_lsb        & 15);
    *upper = ((limits->upper_msb >> 4) & 15) * 1000 +
             (limits->upper_msb        & 15) * 100 +
             ((limits->upper_lsb >> 4) & 15) * 10 +
             (limits->upper_lsb        & 15);
}

static void fetch_ani (char *ani)
{
    int i;

    for (i=0; i<5; i++)
        ani[i] = "0123456789ABCDEF" [radio_mem[0x0CAA+i] & 0x0f];
}

static void print_offset (FILE *out, int delta)
{
    if (delta == 0) {
        fprintf (out, " 0      ");
    } else {
        if (delta > 0) {
            fprintf (out, "+");;
        } else {
            fprintf (out, "-");;
            delta = - delta;
        }
        if (delta % 1000000 == 0)
            fprintf (out, "%-7u", delta / 1000000);
        else
            fprintf (out, "%-7.3f", delta / 1000000.0);
    }
}

static void print_squelch (FILE *out, int ctcs, int dcs)
{
    if      (ctcs)    fprintf (out, "%5.1f", ctcs / 10.0);
    else if (dcs > 0) fprintf (out, "D%03dN", dcs);
    else if (dcs < 0) fprintf (out, "D%03dI", -dcs);
    else              fprintf (out, "   - ");
}

static void print_vfo (FILE *out, char name, int hz, int offset,
    int rx_ctcs, int tx_ctcs, int rx_dcs, int tx_dcs,
    int lowpower, int wide)
{
    fprintf (out, " %c  %8.4f ", name, hz / 1000000.0);
    print_offset (out, offset);
    fprintf (out, " ");
    print_squelch (out, rx_ctcs, rx_dcs);
    fprintf (out, "   ");
    print_squelch (out, tx_ctcs, tx_dcs);

    fprintf (out, "   %-4s  %-6s\n",
        lowpower ? "Low" : "High", wide ? "Wide" : "Narrow");
}

//
// Generic settings.
//
typedef struct {
    uint8_t squelch;    // Carrier Squelch Level
    uint8_t _u1;
    uint8_t save;       // Battery Saver
    uint8_t vox;        // VOX Sensitivity
    uint8_t _u2;
    uint8_t abr;        // Backlight Timeout
    uint8_t tdr;        // Dual Watch
    uint8_t beep;       // Beep
    uint8_t timeout;    // Timeout Timer
    uint8_t _u3 [4];
    uint8_t voice;      // Voice
    uint8_t _u4;
    uint8_t dtmfst;     // DTMF Sidetone
    uint8_t _u5;
    uint8_t screv;      // Scan Resume
    uint8_t pttid;
    uint8_t pttlt;
    uint8_t mdfa;       // Display Mode (A)
    uint8_t mdfb;       // Display Mode (B)
    uint8_t bcl;        // Busy Channel Lockout
    uint8_t autolk;     // Automatic Key Lock
    uint8_t sftd;
    uint8_t _u6 [3];
    uint8_t wtled;      // Standby LED Color
    uint8_t rxled;      // RX LED Color
    uint8_t txled;      // TX LED Color
    uint8_t almod;      // Alarm Mode
    uint8_t band;
    uint8_t tdrab;      // Dual Watch Priority
    uint8_t ste;        // Squelch Tail Eliminate (HT to HT)
    uint8_t rpste;      // Squelch Tail Eliminate (repeater)
    uint8_t rptrl;      // STE Repeater Delay
    uint8_t ponmsg;     // Power-On Message
    uint8_t roger;      // Roger Beep
} settings_t;

//
// Print full information about the device configuration.
//
static void uvb5_print_config (FILE *out)
{
    int i;

    // Print memory channels.
    fprintf (out, "\n");
    fprintf (out, "Channel Name  Receive  TxOffset R-Squel T-Squel Power FM     Scan PTTID\n");
    for (i=1; i<=NCHAN; i++) {
        int rx_hz, txoff_hz, rx_ctcs, tx_ctcs, rx_dcs, tx_dcs;
        int lowpower, wide, scan, pttid;
        int bcl, compander, duplex, revfreq;
        char name[17];

        decode_channel (i, name, &rx_hz, &txoff_hz, &rx_ctcs, &tx_ctcs,
            &rx_dcs, &tx_dcs, &lowpower, &wide, &scan, &pttid,
            &bcl, &compander, &duplex, &revfreq);

        if (rx_hz == 0) {
            // Channel is disabled
            continue;
        }

        fprintf (out, "%5d   %-5s %8.4f ", i, name, rx_hz / 1000000.0);
        print_offset (out, txoff_hz);
        fprintf (out, " ");
        print_squelch (out, rx_ctcs, rx_dcs);
        fprintf (out, "   ");
        print_squelch (out, tx_ctcs, tx_dcs);

        fprintf (out, "   %-4s  %-6s %-4s %-4s\n", lowpower ? "Low" : "High",
            wide ? "Wide" : "Narrow", scan ? "+" : "-", PTTID_NAME[pttid]);
        // TODO: bcl, compander, duplex, revfreq
    }

    // Print frequency mode VFO settings.
    int hz, offset, rx_ctcs, tx_ctcs, rx_dcs, tx_dcs;
    int lowpower, wide, scan, pttid;
    int bcl, compander, duplex, revfreq;;
    fprintf (out, "\n");

    // TODO: scan, pttid, bcl, compander, duplex, revfreq
    decode_channel (0, 0, &hz, &offset, &rx_ctcs, &tx_ctcs,
        &rx_dcs, &tx_dcs, &lowpower, &wide, &scan, &pttid,
        &bcl, &compander, &duplex, &revfreq);
    fprintf (out, "VFO Receive  TxOffset R-Squel T-Squel Power FM\n");
    print_vfo (out, 'A', hz, offset, rx_ctcs, tx_ctcs,
        rx_dcs, tx_dcs, lowpower, wide);
    decode_channel (130, 0, &hz, &offset, &rx_ctcs, &tx_ctcs,
        &rx_dcs, &tx_dcs, &lowpower, &wide, &scan, &pttid,
        &bcl, &compander, &duplex, &revfreq);
    print_vfo (out, 'B', hz, offset, rx_ctcs, tx_ctcs,
        rx_dcs, tx_dcs, lowpower, wide);

    // Print band limits.
    int vhf_lower, vhf_upper, uhf_lower, uhf_upper;
    decode_limits ('V', &vhf_lower, &vhf_upper);
    decode_limits ('U', &uhf_lower, &uhf_upper);
    fprintf (out, "\n");
    fprintf (out, "Limit Lower  Upper \n");
    fprintf (out, " VHF  %5.1f  %5.1f\n", vhf_lower/10.0, vhf_upper/10.0);
    fprintf (out, " UHF  %5.1f  %5.1f\n", uhf_lower/10.0, uhf_upper/10.0);

    // TODO
    return;

    // Get atomatic number identifier.
    char ani[5];
    fetch_ani (ani);

    // Print other settings.
    settings_t *mode = (settings_t*) &radio_mem[0x0E20];
    fprintf (out, "Carrier Squelch Level: %u\n", mode->squelch);
    fprintf (out, "Battery Saver: %s\n", SAVER_NAME[mode->save & 7]);
    fprintf (out, "VOX Sensitivity: %s\n", VOX_NAME[mode->vox & 15]);
    fprintf (out, "Backlight Timeout: %s\n", ABR_NAME[mode->abr & 7]);
    fprintf (out, "Dual Watch: %s\n", mode->tdr ? "On" : "Off");
    fprintf (out, "Keypad Beep: %s\n", mode->beep ? "On" : "Off");
    fprintf (out, "Transmittion Timer: %u\n", (mode->timeout + 1) * 15);
    fprintf (out, "Voice Prompt: %s\n", mode->voice ? "On" : "Off");
    fprintf (out, "Automatic ID[1-5]: %c%c%c%c%c\n", ani[0], ani[1], ani[2], ani[3], ani[4]);
    fprintf (out, "DTMF Sidetone: %s\n", DTMF_SIDETONE_NAME[mode->dtmfst & 3]);
    fprintf (out, "Scan Resume Method: %s\n", SCAN_RESUME_NAME[mode->screv & 3]);
    fprintf (out, "Display Mode A: %s\n", DISPLAY_MODE_NAME[mode->mdfa & 3]);
    fprintf (out, "Display Mode B: %s\n", DISPLAY_MODE_NAME[mode->mdfb & 3]);
    fprintf (out, "Busy Channel Lockout: %s\n", mode->bcl ? "On" : "Off");
    fprintf (out, "Auto Key Lock: %s\n", mode->autolk ? "On" : "Off");
    fprintf (out, "Standby LED Color: %s\n", COLOR_NAME[mode->wtled & 3]);
    fprintf (out, "RX LED Color: %s\n", COLOR_NAME[mode->rxled & 3]);
    fprintf (out, "TX LED Color: %s\n", COLOR_NAME[mode->txled & 3]);
    fprintf (out, "Alarm Mode: %s\n", ALARM_NAME[mode->almod & 3]);
    fprintf (out, "Squelch Tail Eliminate: %s\n", mode->ste ? "On" : "Off");
    fprintf (out, "Squelch Tail Eliminate for Repeater: %s\n", RPSTE_NAME[mode->rpste & 15]);
    fprintf (out, "Squelch Tail Repeater Delay: %s\n", RPSTE_NAME[mode->rptrl & 15]);
    fprintf (out, "Power-On Message: %s\n", mode->ponmsg ? "On" : "Off");
    fprintf (out, "Roger Beep: %s\n", mode->roger ? "On" : "Off");
}

//
// Read memory image from the binary file.
//
static void uvb5_read_image (FILE *img, unsigned char *ident)
{
    char buf[40];

    if (fread (ident, 1, 8, img) != 8) {
        fprintf (stderr, "Error reading image header.\n");
        exit (-1);
    }
    // Ignore next 40 bytes.
    if (fread (buf, 1, 40, img) != 40) {
        fprintf (stderr, "Error reading header.\n");
        exit (-1);
    }
    if (fread (&radio_mem[0], 1, 0x1000, img) != 0x1000) {
        fprintf (stderr, "Error reading image data.\n");
        exit (-1);
    }
}

//
// Save memory image to the binary file.
// Try to be compatible with Chirp.
//
static void uvb5_save_image (FILE *img)
{
    fwrite (radio_ident, 1, 8, img);
    fwrite ("Radio Program data v1.08\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 1, 40, img);
    fwrite (&radio_mem[0], 1, 0x1000, img);
}

static void uvb5_parse_parameter (char *param, char *value)
{
    fprintf (stderr, "TODO: Parse parameter for UV-B5.\n");
    exit(-1);
}

static int uvb5_parse_header (char *line)
{
    fprintf (stderr, "TODO: Parse table header for UV-B5.\n");
    exit(-1);
}

static int uvb5_parse_row (int table_id, int first_row, char *line)
{
    fprintf (stderr, "TODO: Parse table row for UV-B5.\n");
    exit(-1);
}

//
// Baofeng UV-B5, UV-B6
//
radio_device_t radio_uvb5 = {
    "Baofeng UV-B5",
    uvb5_download,
    uvb5_upload,
    uvb5_read_image,
    uvb5_save_image,
    uvb5_print_version,
    uvb5_print_config,
    uvb5_parse_parameter,
    uvb5_parse_header,
    uvb5_parse_row,
};