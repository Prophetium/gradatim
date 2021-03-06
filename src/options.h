/* Options scene */

#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "scene.h"
#include "floue.h"
#include "label.h"

typedef struct _options_scene {
    scene _base;
    scene *bg;

    int menu_idx, last_menu_idx;
    double time, menu_time;

    floue *f;
#define OPTIONS_N_MENU  5
    int menu_val[OPTIONS_N_MENU];
    label *l_menuval[OPTIONS_N_MENU];
} options_scene;

options_scene *options_create(scene *bg);

#endif
