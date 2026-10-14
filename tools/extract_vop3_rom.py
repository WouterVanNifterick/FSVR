#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Wouter van Nifterick
"""Write rom_tables.h and rom_tables.cpp: the tables of the FS1R v1.20 EPROM that the two hosts read.

    python tools/extract_vop3_rom.py <eprom.bin>      the 2 MB image, CPU byte order

Every table is named here with its address, element type and length; the C++ sees only the names.
"""
import hashlib
import struct
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "src/fs1r/firmware/vop3host"
BASE = 0x200000                               # where the EPROM sits in the CPU's map
ELEM = {"u8": ("uint8_t", ">B"), "s8": ("int8_t", ">b"), "u16": ("uint16_t", ">H"), "s16": ("int16_t", ">h"), "f32": ("float", ">f")}

# Plain tables by namespace: (name, address, element, count). "" is rom itself; ins, var, rev and mix hold the
# step and slot lists of the insertion, variation, reverb and mixer handlers.
TABLES = {
    "": [
        ("RESET_SLOTS_REVERB", 0x35DAE4, "u8", 14), ("RESET_SLOTS_REVERB_DELAY", 0x35DAFC, "u8", 12), ("RESET_SLOTS_VARIATION", 0x35DB08, "u8", 12),
        ("RESET_SLOTS_INSERTION", 0x35DB14, "u8", 24), ("STEPS_REVERB", 0x35E02C, "u16", 94), ("SLOTS_REVERB", 0x35E0E8, "u8", 54),
        ("STEPS_AUX", 0x35E11E, "u16", 40), ("SLOTS_AUX", 0x35E16E, "u8", 10), ("LFOS_AUX", 0x35E178, "u8", 4),
        ("STEPS_INSERTION", 0x35E17C, "u16", 96), ("SLOTS_INSERTION", 0x35E23C, "u8", 40), ("LFOS_INSERTION", 0x35E264, "u8", 4),
        ("STEPS_REVERB_DELAY", 0x35E268, "u16", 72), ("SLOTS_REVERB_DELAY", 0x35E2F8, "u8", 12), ("LFOS_REVERB_DELAY", 0x35E304, "u8", 4),
        ("STEPS_VARIATION", 0x35E308, "u16", 72), ("SLOTS_VARIATION", 0x35E398, "u8", 12), ("LFOS_VARIATION", 0x35E3A4, "u8", 4),
        ("STEPS_MASTER_EQ", 0x35E3A8, "u16", 50), ("STEPS_SYSTEM", 0x35E40C, "u16", 88), ("SYSTEM_COEF_DEFAULTS", 0x364E60, "u16", 88),
        ("SYSTEM_PARAM_DEFAULTS", 0x36566A, "u16", 16), ("HALL_OFFSETS_A", 0x36568C, "u16", 13), ("HALL_OFFSETS_B", 0x3656A8, "u16", 13),
        ("ROOM_OFFSETS", 0x3656C4, "u16", 13), ("EQ_DEFAULTS", 0x36580C, "u16", 20), ("LFO_SPEED", 0x36EF54, "u16", 128),
        ("LFO_PHASE", 0x36F054, "u16", 128), ("PITCH_SEMITONE", 0x36F154, "s16", 49), ("PITCH_LFO_SPEED", 0x36F1B6, "s16", 49),
        ("PITCH_FINE", 0x36F218, "s16", 52), ("DETUNE", 0x36F408, "f32", 101), ("AMP_FILTERS", 0x36F5B8, "s8", 56),
        ("COMP_RATIO", 0x36F5F0, "u16", 64), ("OCTAVE", 0x36F670, "s16", 6), ("MIRROR", 0x36F67C, "s16", 20), ("COMPLEMENT", 0x36F6A4, "s16", 16),
        ("COMB_SELECT", 0x36F824, "s16", 39), ("ER_ROWS", 0x36F872, "s16", 30), ("RT60_SECONDS", 0x36F8B0, "f32", 71),
        ("TIME_ROW", 0x36F9CC, "u8", 70), ("COMB_GAIN", 0x36FA12, "u16", 512), ("DAMP_FREQ", 0x36FE12, "s8", 11), ("COMB_CLASS", 0x36FE1D, "u8", 30),
        ("SCALED_PAIRS", 0x36FE3C, "s16", 60), ("ROW_TABLE", 0x36FEB4, "s16", 60), ("ROW_FLAGS_A", 0x36FF2C, "u16", 5),
        ("ROW_FLAGS_2", 0x36FF40, "u16", 5), ("SPAN_ROWS", 0x36FF4A, "u16", 144), ("DIFFUSION", 0x37006A, "u8", 11), ("ROOM_C", 0x3703BE, "u16", 200),
        ("ROOM_A", 0x37054E, "u8", 206), ("ROOM_B", 0x37061C, "s16", 21), ("REVERB_SHELF", 0x370646, "s8", 26), ("ER_PATTERNS", 0x370660, "s16", 400),
        ("ER_DENSITY", 0x370980, "u8", 128), ("ER_LEVEL", 0x370A00, "u8", 11), ("ROW_FLAGS_4", 0x370A0C, "u16", 4),
        ("ROW_TABLE_4", 0x370A14, "s16", 32), ("LOFI_LAW", 0x370F70, "s16", 55), ("LOFI_FILTERS", 0x370FDE, "s8", 60), ("CFG16", 0x37101A, "u8", 15),
        ("TAN", 0x371614, "f32", 61), ("DB", 0x371708, "f32", 25), ("DB_SQRT", 0x37176C, "f32", 25), ("LEVEL_CURVE", 0x3717D0, "f32", 128),
        ("PAN_LAW", 0x3719D0, "s16", 129), ("ER_REV", 0x371AD2, "s16", 129), ("MOD_DELAY", 0x371BD4, "s16", 128),
        ("DELAY_WORD", 0x371CD4, "s16", 128), ("PITCH_DELAY", 0x371DD4, "s16", 128), ("TYPE_LEVEL", 0x371ED4, "u8", 13),
        ("TYPE_SCALE_INSERTION", 0x371F0B, "u8", 252), ("TYPE_SCALE_DELAY", 0x372007, "u8", 154), ("SLOT_AREA", 0x3720A4, "s8", 128),
        ("STEPS_INDIVIDUAL_OUT", 0x3721D4, "u16", 4), ("STEPS_OUTPUT_GAIN", 0x3721DC, "u16", 8),
    ],
    "ins": [
        ("BAL_STEP_C", 0x35D732, "u16", 1), ("BAL_STEP_A", 0x35D73A, "u16", 1), ("BAL_STEP_B", 0x35D73C, "u16", 1),
        ("BAL_STEP_D", 0x35D73E, "u16", 1), ("MODE_STEPS_A", 0x35D77C, "u16", 2), ("MODE_STEPS_B", 0x35D780, "u16", 2),
        ("MOD_LEVEL", 0x35D7C4, "u16", 6), ("ROT_LEVEL", 0x35D7D0, "u16", 8), ("LFO_DIR_STEPS", 0x35D7E0, "u16", 4),
        ("MOD_WORDS", 0x35D830, "u16", 15), ("MOD_SLOTS", 0x35D84E, "u8", 10), ("ROT_WORDS", 0x35D858, "u16", 4), ("ROT_PAN", 0x35D860, "u16", 4),
        ("TREM_WORDS", 0x35D86C, "u16", 2), ("TREM_SLOTS", 0x35D870, "u8", 8), ("PC_MIX", 0x35D87A, "u16", 7), ("PC_VOICE1_A", 0x35D888, "u16", 1),
        ("PC_VOICE2", 0x35D88A, "u16", 2), ("PC_VOICE1_B", 0x35D88E, "u16", 1), ("EQ3_LEVEL", 0x35D890, "u16", 1), ("PC_WET", 0x35D894, "u16", 2),
        ("PC_FB", 0x35D898, "u16", 6), ("PC_SLOTS", 0x35D8A4, "u8", 10), ("WAH_DEPTH", 0x35D950, "u16", 1), ("PH_DEPTH", 0x35D952, "u16", 2),
        ("WAH_CUTOFF", 0x35D958, "u16", 1), ("PH_FB", 0x35D95A, "u16", 2), ("PH_OFFSET", 0x35D966, "u16", 1), ("WAH_SENSE", 0x35D968, "u16", 2),
        ("PH_WIDTH2", 0x35D970, "u16", 2), ("PH_WIDTH1", 0x35D974, "u16", 1), ("PH_STAGES", 0x35D980, "u16", 10), ("PH_DIR", 0x35D996, "u16", 3),
        ("WAH_ATTACK", 0x35D9B4, "u16", 1), ("WAH_DRIVE", 0x35D9B8, "u16", 4), ("DIST_DRIVE", 0x35D9C0, "u16", 4), ("DYN_RELEASE", 0x35D9D0, "u16", 3),
        ("DYN_THRESHOLD", 0x35D9E0, "u16", 1), ("DYN_RATIO", 0x35D9E4, "u16", 8), ("WAH_MIRROR", 0x35DA04, "u16", 3), ("HM_DRIVE", 0x35DA14, "u16", 2),
        ("WAH_WET", 0x35DA30, "u16", 8), ("PH_WET", 0x35DA40, "u16", 4), ("DIST_WET", 0x35DA48, "u16", 5), ("WAH_MODE", 0x35DA74, "u16", 2),
        ("WAH_MODE2", 0x35DA78, "u16", 2), ("DIST_MODE", 0x35DA7C, "u16", 2), ("DIST_LEVEL", 0x35DA80, "u16", 2), ("ER_ROW_SLOTS", 0x35DBE4, "u8", 8),
        ("ER_ROW_STEPS", 0x35DC54, "u16", 16), ("ER_FEEDBACK", 0x35DCF0, "u16", 1), ("ER_SLOTS", 0x35DCF4, "u8", 22),
        ("ER_STEPS", 0x35DD0E, "u16", 20), ("ER_DIFF_SLOTS", 0x35DD36, "u8", 4), ("LOFI_RATE", 0x35DDB4, "u16", 1), ("LOFI_BITS", 0x35DDB6, "u16", 1),
        ("LOFI_LEVEL", 0x35DDB8, "u16", 1), ("LOFI_WET", 0x35DDBA, "u16", 2), ("LOFI_WET2", 0x35DDBE, "u16", 2), ("LOFI_GAIN", 0x35DDC2, "u16", 1),
        ("LOFI_GAIN2", 0x35DDC4, "u16", 1), ("LOFI_LPF2", 0x35DDC8, "u16", 5), ("LOFI_HIGH_SHELF", 0x35DDD2, "u16", 3),
        ("LOFI_HIGH_SHELF_2", 0x35DDD8, "u16", 3), ("LOFI_FILTER", 0x35DDDE, "u16", 3), ("LOFI_FILTER_2", 0x35DDE4, "u16", 5),
        ("LOFI_FILTER_3", 0x35DDEE, "u16", 3), ("LOFI_OUT", 0x35DDF4, "u16", 1), ("WD_CUTOFF", 0x35DDF8, "u16", 1), ("WD_SENSE", 0x35DDFA, "u16", 1),
        ("WD_ATTACK", 0x35DDFC, "u16", 1), ("WD_DRIVE", 0x35DDFE, "u16", 4), ("WD_MIRROR", 0x35DE06, "u16", 3), ("WD_WET", 0x35DE10, "u16", 8),
        ("WD_LEVEL", 0x35DE22, "u16", 3), ("WD_MODE", 0x35DE28, "u16", 2), ("WD_MODE2", 0x35DE30, "u16", 2), ("WD_FB", 0x35DE34, "u16", 1),
        ("WD_SL", 0x35DE36, "u8", 5), ("DD_RELEASE", 0x35DE3C, "u16", 3), ("DD_THRESHOLD", 0x35DE44, "u16", 1), ("DD_RATIO", 0x35DE46, "u16", 8),
        ("DD_DRIVE", 0x35DE56, "u16", 4), ("DD_WET", 0x35DE60, "u16", 4), ("DD_LEVEL", 0x35DE6A, "u16", 3), ("DD_MODE", 0x35DE70, "u16", 3),
        ("DD_FB", 0x35DE7A, "u16", 1), ("DD_SLOTS", 0x35DE7C, "u8", 5), ("WD_BAND", 0x35DE82, "u16", 12), ("DD_BAND", 0x35DE9A, "u16", 12),
        ("WD_HPF", 0x35DEB8, "u16", 3), ("WD_LPF", 0x35DEBE, "u16", 3), ("WD_LOW_SHELF", 0x35DEC4, "u16", 3), ("WD_HIGH_SHELF", 0x35DECA, "u16", 3),
        ("WD_HPF_2", 0x35DED0, "u16", 3), ("WD_LPF_2", 0x35DED6, "u16", 3), ("WD_LPF_3", 0x35DEDC, "u16", 3), ("WD_LOW_SHELF_2", 0x35DEE2, "u16", 3),
        ("WD_LPF_4", 0x35DEE8, "u16", 3), ("WD_LOW_SHELF_COPY", 0x35DEEE, "u16", 3), ("WD_HIGH_SHELF_COPY", 0x35DEF4, "u16", 3),
        ("DD_ATTACK", 0x35DEFA, "u16", 3), ("DD_LPF", 0x35DF00, "u16", 3), ("DD_LPF_2", 0x35DF06, "u16", 3), ("WD_PEAK", 0x35DF16, "u16", 5),
        ("DD_HPF", 0x35DF20, "u16", 5), ("DD_LPF2", 0x35DF2A, "u16", 5), ("DD_PEAK", 0x35DF34, "u16", 5), ("DD_LOW_SHELF", 0x35DF3E, "u16", 5),
        ("ER_WET", 0x35DF6C, "u16", 4), ("LPF_MOD", 0x35E522, "u16", 3), ("HPF_MOD", 0x35E528, "u16", 3), ("LPF_MOD_C", 0x35E52E, "u16", 3),
        ("HPF_MOD_C", 0x35E534, "u16", 3), ("DELAY_CURVE", 0x35E53A, "u16", 2), ("DELAY_CURVE_COPY", 0x35E540, "u16", 2),
        ("LPF_ROT", 0x35E546, "u16", 3), ("HPF_ROT", 0x35E54C, "u16", 3), ("LPF_ROT_C", 0x35E552, "u16", 3), ("HPF_ROT_C", 0x35E558, "u16", 3),
        ("LPF_PH", 0x35E55E, "u16", 3), ("HPF_PH", 0x35E564, "u16", 3), ("LPF_PH_C", 0x35E56A, "u16", 3), ("HPF_PH_C", 0x35E570, "u16", 3),
        ("ER_LPF", 0x35E576, "u16", 3), ("WAH_HPF", 0x35E57C, "u16", 3), ("WAH_LPF", 0x35E582, "u16", 3), ("LPF_WAH", 0x35E588, "u16", 3),
        ("HPF_WAH", 0x35E58E, "u16", 3), ("WAH_HPF_2", 0x35E594, "u16", 3), ("WAH_LPF_2", 0x35E59A, "u16", 3), ("LPF_WAH_C", 0x35E5A0, "u16", 3),
        ("HPF_WAH_C", 0x35E5A6, "u16", 3), ("DIST_OUTPUT", 0x35E5AC, "u16", 3), ("DIST_OUTPUT_C", 0x35E5B2, "u16", 3), ("DYN_ATTACK", 0x35E5B8, "u16", 3),
        ("EQ_MOD", 0x35E7B4, "u16", 5), ("EQ_MOD_C", 0x35E7BE, "u16", 5), ("EQ_ROT", 0x35E7C8, "u16", 5), ("EQ_ROT_C", 0x35E7D2, "u16", 5),
        ("ER_HPF", 0x35E7DC, "u16", 5), ("ER_LPF_2", 0x35E7F0, "u16", 5), ("WAH_LPF2", 0x35E7FA, "u16", 5), ("WAH_PEAK", 0x35E804, "u16", 5),
        ("WAH_LOW_SHELF2", 0x35E80E, "u16", 5), ("DIST_INPUT", 0x35E818, "u16", 5), ("DIST_INPUT_C", 0x35E822, "u16", 5),
        ("DIST_PRE", 0x35E82C, "u16", 5), ("DIST_PRE_C", 0x35E836, "u16", 5), ("DIST_EQ", 0x35E840, "u16", 5), ("EQ7_A", 0x35E84A, "u16", 5),
        ("EQ7_B", 0x35E854, "u16", 5), ("EQ7_C", 0x35E85E, "u16", 5), ("EQ7_A_C", 0x35E868, "u16", 5), ("EQ7_B_C", 0x35E872, "u16", 5),
        ("EQ7_C_C", 0x35E87C, "u16", 5), ("DIST_BAND", 0x35E9CA, "u16", 12), ("WAH_BAND", 0x35E9E2, "u16", 12),
    ],
    "var": [
        ("BAL_STEP_C", 0x35D75A, "u16", 1), ("BAL_STEP_A", 0x35D762, "u16", 1), ("BAL_STEP_B", 0x35D764, "u16", 1),
        ("MODE_STEPS_A", 0x35D7A8, "u16", 2), ("MODE_STEPS_B", 0x35D7AC, "u16", 2), ("MOD_LEVEL_STEPS", 0x35D80C, "u16", 6),
        ("ROT_STEPS", 0x35D818, "u16", 7), ("DETUNE_DET", 0x35D828, "u16", 4), ("PAN_STEPS", 0x35D8EA, "u16", 4), ("FB_STEPS", 0x35D8F2, "u16", 2),
        ("MIX_STEPS", 0x35D8F6, "u16", 2), ("MOD_SLOTS", 0x35D8FA, "u8", 10), ("PAN_STEPS_B", 0x35D904, "u16", 4), ("TREM_WORDS", 0x35D90C, "u16", 2),
        ("TREM_SLOTS", 0x35D910, "u8", 8), ("DETUNE_PAN", 0x35D91A, "u16", 4), ("DETUNE_SLOTS", 0x35D922, "u8", 10),
        ("DELAY_WORDS", 0x35D92C, "u16", 14), ("DELAY_SLOTS", 0x35D948, "u8", 6), ("WAH_DEPTH", 0x35D956, "u16", 1),
        ("WAH_CUTOFF", 0x35D95E, "u16", 1), ("PH_FB", 0x35D960, "u16", 2), ("WAH_SENSE", 0x35D96C, "u16", 2), ("PH_WIDTH2", 0x35D978, "u16", 2),
        ("PH_WIDTH1", 0x35D97C, "u16", 1), ("PH_STAGES", 0x35D99C, "u16", 8), ("PH_DIR", 0x35D9AE, "u16", 3), ("WAH_ATTACK", 0x35D9B6, "u16", 1),
        ("DIST_DRIVE", 0x35D9C8, "u16", 4), ("DYN_RELEASE", 0x35D9D8, "u16", 3), ("DYN_THRESHOLD", 0x35D9E2, "u16", 1),
        ("DYN_RATIO", 0x35D9F4, "u16", 8), ("WAH_MIRROR", 0x35DA0C, "u16", 3), ("HM_DRIVE", 0x35DA18, "u16", 2), ("DIST_PAN", 0x35DA52, "u16", 4),
        ("WAH_PAN", 0x35DA5A, "u16", 4), ("PH_PAN", 0x35DA62, "u16", 4), ("DYN_OUT", 0x35DA6A, "u16", 4), ("KARAOKE_SLOTS", 0x35DB69, "u8", 2),
        ("KARAOKE_ROW_SLOTS", 0x35DBEC, "u8", 8), ("KARAOKE_ROW_STEPS", 0x35DC74, "u16", 16), ("REVERB_HPF", 0x35DD52, "u16", 3),
        ("REVERB_LPF", 0x35DD58, "u16", 3), ("REVERB_ER_IDS", 0x35DD6A, "u8", 6), ("REVERB_COMBS", 0x35DD70, "u16", 8),
        ("REVERB_ROW_SLOTS", 0x35DD80, "u8", 4), ("REVERB_ROW_STEPS", 0x35DD84, "u16", 8), ("REVERB_PRE", 0x35DD94, "u8", 2),
        ("REVERB_W", 0x35DD96, "u16", 10), ("REVERB_WIDTH", 0x35DDAA, "u16", 1), ("REVERB_PAN", 0x35DDAC, "u16", 4),
        ("KARAOKE_PAN", 0x35DF80, "u16", 4), ("LPF_A", 0x35E6CE, "u16", 3), ("HPF_A", 0x35E6D4, "u16", 3), ("LPF_A_COPY", 0x35E6DA, "u16", 3),
        ("HPF_A_COPY", 0x35E6E0, "u16", 3), ("LPF_B", 0x35E6E6, "u16", 3), ("HPF_B", 0x35E6EC, "u16", 3), ("LPF_B_COPY", 0x35E6F2, "u16", 3),
        ("HPF_B_COPY", 0x35E6F8, "u16", 3), ("DIST_INPUT", 0x35E6FE, "u16", 3), ("DIST_OUTPUT", 0x35E704, "u16", 3), ("DYN_HPF", 0x35E710, "u16", 3),
        ("DYN_HPF_COPY", 0x35E716, "u16", 3), ("DYN_ATTACK", 0x35E71C, "u16", 3), ("WAH_HPF", 0x35E722, "u16", 3), ("WAH_LPF", 0x35E728, "u16", 3),
        ("WAH_FILTER", 0x35E72E, "u16", 3), ("WAH_FILTER_2", 0x35E734, "u16", 3), ("WAH_FILTER_3", 0x35E73A, "u16", 3),
        ("WAH_FILTER_4", 0x35E740, "u16", 3), ("PH_FILTER", 0x35E746, "u16", 3), ("PH_FILTER_2", 0x35E74C, "u16", 3),
        ("PH_FILTER_3", 0x35E752, "u16", 3), ("PH_FILTER_4", 0x35E758, "u16", 3), ("DETUNE_LOW_SHELF", 0x35E75E, "u16", 3),
        ("DETUNE_HIGH_SHELF", 0x35E764, "u16", 3), ("DETUNE_LOW_SHELF_COPY", 0x35E76A, "u16", 3), ("DETUNE_HIGH_SHELF_COPY", 0x35E770, "u16", 3),
        ("DELAY_LPF", 0x35E776, "u16", 3), ("DELAY_HPF", 0x35E77C, "u16", 3), ("DELAY_LPF_COPY", 0x35E782, "u16", 3),
        ("DELAY_HPF_COPY", 0x35E788, "u16", 3), ("DELAY_CURVE", 0x35E78E, "u16", 2), ("DELAY_CURVE_COPY", 0x35E792, "u16", 2),
        ("KARAOKE_HPF", 0x35E796, "u16", 3), ("KARAOKE_LPF", 0x35E79C, "u16", 3), ("KARAOKE_LEVEL", 0x35E7A2, "u16", 2),
        ("DIST_LPF2", 0x35E8B8, "u16", 5), ("DIST_PEAK", 0x35E8C2, "u16", 5), ("DIST_LOW_SHELF", 0x35E8CC, "u16", 5),
        ("DYN_HPF2", 0x35E8D6, "u16", 5), ("DYN_HPF2_COPY", 0x35E8E0, "u16", 5), ("EQ3_LOW_SHELF2", 0x35E8EA, "u16", 5),
        ("EQ3_PEAK", 0x35E8F4, "u16", 5), ("EQ3_HIGH_SHELF2", 0x35E8FE, "u16", 5), ("EQ3_LOW_SHELF2_COPY", 0x35E908, "u16", 5),
        ("EQ3_PEAK_COPY", 0x35E912, "u16", 5), ("EQ3_HIGH_SHELF2_COPY", 0x35E91C, "u16", 5), ("DIST_BAND", 0x35E9FA, "u16", 12),
        ("EQ3_A", 0x35EA12, "u16", 3), ("EQ3_B", 0x35EA18, "u16", 3),
    ],
    "rev": [
        ("DELAY_WORDS", 0x35D8C8, "u16", 14), ("DELAY_SLOTS", 0x35D8E4, "u8", 6), ("COMB_SLOTS", 0x35DB39, "u8", 24),
        ("HALL_COMB_STEPS", 0x35DB6C, "u16", 24), ("ROW_SLOTS", 0x35DBCC, "u8", 12), ("ROW_STEPS", 0x35DBF4, "u16", 24),
        ("ER_BASE_SLOTS", 0x35DC94, "u8", 12), ("ER_WORDS", 0x35DCA4, "u16", 22), ("WIDTH_STEPS", 0x35DCEC, "u16", 2),
        ("MIX_STEPS", 0x35DF48, "u16", 5), ("LEVEL_STEPS", 0x35DF68, "u16", 2), ("HPF_STEPS", 0x35E4BC, "u16", 3), ("LPF_STEPS", 0x35E4C2, "u16", 3),
        ("HPF_COPY", 0x35E4C8, "u16", 3), ("LPF_COPY", 0x35E4CE, "u16", 3), ("TABLE_STEPS", 0x35E4D4, "u16", 3), ("DELAY_LPF", 0x35E69C, "u16", 3),
        ("DELAY_HPF", 0x35E6A2, "u16", 3), ("DELAY_LPF_COPY", 0x35E6A8, "u16", 3), ("DELAY_HPF_COPY", 0x35E6AE, "u16", 3),
        ("DELAY_CURVE", 0x35E6B4, "u16", 2), ("DELAY_CURVE_COPY", 0x35E6B8, "u16", 2),
    ],
    "mix": [
        ("MIXER_A", 0x35DF88, "u16", 12), ("MIXER_B", 0x35DFA0, "u16", 16), ("MIXER_C", 0x35DFC0, "u16", 34), ("MIXER_HPF", 0x35E7A8, "u16", 3),
        ("MIXER_HPF_COPY", 0x35E7AE, "u16", 3), ("EQ_LOW", 0x35E926, "u16", 5), ("EQ_MID", 0x35E930, "u16", 5), ("EQ_HIGH", 0x35E93A, "u16", 5),
        ("EQ_LOW_MID", 0x35E944, "u16", 5), ("EQ_HIGH_MID", 0x35E94E, "u16", 5), ("EQ_LOW_COPY", 0x35E958, "u16", 5),
        ("EQ_MID_COPY", 0x35E962, "u16", 5), ("EQ_HIGH_COPY", 0x35E96C, "u16", 5), ("EQ_LOW_MID_COPY", 0x35E976, "u16", 5),
        ("EQ_HIGH_MID_COPY", 0x35E980, "u16", 5),
    ],
}
# Tables with rows: (name, address, element, rows, columns).
GRIDS = [
    ("TYPE_MAP_REVERB", 0x372124, "u8", 17, 2),      # Data List type -> (0 reverb window or 3 delay window, type code)
    ("TYPE_MAP_VARIATION", 0x372146, "u8", 29, 2),   # Data List type -> (msb, lsb)
    ("TYPE_MAP_INSERTION", 0x372180, "u8", 41, 2),
    ("INSERTION_CONTROL", 0x3895F2, "u8", 41, 14),   # per insertion type: controller destination 1..14 -> parameter, 0xFF none
    ("BASE_TAGS", 0x3661E4, "u8", 2, 232),           # the base program's tags, then its cleared tags
]
# A window's programs, one per type msb: (name, address, entries, steps); each step is five words. Tags alike.
PROGRAMS = [("BASE_PROGRAM", 0x3658D4, 1, 232), ("AUX_PROGRAM", 0x3663B4, 1, 40), ("INSERTION_PROGRAMS", 0x366A94, 15, 96),
            ("REVERB_DELAY_PROGRAMS", 0x36A874, 12, 72), ("VARIATION_PROGRAMS", 0x36CA34, 12, 72)]
