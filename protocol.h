/*
 * This file has been translated using AI
 *
 * This driver allows you to communicate with your Novation Launchpad (NVLPD01)
 * Copyright (C) 2012-2018 Vincent Deca. known as Bigcake@ubuntu-fr.org
 * (user@forum not email)
 *
 * This file is part of 'launchpadctrl'.
 *
 * This driver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 */

#ifndef INC_PROTOCOL_H
#define INC_PROTOCOL_H

/* ----------------- Driver Features ----------------------
 * ====== Retrieve Launchpad State =====
 * If an application sends 2 bytes: {LP_OPTION, LP_GET_STAT}
 * The driver responds with 3 bytes: {LP_OPTION, LP_GET_STAT, LP_MENU or LP_GRID}
 *
 * ====== Retrieve Driver Version =====
 * If an application sends 2 bytes: {LP_OPTION, LP_GET_VERSION}
 * The driver responds with 4 bytes: {LP_OPTION, LP_GET_VERSION, version_byte_1, version_byte_2}
 * Example for version 1.0: byte 1 = 1, byte 2 = 0
 *
 * ====== Disconnection Signal =====
 * If the launchpad is unplugged while an application is using it,
 * the driver sends 2 bytes: {LP_OPTION, LP_IS_UNPLUG}
 */

/* -------------------- Launchpad & Driver Protocol ----------------------- */

/* --- 1st Byte (Message Type) --- */
#define LP_MENU         176  /* Data corresponds to top menu buttons */
#define LP_OPTION       175  /* Data corresponds to a driver-specific option */
#define LP_GRID         144  /* Data corresponds to grid or side buttons */

/* --- 2nd Byte (Option or Key ID) --- */
#define LP_GET_STAT     1    /* Driver Option - Get current state */
#define LP_IS_UNPLUG    2    /* Driver Option - Device was unplugged */
#define LP_GET_VERSION  3    /* Driver Option - Get driver version */

/* Top Menu Buttons */
#define LP_TOP_LEARN    104  /* 'Learn' button */
#define LP_TOP_VIEW     105  /* 'View' button */
#define LP_TOP_PAGE_L   106  /* '<]' button */
#define LP_TOP_PAGE_R   107  /* '[>' button */
#define LP_TOP_SESSION  108  /* 'Session' button */
#define LP_TOP_USER1    109  /* 'User 1' button */
#define LP_TOP_USER2    110  /* 'User 2' button */
#define LP_TOP_MIXER    111  /* 'Mixer' button */

/* Grid Mapping Examples */
#define LP_1X1          0    /* Grid: Row 1, Col 1 */
#define LP_1X8          7    /* Grid: Row 1, Col 8 */
#define LP_LEFT_VOL     8    /* 'Vol' button */
#define LP_2X1          16   /* Grid: Row 2, Col 1 */
#define LP_2X8          23   /* Grid: Row 2, Col 8 */
#define LP_LEFT_PAN     24   /* 'Pan' button */
/* ... etc (Mapping continues for the 8x8 grid) ... */
#define LP_LEFT_SNDA    40   /* 'Snd A' button */
#define LP_LEFT_SNDB    56   /* 'Snd B' button */
#define LP_LEFT_STOP    72   /* 'Stop' button */
#define LP_LEFT_TRKON   88   /* 'Trk On' button */
#define LP_LEFT_SOLO    104  /* 'Solo' button */
#define LP_LEFT_ARM     120  /* 'Arm' button */

/* --- 3rd Byte (Data / Velocity / Color) --- */

/* Write Operations (LED Control) */
#define LP_COPY         (1 << 2)  /* Copy data to 2nd buffer */
#define LP_CLEAR        (1 << 3)  /* Clear 2nd buffer */

#define LP_OFF          (LP_CLEAR | LP_COPY) /* LED Off */

/* Red LED Intensity */
#define LP_LOW_RED      (LP_OFF | 1 << 0)
#define LP_MED_RED      (LP_OFF | 1 << 1)
#define LP_FULL_RED     (LP_LOW_RED | LP_MED_RED)

/* Green LED Intensity */
#define LP_LOW_GREEN    (LP_OFF | 1 << 4)
#define LP_MED_GREEN    (LP_OFF | 1 << 5)
#define LP_FULL_GREEN   (LP_LOW_GREEN | LP_MED_GREEN)

/* Combined Colors */
#define LP_LOW_YELLOW   (LP_OFF | LP_LOW_GREEN | LP_LOW_RED)
#define LP_MED_YELLOW   (LP_OFF | LP_MED_GREEN | LP_MED_RED)
#define LP_FULL_YELLOW  (LP_FULL_GREEN | LP_FULL_RED)

#define LP_LOW_ORANGE   (LP_OFF | LP_MED_RED | LP_LOW_GREEN)
#define LP_MED_ORANGE   (LP_OFF | LP_FULL_RED | LP_MED_GREEN)
#define LP_FULL_ORANGE  (LP_FULL_RED | LP_LOW_GREEN)

/* Animation Modes */
#define LP_START_FLASH  40 /* Flash Mode ON */
#define LP_STOP_FLASH   48 /* Flash Mode OFF */

/* Read Operations (Button States) */
#define LP_PUSHED       127 /* Button is pressed */
#define LP_RELEASE      0   /* Button is released */

/* --- Hardware brightness levels --- */
#define LP_BRIGHT_LVL0  0   /* Off */
#define LP_BRIGHT_LVL1  1   /* Low */
#define LP_BRIGHT_LVL2  2   /* Medium */
#define LP_BRIGHT_LVL3  3   /* Full */

/* --- Global Commands --- */
#define LP_RESET        {176, 0, 0}   /* Turn everything off */
#define LP_ALL_ON_L     {176, 0, 125} /* Turn all on (Low) */
#define LP_ALL_ON_M     {176, 0, 126} /* Turn all on (Medium) */
#define LP_ALL_ON_F     {176, 0, 127} /* Turn all on (Full) */

#endif /* INC_PROTOCOL_H */
