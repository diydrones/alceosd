 /*
    AlceOSD - UART bootloader
    Copyright (C) 2015  Luis Alves

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BOOTLOADER_H
#define	BOOTLOADER_H

#include <xc.h>
#include <string.h>
#include <p33Exxxx.h>
#include "flash.h"
#include "uart.h"
#include "bin_loader.h"


/* board config */
#define TRIS_LED                        TRISAbits.TRISA10
#define LED                             LATAbits.LATA10

#define FCY                             70000000


#endif	/* BOOTLOADER_H */

