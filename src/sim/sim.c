#include "sim.h"
#include "schnitt.h"
#include "../global.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const double MAX_VY = 8 * SIM_GRAVITY;
static const double MOV_DIST_MAX = 0.2;

sim *sim_create(int grows, int gcols)
{
    sim *ret = malloc(sizeof(sim));
    memset(ret, 0, sizeof(sim));
    ret->grows = grows;
    ret->gcols = gcols;
    ret->grid = malloc(grows * gcols * sizeof(sobj));
    memset(ret->grid, 0, grows * gcols * sizeof(sobj));

    ret->anim_cap = 16;
    ret->anim = malloc(ret->anim_cap * sizeof(sobj *));
    ret->anim_sz = 0;
    ret->block_cap = 16;
    ret->block = malloc(ret->block_cap * sizeof(sobj *));
    ret->block_sz = 0;
    ret->volat_cap = 16;
    ret->volat = malloc(ret->volat_cap * sizeof(sobj *));
    ret->volat_sz = 0;

    ret->last_land = -1e10;

    int i, j;
    for (i = 0; i < grows; ++i)
        for (j = 0; j < gcols; ++j) {
            sim_grid(ret, i, j).x = j;
            sim_grid(ret, i, j).y = i;
            sim_grid(ret, i, j).w = sim_grid(ret, i, j).h = 1;
            sim_grid(ret, i, j).t = -1;
        }
    return ret;
}

void sim_drop(sim *this)
{
    free(this->grid);
    free(this->anim);
    free(this->block);
    free(this->volat);
    free(this);
}

/* Adds a given object (usu. extra object) to the `block` list, if necessary */
void sim_check_block(sim *this, sobj *o)
{
    if (!sobj_needs_collision(o)) return;
    this->block[this->block_sz++] = o;
    if (this->block_sz == this->block_cap) {
        this->block_cap <<= 1;
        this->block = realloc(this->block, this->block_cap * sizeof(sobj *));
    }
}

/* Adds a given object (usu. grid cell) to the `volat` list, if necessary */
void sim_check_volat(sim *this, sobj *o)
{
    if (!sobj_needs_update(o)) return;
    this->volat[this->volat_sz++] = o;
    if (this->volat_sz == this->volat_cap) {
        this->volat_cap <<= 1;
        this->volat = realloc(this->volat, this->volat_cap * sizeof(sobj *));
    }
}

/* Adds a given object to the `anim` list; and `volat`, if necessary */
void sim_add(sim *this, sobj *o)
{
    sobj_init(o);
    this->anim[this->anim_sz++] = o;
    if (this->anim_sz == this->anim_cap) {
        this->anim_cap <<= 1;
        this->anim = realloc(this->anim, this->anim_cap * sizeof(sobj *));
    }
    sim_check_block(this, o);
    sim_check_volat(this, o);
}

/* Initialize all grid cells */
static inline void init_grid(sim *this)
{
    this->grid_initialized = true;
    int i, j;
    for (i = 0; i < this->grows; ++i)
        for (j = 0; j < this->gcols; ++j) {
            sobj_init(&sim_grid(this, i, j));
            sim_check_volat(this, &sim_grid(this, i, j));
        }
}

void sim_reinit(sim *this)
{
    init_grid(this);
    int i;
    for (i = 0; i < this->anim_sz; ++i)
        sobj_init(this->anim[i]);
}

/* Sends a rectangle to schnitt for checking. */
static inline bool apply_intsc(sim *this, sobj *o)
{
    return schnitt_apply(
        o->x - this->prot.x,
        o->y - this->prot.y,
        o->x - this->prot.x + o->w,
        o->y - this->prot.y + o->h);
}

/* Checks for intersections; **`schnitt_flush()` must be called afterwards**
 * if `inst` is true, the function will return on first intersection
 * if `mark_lands` is true, the `is_on` field will be updated */
static inline bool check_intsc(sim *this, bool inst, bool mark_lands)
{
    bool in = false, cur;
    /* Neighbouring cells */
    int px = (int)this->prot.x, py = (int)this->prot.y;
    int i, j;
    sobj *o = &sim_grid(this, py, px);
    for (i = 0; i <= 1; ++i)
        for (j = 0; j <= 1; ++j)
            if (px + j < this->gcols && py + i < this->grows) {
                o = &sim_grid(this, py + i, px + j);
                if (o->tag != 0) {
                    in |= (cur = apply_intsc(this, o));
                    if (mark_lands) o->is_on = cur;
                    if (inst && in) return true;
                }
            }
    for (i = 0; i < this->block_sz; ++i) {
        o = this->block[i];
        in |= (cur = apply_intsc(this, o));
        if (mark_lands) o->is_on = cur;
        if (inst && in) return true;
    }
    return in;
}

static inline bool check_intsc_mov(sim *this, double x, double y)
{
    this->prot.x = x;
    this->prot.y = y;
    bool in = check_intsc(this, true, false);
    schnitt_flush(NULL, NULL);
    return in;
}

