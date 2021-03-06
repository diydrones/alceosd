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

#include "alce-osd.h"

#define X_SIZE  100
#define Y_SIZE  35

/* range in degrees */
#define RANGE       90
#define MAJOR_TICK  45
#define MINOR_TICK  15
#define X_CENTER    (X_SIZE/2) - 2
#define Y_CENTER    (Y_SIZE/2) - 1

struct widget_priv {
    int heading;
    char heading_s[4];
};

static void render_callback(struct timer *t, void *d)
{
    struct widget *w = d;
    struct widget_priv *priv = w->priv;
    mavlink_vfr_hud_t *vfr_hud = mavdata_get(MAVLINK_MSG_ID_VFR_HUD);
    
    priv->heading = vfr_hud->heading;
    priv->heading_s[0] = '0' + (priv->heading / 100);
    priv->heading_s[1] = '0' + ((priv->heading % 100) / 10);
    priv->heading_s[2] = '0' + (priv->heading % 10);

    schedule_widget(w);
}

static int open(struct widget *w)
{
    struct widget_priv *priv;

    priv = (struct widget_priv*) widget_malloc(sizeof(struct widget_priv));
    if (priv == NULL)
        return -1;
    w->priv = priv;

    priv->heading_s[3] = '\0';
    w->ca.width = X_SIZE;
    w->ca.height = Y_SIZE;
    
    add_timer(TIMER_WIDGET, 250, render_callback, w);
    return 0;
}

static void render(struct widget *w)
{
    struct widget_priv *priv = w->priv;
    struct canvas *ca = &w->ca;
    const char cardinals[] = {'N', 'E', 'S', 'W'};
    int i, j, x;

    draw_jstr(priv->heading_s, X_CENTER, 0, JUST_HCENTER, ca, 1);
    set_pixel(X_CENTER, Y_CENTER-1, 1, ca);
    draw_hline(X_CENTER-1, X_CENTER+1, Y_CENTER-2, 1, ca);
    draw_hline(X_CENTER-2, X_CENTER+2, Y_CENTER-3, 1, ca);
    for(i = -RANGE / 2; i <= RANGE / 2; i++) {
        x = X_CENTER + i;
        j = (priv->heading + i + 360) % 360;
        if (j == 0 || j == 90 || j == 180 || j == 270) {
            draw_vline(x,   Y_CENTER, Y_CENTER + 5, 1, ca);
            draw_vline(x-1, Y_CENTER, Y_CENTER + 5, 3, ca);
            draw_vline(x+1, Y_CENTER, Y_CENTER + 5, 3, ca);
            draw_chr(cardinals[j / 90], x-3, Y_CENTER + 6, ca, 0);
        } else if(j % MAJOR_TICK == 0) {
            draw_vline(x,   Y_CENTER, Y_CENTER + 5, 1, ca);
            draw_vline(x-1, Y_CENTER, Y_CENTER + 5, 3, ca);
            draw_vline(x+1, Y_CENTER, Y_CENTER + 5, 3, ca);
        } else if(j % MINOR_TICK == 0) {
            draw_vline(x,   Y_CENTER, Y_CENTER + 3, 1, ca);
            draw_vline(x-1, Y_CENTER, Y_CENTER + 3, 3, ca);
            draw_vline(x+1, Y_CENTER, Y_CENTER + 3, 3, ca);
        }
    }
}


const struct widget_ops compass_widget_ops = {
    .name = "Compass",
    .mavname = "COMPASS",
    .id = WIDGET_COMPASS_ID,
    .init = NULL,
    .open = open,
    .render = render,
    .close = NULL,
};
