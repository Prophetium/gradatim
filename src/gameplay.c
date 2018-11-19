#include "gameplay.h"

#include "global.h"

#include <math.h>

static const double UNIT_PX = 48;
static const double WIN_W_UNITS = (double)WIN_W / UNIT_PX;
static const double WIN_H_UNITS = (double)WIN_H / UNIT_PX;

static const double BEAT = 60.0 / 132;  /* Temporary */
#define HOP_SPD SIM_GRAVITY
static const double HOP_PRED_DUR = 0.2;
static const double HOP_GRACE_DUR = 0.15;
#define ANTHOP_DELUGE_SPD (6.0 * SIM_GRAVITY)
static const double HOR_SPD = 2;
static const double DASH_DUR = 1;
#define DASH_HOR_V0     (6.5 * 1.414213562)
#define DASH_HOR_ACCEL  (6.5 * 1.414213562)
#define DASH_VER_V0     (5.5 * 1.414213562)
#define DASH_VER_ACCEL  (5.5 * 1.414213562 - SIM_GRAVITY)
static const double DASH_DIAG_SCALE = 0.8;

static inline double clamp(double x, double l, double u)
{
    return (x < l ? l : (x > u ? u : x));
}

static void gameplay_scene_tick(gameplay_scene *this, double dt)
{
    double prev_vx = this->simulator->prot.vx;
    this->simulator->prot.vx = 
        (this->hor_state == HOR_STATE_LEFT) ? -HOR_SPD :
        (this->hor_state == HOR_STATE_RIGHT) ? +HOR_SPD : 0;
    this->simulator->prot.ay =
        (this->ver_state == VER_STATE_DOWN) ? 4.0 * SIM_GRAVITY : 0;
    double deluge_vy = this->simulator->prot.ay;
    if (this->mov_state == MOV_ANTHOP) {
        if ((this->mov_time -= dt / BEAT) <= 0) {
            /* Perform a jump */
            this->mov_state = MOV_NORMAL;
            this->simulator->prot.vy = -HOP_SPD;
        } else {
            /* Deluging */
            this->simulator->prot.ay = ANTHOP_DELUGE_SPD;
        }
    } else if (this->mov_state & MOV_DASH_BASE) {
        if ((this->mov_time -= dt / BEAT) <= 0) {
            /* XXX: Avoid duplicates? */
            this->mov_state = MOV_NORMAL;
            this->simulator->prot.ax = 0;
            this->simulator->prot.ay = 0;
        } else {
            if (this->mov_state & MOV_HORDASH & 3) {
                this->simulator->prot.vx = prev_vx;
                this->simulator->prot.ax =
                    (this->mov_state & MOV_DASH_LEFT) ?
                    +DASH_HOR_ACCEL : -DASH_HOR_ACCEL;
                this->simulator->prot.ay = -SIM_GRAVITY;
            }
            if (this->mov_state & MOV_VERDASH & 3) {
                this->simulator->prot.ay =
                    (this->mov_state & MOV_DASH_UP) ? DASH_VER_ACCEL : 0;
            }
            if (this->mov_state & 3) {
                this->simulator->prot.ax *= DASH_DIAG_SCALE;
                this->simulator->prot.ay *= DASH_DIAG_SCALE;
            }
            if (this->simulator->prot.vx * this->simulator->prot.ax >= 0)
                this->simulator->prot.ax = 0;
            if (this->simulator->prot.vy * this->simulator->prot.ay >= 0)
                this->simulator->prot.ay = 0;
            this->simulator->prot.ay += deluge_vy;
        }
    }

    double rt = this->rem_time + dt / BEAT;
    while (rt >= SIM_STEPLEN) {
        sim_tick(this->simulator);
        rt -= SIM_STEPLEN;
    }
    this->rem_time = rt;

    /* Move the camera */
    double dest_x = clamp(this->simulator->prot.x,
        WIN_W_UNITS / 2, this->simulator->gcols - WIN_W_UNITS / 2);
    double dest_y = clamp(this->simulator->prot.y,
        WIN_H_UNITS / 2, this->simulator->grows - WIN_H_UNITS / 2);
    double cam_dx = dest_x - (this->cam_x + WIN_W_UNITS / 2);
    double cam_dy = dest_y - (this->cam_y + WIN_H_UNITS / 2);
    double rate = (dt > 0.1 ? 0.1 : dt) * 10;
    this->cam_x += rate * cam_dx;
    this->cam_y += rate * cam_dy;
}

static void gameplay_scene_draw(gameplay_scene *this)
{
    SDL_SetRenderDrawColor(g_renderer, 216, 224, 255, 255);
    SDL_RenderClear(g_renderer);

    int rmin = floorf(this->cam_y),
        rmax = ceilf(this->cam_y + WIN_H_UNITS),
        cmin = floorf(this->cam_x),
        cmax = ceilf(this->cam_x + WIN_W_UNITS);
    int r, c;
    for (r = rmin; r < rmax; ++r)
        for (c = cmin; c < cmax; ++c) {
            sobj *o = &sim_grid(this->simulator, r, c);
            if (o->tag != 0) {
                render_texture(this->grid_tex[o->tag], &(SDL_Rect){
                    (o->x - this->cam_x) * UNIT_PX,
                    (o->y - this->cam_y) * UNIT_PX,
                    o->w * UNIT_PX, o->h * UNIT_PX
                });
            }
        }

    render_texture(this->prot_tex, &(SDL_Rect){
        (this->simulator->prot.x - this->cam_x) * UNIT_PX,
        (this->simulator->prot.y - this->cam_y) * UNIT_PX,
        round(this->simulator->prot.w * UNIT_PX),
        round(this->simulator->prot.h * UNIT_PX)
    });
}