void sim_tick(sim *this)
{
    if (!this->grid_initialized) init_grid(this);
    sobj_new_round();

    this->cur_time += SIM_STEPLEN;
    if (this->cur_time < 0) return;

    this->prot.vx += this->prot.ax * SIM_STEPLEN;
    this->prot.vy += (this->prot.ay + SIM_GRAVITY) * SIM_STEPLEN;
    if (this->prot.vy >= MAX_VY) this->prot.vy = MAX_VY;
    this->prot.x += this->prot.vx * SIM_STEPLEN;
    this->prot.y += this->prot.vy * SIM_STEPLEN;

    /* Update all objects, before collision detection */
    int i;
    for (i = 0; i < this->volat_sz; ++i) {
        sobj_update_pred(this->volat[i], this->cur_time, &this->prot);
        this->volat[i]->is_on = false;
    }

    /* Respond by movement */
    double x0 = this->prot.x, y0 = this->prot.y;
    bool in = check_intsc(this, false, true);
    double dx[8], dy[8];
    sobj *land_on = NULL;
    schnitt_flush(dx, dy);
    if (in) {
        int i, dir = -1;
        double min = 10;
        for (i = 0; i < 8; ++i) {
            if (i == 4 && dir != -1) break;
            if (fabs(dx[i]) > MOV_DIST_MAX || fabs(dy[i]) > MOV_DIST_MAX)
                continue;
            in = check_intsc_mov(this, x0 + dx[i], y0 + dy[i]);
            if (!in && dx[i] * dx[i] + dy[i] * dy[i] < min) {
                min = dx[i] * dx[i] + dy[i] * dy[i];
                dir = i;
            }
        }
        if (dir == -1) {
            /* Last struggles */
            for (i = 0; i < 4; ++i) {
                int xsgn = (i % 2 == 0 ? +1 : -1),
                    ysgn = (i / 2 == 0 ? +1 : -1);
                double lo = 0, hi = MOV_DIST_MAX, mid;
                if (!check_intsc_mov(this, x0 + hi * xsgn, y0 + hi * ysgn)) {
                    for (i = 0; i < 20; ++i) {
                        mid = (lo + hi) / 2;
                        if (!check_intsc_mov(this, x0 + mid * xsgn, y0 + mid * ysgn))
                            hi = mid;
                        else lo = mid;
                    }
                    this->prot.x = x0 + mid * xsgn;
                    this->prot.y = y0 + mid * ysgn;
                    this->prot.vx = this->prot.ax = 0;
                    this->prot.vy = this->prot.ay = 0;
                    dir = 8;
                }
            }
            if (dir == -1) {
                /* Most probably, the protagonist is stuck somewhere */
                this->prot.x = x0;
                this->prot.y = y0;
                this->prot.is_on = true;
                this->prot.tag = PROT_TAG_FAILURE;
                this->prot.t = this->cur_time;
            }
        } else {
            this->prot.x = x0 + dx[dir];
            this->prot.y = y0 + dy[dir];
            if ((dir == 1 && this->prot.vx > 0) ||
                (dir == 2 && this->prot.vx < 0) || dir >= 4)
            {
                this->prot.vx = this->prot.ax = 0;
            }
            if ((dir == 0 && this->prot.vy > 0) ||
                (dir == 3 && this->prot.vy < 0) || dir >= 4)
            {
                this->prot.vy = this->prot.ay = 0;
            }
            if (dir == 0) {
                this->last_land = this->cur_time;
            }
        }
    }

    /* Update all objects, after collision detection */
    for (i = 0; i < this->volat_sz; ++i)
        sobj_update_post(this->volat[i], this->cur_time, &this->prot);

    /* Sanitize */
    this->prot.x = clamp(this->prot.x, 0, this->gcols - this->prot.w);
    this->prot.y = clamp(this->prot.y, 0, this->grows - this->prot.h);

    /* Check for special actions */
    int px = (int)this->prot.x, py = (int)this->prot.y;
    if (sim_grid(this, py, px).tag == OBJID_NXSTAGE) {
        this->prot.is_on = true;
        this->prot.tag = PROT_TAG_NXSTAGE;
        this->prot.t = this->cur_time;
    }
}

/* Tells whether a landing will happen in a given amount of time.
 * This is only an approximation, but should be sufficient.
 * The only times when problems may arise is when the protagonist
 * is around the corners of moving platforms, in which case
 * mispredictions may cause an incorrect anticipated deluge but no
 * following jump. However, the player would continue to fall anyway. */
static inline bool sim_prophecy_partial(sim *this, double time)
{
    double x0 = this->prot.x, y0 = this->prot.y;
    this->prot.x += (this->prot.vx + this->prot.ax * time / 2) * time;
    this->prot.y += (this->prot.vy + this->prot.ay * time / 2) * time;
    /* Firstly, look for intersections */
    bool in = check_intsc(this, false, false);
    double dx[8], dy[8];
    schnitt_flush(dx, dy);
    if (in) {
        /* Checks whether it is a landing event
         * i.e. is it possible that the protagonist gets out
         * of the colliding object by upward movement (dir = 0, 4, 5) */
        in = !check_intsc_mov(this, x0 + dx[0], y0 + dy[0]) ||
            !check_intsc_mov(this, x0 + dx[4], y0 + dy[4]) ||
            !check_intsc_mov(this, x0 + dx[5], y0 + dy[5]);
    }
    this->prot.x = x0;
    this->prot.y = y0;
    return in;
}

bool sim_prophecy(sim *this, double time)
{
    if (sim_prophecy_partial(this, time)) return true;
    double vx0 = this->prot.vx, ax0 = this->prot.ax;
    this->prot.vx = 0;
    this->prot.ax = 0;
    bool ret = sim_prophecy_partial(this, time);
    this->prot.vx = vx0;
    this->prot.ax = ax0;
    return ret;
}