TAGS = [("AUX_TAGS", 0x3669F4, 1, 40), ("INSERTION_TAGS", 0x36A2D4, 15, 96), ("DELAY_TAGS", 0x36EBF4, 12, 72)]
# Presets: a block of constants, delay offsets and LFO words per type, found through an index table, and sixteen
# default parameters per type. window: (words a block, {msb: (blocks, index table, defaults)}); a missing msb is msb 0.
PRESETS = {
    "REVERB": (148, {0: (0x363600, 0x364F10, 0x364F4A)}),
    "AUX": (66, {0: (0x35EA20, 0x3619B4, 0x3610F4)}),
    "INSERTION": (152, {0: (0x35ECB4, 0x3619C2, 0x3612B4), 1: (0x35F634, 0x3619D4, 0x3614F4), 2: (0x3619FC, 0x3635DC, 0x36319C),
                        3: (0x363AA0, 0x364F1D, 0x3650EA), 4: (0x361D8C, 0x3635DF, 0x3631FC), 5: (0x36211C, 0x3635E7, 0x3632FC),
                        7: (0x35FD54, 0x3619DA, 0x3615B4), 10: (0x3642F0, 0x364F32, 0x36538A), 12: (0x364550, 0x364F34, 0x3653CA),
                        13: (0x3647B0, 0x364F37, 0x36542A)}),
    "REVERB_DELAY": (100, {0: (0x360474, 0x3619E1, 0x361694), 8: (0x360D0C, 0x3619F5, 0x361914)}),
    "VARIATION": (100, {0: (0x360474, 0x3619E1, 0x361694), 1: (0x36085C, 0x3619EF, 0x361854), 2: (0x36283C, 0x3635EF, 0x3633FC),
                        3: (0x3629CC, 0x3635F3, 0x36347C), 4: (0x362F44, 0x3635F7, 0x3634FC), 5: (0x362CEC, 0x3635FB, 0x36357C),
                        6: (0x360B7C, 0x3619F3, 0x3618D4), 7: (0x3656E0, 0x3657A8, 0x3657AC), 8: (0x360D0C, 0x3619F5, 0x361914),
                        9: (0x364B40, 0x364F3C, 0x3654CA), 10: (0x364CD0, 0x364F40, 0x36554A)}),
}
# The filter host's program and laws, in namespace flt: (name, address, element, count[, words a row]). The laws are
# 128 long where the firmware indexes them with a byte as it came, whatever lies behind their last real entry.
FILTER = [("PROGRAM", 0x375E1A, "u16", 512, 5), ("CONSTANTS", 0x37541A, "u16", 512), ("TAGS", 0x375C1A, "u8", 512),
          ("CFG16", 0x37861A, "u8", 15), ("LFO2_SPEED", 0x374A04, "u16", 128), ("LFO2_PHASE", 0x374B04, "u16", 128),
          ("RESONANCE_A", 0x374B24, "u16", 128), ("RESONANCE_B", 0x374C24, "u16", 128), ("GAIN", 0x374D24, "u16", 128),
          ("EG_RATE", 0x374F4E, "u16", 102)]