static void gameplay_scene_drop(gameplay_scene *this)
{
    sim_drop(this->simulator);
}

static void try_hop(gameplay_scene *this)
{
    if ((this->simulator->cur_time - this->simulator->last_land) <= HOP_GRACE_DUR) {
        /* Grace jump */
        this->simulator->prot.vy = -HOP_SPD;
    } else if (sim_prophecy(this->simulator, HOP_PRED_DUR)) {
        /* Will land soon, deluge and then jump */
        this->mov_state = MOV_ANTHOP;
        /* Binary search on time needed till landing happens */
        double lo = 0, hi = HOP_PRED_DUR, mid;
        int i;
        for (i = 0; i < 10; ++i) {
            mid = (lo + hi) / 2;
            if (sim_prophecy(this->simulator, mid)) hi = mid;
            else lo = mid;
        }
        this->mov_time = hi;
    }
}

static void try_dash(gameplay_scene *this)
{
    int dir_has = 0, dir_denotes = 0;
    if (this->hor_state != HOR_STATE_NONE) {
        dir_has |= 1;
        dir_denotes |= (this->hor_state == HOR_STATE_LEFT ? MOV_DASH_LEFT : 0);
        this->simulator->prot.vx =
            (this->hor_state == HOR_STATE_LEFT ? -DASH_HOR_V0 : +DASH_HOR_V0);
    }
    if (this->ver_state == VER_STATE_UP) {
        dir_has |= 2;
        dir_denotes |= MOV_DASH_UP;
        this->simulator->prot.vy = -DASH_VER_V0;
    }
    if (dir_has == 0) {
        /* TODO: Dash in the last movement direction */
    } else if (dir_has == 3) {
        this->simulator->prot.vx *= DASH_DIAG_SCALE;
        this->simulator->prot.vy *= DASH_DIAG_SCALE;
    }
    this->simulator->last_land = -1e10; /* Disable grace jumps */
    this->mov_state = MOV_DASH_BASE | dir_has | dir_denotes;
    this->mov_time = DASH_DUR;
}

static void gameplay_scene_key_handler(gameplay_scene *this, SDL_KeyboardEvent *ev)
{
#define toggle(__thisstate, __keystate, __has, __none) do { \
    if ((__keystate) == SDL_PRESSED) (__thisstate) = (__has); \
    else if ((__thisstate) == (__has)) (__thisstate) = (__none); \
} while (0)

    if (ev->repeat) return;
    switch (ev->keysym.sym) {
        case SDLK_c:
            if (ev->state == SDL_PRESSED) try_hop(this);
            break;
        case SDLK_x:
            if (ev->state == SDL_PRESSED) try_dash(this);
            break;
        case SDLK_UP:
            toggle(this->ver_state, ev->state, VER_STATE_UP, VER_STATE_NONE);
            break;
        case SDLK_DOWN:
            toggle(this->ver_state, ev->state, VER_STATE_DOWN, VER_STATE_NONE);
            break;
        case SDLK_LEFT:
            toggle(this->hor_state, ev->state, HOR_STATE_LEFT, HOR_STATE_NONE);
            break;
        case SDLK_RIGHT:
            toggle(this->hor_state, ev->state, HOR_STATE_RIGHT, HOR_STATE_NONE);
            break;
        default: break;
    }
}

gameplay_scene *gameplay_scene_create(scene **bg)
{
    gameplay_scene *ret = malloc(sizeof(gameplay_scene));
    memset(ret, 0, sizeof(gameplay_scene));
    ret->_base.children = bekter_create();
    ret->_base.tick = (scene_tick_func)gameplay_scene_tick;
    ret->_base.draw = (scene_draw_func)gameplay_scene_draw;
    ret->_base.drop = (scene_drop_func)gameplay_scene_drop;
    ret->_base.key_handler = (scene_key_func)gameplay_scene_key_handler;
    ret->bg = *bg;
    ret->bg_ptr = bg;

    ret->simulator = sim_create(128, 128);
    ret->rem_time = 0;
    ret->prot_tex = retrieve_texture("4.png");
    ret->grid_tex[1] = retrieve_texture("4.png");
    ret->grid_tex[OBJID_SPRING] =
    ret->grid_tex[OBJID_FRAGILE] = retrieve_texture("1.png");
    ret->grid_tex[OBJID_FRAGILE + 1] = retrieve_texture("2.png");
    ret->grid_tex[OBJID_FRAGILE + 2] = retrieve_texture("3.png");
    ret->grid_tex[OBJID_FRAGILE + 3] = retrieve_texture("4.png");
    ret->cam_x = ret->cam_y = 100.0;

    int i;
    for (i = 50; i < 120; ++i)
        sim_grid(ret->simulator, 107, i).tag = (i >= 110 || i % 2 == 0);
    for (i = 0; i < 128; ++i) sim_grid(ret->simulator, i, 127).tag = 1;
    for (i = 0; i < 128; ++i) sim_grid(ret->simulator, 127, i).tag = 1;
    sim_grid(ret->simulator, 120, 125).tag = OBJID_SPRING;
    sim_grid(ret->simulator, 120, 125).data.t = -100;
    sim_grid(ret->simulator, 121, 125).tag = 1;
    for (i = 118; i < 124; ++i) sim_grid(ret->simulator, 121, i).tag = OBJID_FRAGILE;
    ret->simulator->prot.x = ret->simulator->prot.y = 116;
    ret->simulator->prot.w = ret->simulator->prot.h = 0.6;

    return ret;
}
