/**
 * @file
 * @brief Nothing other than storing a common rev information
 *
 * FileName: include/rev.h
 *
 */
/*
 * (C) Copyright 2008-2009
 * Texas Instruments, <www.ti.com>
 * Nishanth Menon <nm@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef __LIB_REV_H
#define __LIB_REV_H
#define OMAP_UBOOT_UTILS_REVISION "\nRev: OMAP U-boot Utils v0.2.1\n"
#define OMAP_UBOOT_UTILS_LICENSE \
	       "\nCopyright (C) 2008-2009 Texas Instruments, <www.ti.com>\n"\
	       "This is free software; see the source for copying conditions.\n"\
	       "There is NO warranty; not even for MERCHANTABILITY or FITNESS\n"\
	       "FOR A PARTICULAR PURPOSE.\n"

#include <common.h>
#define REVPRINT() COLOR_PRINT(BLUE, OMAP_UBOOT_UTILS_REVISION)
#define LIC_PRINT() COLOR_PRINT(GREEN, OMAP_UBOOT_UTILS_LICENSE)

#endif /* __LIB_REV_H */