# Parameter descriptors, sixteen bytes each: +6 minimum, +8 maximum, +12 how the value is stored.
COMMON_PARAMS = 0x37E654                      # by performance byte
TYPED_PARAMS = [("REVERB_PARAMS", 0x388DBC, 17, 24), ("VARIATION_PARAMS", 0x388E00, 29, 31), ("INSERTION_PARAMS", 0x388E74, 41, 31)]

if len(sys.argv) != 2:
    sys.exit(__doc__)
rom = Path(sys.argv[1]).read_bytes()
if not hashlib.sha256(rom).hexdigest().startswith("3be337ed29bc44c0"):
    sys.exit("not the FS1R v1.20 EPROM in CPU byte order")

def u8(a): return rom[a - BASE]
def s8(a): return struct.unpack(">b", rom[a - BASE:a - BASE + 1])[0]
def u16(a): return struct.unpack(">H", rom[a - BASE:a - BASE + 2])[0]
def s16(a): return struct.unpack(">h", rom[a - BASE:a - BASE + 2])[0]
def u32(a): return struct.unpack(">I", rom[a - BASE:a - BASE + 4])[0]

owner = {}                                    # byte -> the plain table that holds it
def read(name, addr, elem, n):
    size = struct.calcsize(ELEM[elem][1])
    for b in range(addr, addr + n * size):
        if owner.setdefault(b, name) != name: sys.exit(f"{name} overlaps {owner[b]} at 0x{b:06X}")
    return [struct.unpack(ELEM[elem][1], rom[a - BASE:a - BASE + size])[0] for a in range(addr, addr + n * size, size)]

