/*
    AlceOSD - Graphical OSD
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

#ifndef WIDGETS_H
#define	WIDGETS_H

enum {
    WIDGET_ALTITUDE_ID = 0,
    WIDGET_HORIZON_ID,
    WIDGET_BATTERY_INFO_ID,
    WIDGET_COMPASS_ID,
    WIDGET_FLIGHT_MODE_ID,
    WIDGET_GPS_INFO_ID,
    WIDGET_RSSI_ID,
    WIDGET_SPEED_ID,
    WIDGET_THROTTLE_ID,
    WIDGET_VARIOMETER_ID,
    WIDGET_RC_CHANNELS_ID,
};


#define JUST_TOP        0x0
#define JUST_BOT        0x1
#define JUST_VCENTER    0x2
#define JUST_LEFT       0x0
#define JUST_RIGHT      0x4
#define JUST_HCENTER    0x8

#define TABS_END        (0xffff)

enum {
    VJUST_TOP = 0,
    VJUST_BOT = 1,
    VJUST_CENTER = 2,
};

enum {
    HJUST_LEFT = 0,
    HJUST_RIGHT = 1,
    HJUST_CENTER = 2,
};

typedef union {
    unsigned int raw;
    struct {
        unsigned vjust:2;
        unsigned hjust:2;
        unsigned mode:4;
        unsigned :8;
    };
} widget_props;


struct widget_config {
    unsigned int tab;
    unsigned int widget_id;
    int x, y;
    widget_props props;
};


struct widget {
    unsigned int id;
    void (*init)(struct widget_config *wcfg);
    int (*render)(void);
    char name[];
};


void widgets_init(void);
void build_tab_list(void);
void load_tab(unsigned char tab);
void widgets_process(void);
void schedule_widget(const struct widget *w);
const struct widget *get_widget(unsigned int id);

#endif