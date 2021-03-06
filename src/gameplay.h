/* The scene for gameplay */

#ifndef _GAMEPLAY_H
#define _GAMEPLAY_H

#include "resources.h"
#include "scene.h"
#include "sim/sim.h"
#include "game_data.h"
#include "mod.h"
#include "label.h"
#include "particle_sys.h"

typedef struct _gameplay_scene {
    scene _base;
    scene *bg;

    /* Struct holding all chapter-specific data */
    struct chap_rec *chap;
    int cur_stage_idx;
    /* Struct holding all stage-specific data
     * (objects, dialogues, textures etc.) */
    struct stage_rec *rec;
    sim *simulator, *prev_sim;
    double rem_time;
    bool paused;
    unsigned int dialogue_triggered;
    int dialogue_idx;   /* For delayed dialogue */

    /* For updating player's records */
    int start_stage_idx;
    double stage_start_time, total_time;
    int retry_count;

    /* Modifier states as a bitmask */
    int mods;
    /* Speed multiplier generated from mods mask */
    double mul;

    /* Display states */
    enum display_state {
        DISP_NORMAL,    /* Normal simulation and gameplay */
        DISP_LEADIN,    /* Flashlight lead in */
        DISP_DIALOGUE_IN,   /* With a dialogue overlay, zooming in */
        DISP_DIALOGUE_OUT,  /* With a dialogue overlay, zooming out */
        DISP_FAILURE,   /* Failure animation */
        DISP_CHAPFIN    /* End of chapter */
    } disp_state;
    double disp_time;   /* Remaining time of global animation, in seconds */
    SDL_Texture *leadin_tex;

    /* The position of the camera's top-left corner in the
     * simulated world, expressed in units */
    double cam_x, cam_y;
    /* Scales everything from a certain point */
    double scale;

    /* Input state */
    enum { HOR_STATE_NONE, HOR_STATE_LEFT, HOR_STATE_RIGHT } hor_state, facing;
    enum { VER_STATE_NONE, VER_STATE_UP, VER_STATE_DOWN } ver_state;
    /* Special movement states */
    enum movement_state {
        MOV_NORMAL = 0, MOV_ANTHOP = 1,
        /* Dash directions can be distinguished by taking & 3 */
        MOV_DASH_BASE = 4,
        MOV_HORDASH = MOV_DASH_BASE + 1,
        MOV_VERDASH = MOV_DASH_BASE + 2,
        MOV_DIAGDASH = MOV_DASH_BASE + 3,
        /* Bitmasks denoting dash direction */
        MOV_DASH_LEFT = 8,
        MOV_DASH_UP = 16,
        MOV_USING_REFILL = 32
    } mov_state;
    double mov_time;    /* Time remaining until movement state resets */
    double refill_time; /* Time until refill disappears; -1 if none persists */

    double since_hop;   /* Time since last hop; for the animation */

    /* Parallax sidescrollers */
    texture ss_tex[MAX_SIDESCROLLERS];

    /* Hints */
    label *l_hints[MAX_HINTS];
#define MAX_SIG 16
    sprite *s_hints[MAX_HINTS][MAX_SIG];
    int w_hints[MAX_HINTS]; /* Width of a horizontal slice */

    /* Particles */
    particle_sys particle;

    /* Speedrun clock */
    label *clock_stg, *clock_stg_dec;
    label *clock_chap, *clock_chap_dec;
} gameplay_scene;

gameplay_scene *gameplay_scene_create(scene *bg, struct chap_rec *chap, int idx, int mods);
void gameplay_init_textures(gameplay_scene *this);
void gameplay_run_leadin(gameplay_scene *this);

#endif