def lit(v, elem):
    if elem != "f32": return f"0x{v:X}" if elem[0] == "u" and v > 9 else str(v)
    s = next(s for s in (f"{v:.{p}g}" for p in range(1, 10)) if struct.unpack(">f", struct.pack(">f", float(s)))[0] == v)
    return (s if any(c in s for c in ".en") else s + ".0") + "f"

def rows(vals, per, elem, indent="    "):
    return [indent + ",".join(lit(v, elem) for v in vals[i:i + per]) + "," for i in range(0, len(vals), per)]

h = ["// FS1R v1.20 EPROM: the tables the two hosts read. Yamaha's data.", "// Written by tools/extract_vop3_rom.py. Do not edit.",
     "#pragma once", "#include <cstdint>", "", "namespace fs1r::vop3host::rom {", ""]
c = [h[0], h[1], '#include "rom_tables.h"', "", "namespace fs1r::vop3host::rom {", ""]

# ---- plain tables
for ns, tables in TABLES.items():
    if ns: h.append(f"namespace {ns} {{"); c.append(f"namespace {ns} {{")
    for name, addr, elem, n in tables:
        vals = read(name, addr, elem, n)
        if "STEPS" in name or (ns and elem == "u16"):
            if elem == "u16" and max(vals) >= 512: sys.exit(f"{name}: a step past 511")
        if (ns and elem == "u8") or name.startswith(("SLOTS_", "RESET_SLOTS_")):
            if max(vals) >= 128: sys.exit(f"{name}: a delay slot past 127")
        if name.startswith("LFOS_") and max(vals) >= 16: sys.exit(f"{name}: an LFO past 15")
        h.append(f"extern const {ELEM[elem][0]} {name}[{n}];")
        if n <= 24: c.append(f"const {ELEM[elem][0]} {name}[{n}] = {{{','.join(lit(v, elem) for v in vals)}}};")
        else: c += [f"const {ELEM[elem][0]} {name}[{n}] = {{"] + rows(vals, 24 if elem != "f32" else 12, elem) + ["};"]
    if ns: h.append("}"); c.append("}")
    h.append(""); c.append("")

# ---- tables with rows
for name, addr, elem, r, k in GRIDS:
    size = struct.calcsize(ELEM[elem][1])
    vals = [struct.unpack(ELEM[elem][1], rom[a - BASE:a - BASE + size])[0] for a in range(addr, addr + r * k * size, size)]
    h.append(f"extern const {ELEM[elem][0]} {name}[{r}][{k}];")
    c += [f"const {ELEM[elem][0]} {name}[{r}][{k}] = {{"] + ["    {" + ",".join(lit(v, elem) for v in vals[i:i + k]) + "}," for i in range(0, r * k, k)] + ["};"]
for name, addr, entries, steps in PROGRAMS:
    vals = [u16(a) for a in range(addr, addr + entries * steps * 10, 2)]
    note = f"   // {entries} programs of {steps} steps, by type msb" if entries > 1 else ""
    h.append(f"extern const uint16_t {name}[{entries * steps}][5];{note}")
    c.append(f"const uint16_t {name}[{entries * steps}][5] = {{")
    for e in range(entries):
        if entries > 1: c.append(f"    // {e}")
        c += ["    " + ",".join("{" + ",".join(lit(v, "u16") for v in vals[(e * steps + s) * 5:(e * steps + s) * 5 + 5]) + "}" for s in range(t, min(t + 6, steps))) + ","
              for t in range(0, steps, 6)]
    c.append("};")
for name, addr, entries, steps in TAGS:
    note = f"   // {entries} programs of {steps} steps, by type msb" if entries > 1 else ""
    h.append(f"extern const uint8_t {name}[{entries * steps}];{note}")
    c.append(f"const uint8_t {name}[{entries * steps}] = {{")
    for e in range(entries):
        vals = [u8(a) for a in range(addr + e * steps, addr + (e + 1) * steps)]
        c += rows(vals, 32, "u8") if entries == 1 else ["    " + ",".join(lit(v, "u8") for v in vals) + ","]
    c.append("};")
h.append(""); c.append("")

# ---- the filter host's tables, and the index maps it computes instead of reading
h.append("namespace flt {"); c.append("namespace flt {")
for name, addr, elem, n, *row in FILTER:
    size = struct.calcsize(ELEM[elem][1])
    k = row[0] if row else 1
    vals = [struct.unpack(ELEM[elem][1], rom[a - BASE:a - BASE + size])[0] for a in range(addr, addr + n * k * size, size)]
    dims = f"[{n}][{k}]" if row else f"[{n}]"
    h.append(f"extern const {ELEM[elem][0]} {name}{dims};")
    c.append(f"const {ELEM[elem][0]} {name}{dims} = {{")
    c += ["    " + ",".join("{" + ",".join(lit(v, elem) for v in vals[s * k:s * k + k]) + "}" for s in range(t, min(t + 6, n))) + "," for t in range(0, n, 6)] if row else rows(vals, 24, elem)
    c.append("};")
h += ["}", ""]; c += ["}", ""]
page = lambda s: 16 + 124 * (((s >> 2) + 1) & 3)          # slot s: its page of the program, its lane in it, its registers
lane = lambda s: s & 3
regs = lambda s: 6 * ((s + 4) & 15)
MAPS = [(0x374D64, 2, lambda s: page(s) + 52 + 4 * lane(s)), (0x374D84, 2, lambda s: page(s) + 53 + 4 * lane(s)),
        (0x374DA4, 2, lambda s: page(s) + 54 + 4 * lane(s)), (0x374DC4, 2, lambda s: page(s) + 28 + lane(s)),
        (0x374DE4, 2, lambda s: page(s) + 3 * lane(s)), (0x374E04, 2, lambda s: page(s) + 3 * lane(s) + 1),
        (0x374E24, 2, lambda s: page(s) + 120 + lane(s)), (0x374E44, 2, lambda s: page(s) + 3 * lane(s) + 2),
        (0x374E64, 2, lambda s: page(s) + 32 + lane(s)), (0x374E84, 2, lambda s: page(s) + 116 + lane(s)),
        (0x374EAE, 1, lambda s: regs(s) + 1), (0x374EBE, 1, lambda s: regs(s) + 5), (0x374ECE, 1, lambda s: regs(s) + 4),
        (0x374EDE, 1, lambda s: regs(s) + 3), (0x374EEE, 1, lambda s: regs(s) + 2), (0x374EFE, 1, lambda s: 99 + 3 * lane(s)),
        (0x374F0E, 1, lambda s: 98 + 3 * lane(s)), (0x374F2E, 1, lambda s: (s + 2) & 15), (0x374F3E, 1, lambda s: (s + 14) & 15)]
for addr, size, f in MAPS:
    if [u16(addr + 2 * s) if size == 2 else u8(addr + s) for s in range(16)] != [f(s) for s in range(16)]: sys.exit(f"the filter host's map at 0x{addr:06X} is not what filter_host.cpp computes")
if [u16(0x374EA4 + 2 * j) for j in range(5)] != [5, 6, 7, 8, 9]: sys.exit("the filter's clear steps are not 5..9")
if any(u16(0x37501A + 2 * i) != i for i in range(512)): sys.exit("the filter's step list is not 0..511")
for s in range(16):                           # the four constants a note retags, through the lane rows and the bank bases
    got = [u8(0x378629 + 6 * lane(s) + j) + s16(0x378642 + 2 * (s >> 2)) - 1 for j in range(4)]
    if got != [page(s) + 3 * lane(s), page(s) + 3 * lane(s) + 1, page(s) + 3 * lane(s) + 2, page(s) + 32 + lane(s)]: sys.exit(f"slot {s}: the retagged constants are not where filter_host.cpp puts them")
if u16(0x374F4E + 2 * 101) != 255: sys.exit("the EG rate that holds is not 255")

# ---- presets: the type codes a performance can reach, from the type maps and the codes the host starts with
maps = {g[0]: [(u8(g[1] + 2 * i), u8(g[1] + 2 * i + 1)) for i in range(g[3])] for g in GRIDS[:3]}
codes = {
    "REVERB": {(0, 0)} | {(0, lsb) for w, lsb in maps["TYPE_MAP_REVERB"] if w == 0},
    "AUX": {(0, 0)},
    "INSERTION": {(0, 0)} | set(maps["TYPE_MAP_INSERTION"]),
    "REVERB_DELAY": {(0, 0), (0, 1)} | {(8, lsb) for w, lsb in maps["TYPE_MAP_REVERB"] if w != 0},
    "VARIATION": {(0, 0)} | set(maps["TYPE_MAP_VARIATION"]),
}
h += ["// A window's presets for one type msb: the block of a type lsb is blocks + blockOf[lsb] * its length in words.",
      "struct PresetGroup { const uint16_t* blocks; const uint8_t* blockOf; const uint16_t (*defaults)[16]; };"]
for window, (words, groups) in PRESETS.items():
    top = max(m for m, l in codes[window])
    if any(m not in groups for m, l in codes[window]): sys.exit(f"{window}: a type msb without a preset group")
    names = {}
    for msb, (blocks, index, defaults) in sorted(groups.items()):
        lsbs = sorted(l for m, l in codes[window] if m == msb)
        if not lsbs: continue
        at = {}                               # block address -> row
        block_of, default_rows = [], []
        for lsb in range(lsbs[-1] + 1):
            if lsb not in lsbs: block_of.append(0); default_rows.append([0] * 16); continue
            if window == "VARIATION" and msb == 7: a = blocks + ((s8(index + lsb) & 0xFFFF) * 200 & 0xFF)   # the firmware's own byte-wide product
            else: a = blocks + struct.unpack(">h", struct.pack(">H", u8(index + lsb) * words * 2 & 0xFFFF))[0]
            block_of.append(at.setdefault(a, len(at)))
            default_rows.append([u16(defaults + lsb * 32 + 2 * i) for i in range(16)])
        # the hall handler picks its delay offsets by the reverb's block number
        if window == "REVERB" and block_of != [u8(index + lsb) for lsb in range(len(block_of))]: sys.exit("REVERB: the blocks are not in the index table's order")
        n = f"{window}_{msb}"
        c.append(f"static const uint16_t {n}_BLOCKS[{len(at)}][{words}] = {{")
        for a in at: c += ["  {"] + rows([u16(a + 2 * i) for i in range(words)], 24, "u16") + ["  },"]
        c += ["};", f"static const uint8_t {n}_BLOCK_OF[{len(block_of)}] = {{{','.join(map(str, block_of))}}};",
              f"static const uint16_t {n}_DEFAULTS[{len(default_rows)}][16] = {{"]
        c += ["    {" + ",".join(lit(v, "u16") for v in r) + "}," for r in default_rows] + ["};"]
        names[msb] = f"{{{n}_BLOCKS[0], {n}_BLOCK_OF, {n}_DEFAULTS}}"
    if top == 0:
        h.append(f"extern const PresetGroup {window}_PRESETS;")
        c.append(f"const PresetGroup {window}_PRESETS = {names[0]};")
    else:
        h.append(f"extern const PresetGroup {window}_PRESETS[{top + 1}];   // by type msb")
        c.append(f"const PresetGroup {window}_PRESETS[{top + 1}] = {{" + ", ".join(names.get(m, names[0]) for m in range(top + 1)) + "};")
h.append(""); c.append("")

# ---- parameter descriptors: the range, and how many bytes the value takes (two are 7 bits each, high first)
def desc(a):
    kind = u8(a + 12) & 0x3F
    lo, hi = s16(a + 6), s16(a + 8)
    if kind not in (0, 8, 13) or (kind and not 0 <= lo <= hi): sys.exit(f"descriptor at 0x{a:06X}: type {kind}, range {lo}..{hi}")
    return "{%d,%d,%d}" % (lo, hi, {0: 0, 8: 1, 13: 2}[kind])
h += ["struct ParamRange { int16_t min, max; uint8_t bytes; };", "extern const ParamRange COMMON_PARAMS[192];   // by performance byte"]
c += ["const ParamRange COMMON_PARAMS[192] = {"] + ["    " + ",".join(desc(COMMON_PARAMS + 16 * i) for i in range(k, k + 8)) + "," for k in range(0, 192, 8)] + ["};"]
if desc(COMMON_PARAMS + 16 * 191).endswith(",2}"): sys.exit("the last performance byte starts a two-byte parameter")
for name, table, types, n in TYPED_PARAMS:
    h.append(f"extern const ParamRange {name}[{types}][{n}];   // by type, then byte within the block")
    c.append(f"const ParamRange {name}[{types}][{n}] = {{")
    for t in range(types):
        p = u32(table + 4 * t)
        c.append("    {" + ",".join(desc(p + 16 * i) for i in range(n)) + "},")
    c.append("};")
for g, (name, table, types, n) in zip(GRIDS[:3], TYPED_PARAMS):
    last = s16(COMMON_PARAMS + 16 * {"REVERB_PARAMS": 168, "VARIATION_PARAMS": 171, "INSERTION_PARAMS": 175}[name] + 8)
    if last != types - 1 or g[3] != types: sys.exit(f"{name}: {types} types against a type byte that goes to {last}")

tail = ["", "}  // namespace fs1r::vop3host::rom", ""]
(OUT / "rom_tables.h").write_bytes("\n".join(h + tail).encode())
(OUT / "rom_tables.cpp").write_bytes("\n".join(c + tail).encode())
print(f"wrote rom_tables.h and rom_tables.cpp: {sum(len(t) for t in TABLES.values())} plain tables, {len(owner)} bytes in them")
