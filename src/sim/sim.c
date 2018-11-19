#include "sim.h"
#include "schnitt.h"

#include <stdlib.h>
#include <string.h>

#if DEBUG
#define debug printf
#else
#define debug(...)
#endif

const double SIM_GRAVITY = 3.5 * 1.414213562;
const double SIM_STEPLEN = 0.001;
static const double MAX_VY = 8 * SIM_GRAVITY;

sim *sim_create(int grows, int gcols)
{
    sim *ret = malloc(sizeof(sim));
    memset(ret, 0, sizeof(sim));
    ret->grows = grows;
    ret->gcols = gcols;
    ret->grid = malloc(grows * gcols * sizeof(sobj));
    memset(ret->grid, 0, grows * gcols * sizeof(sobj));
    /* TODO: Dynamic allocation! */
    ret->anim_cap = 16;
    ret->anim = malloc(ret->anim_cap * sizeof(sobj *));
    ret->anim_sz = 0;
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
    free(this->volat);
    free(this);
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
    this->anim[this->anim_sz++] = o;
    if (this->anim_sz == this->anim_cap) {
        this->anim_cap <<= 1;
        this->anim = realloc(this->anim, this->anim_cap * sizeof(sobj *));
    }
    sim_check_volat(this, o);
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
    sobj *o = &sim_grid(this, py, px);
    if (o->tag != 0) {
        in |= (cur = apply_intsc(this, o));
        if (mark_lands) o->is_on = cur;
        if (inst && in) return true;
    }
    if (px + 1 < this->gcols) {
        o = &sim_grid(this, py, px + 1);
        if (o->tag != 0) {
            in |= (cur = apply_intsc(this, o));
            if (mark_lands) o->is_on = cur;
            if (inst && in) return true;
        }
    }
    if (py + 1 < this->grows) {
        o = &sim_grid(this, py + 1, px);
        if (o->tag != 0) {
            in |= (cur = apply_intsc(this, o));
            if (mark_lands) o->is_on = cur;
            if (inst && in) return true;
        }
    }
    if (px + 1 < this->gcols && py + 1 < this->grows) {
        o = &sim_grid(this, py + 1, px + 1);
        if (o->tag != 0) {
            in |= (cur = apply_intsc(this, o));
            if (mark_lands) o->is_on = cur;
            if (inst && in) return true;
        }
    }
    for (px = 0; px < this->anim_sz; ++px) {
        o = this->anim[px];
        in |= (cur = apply_intsc(this, o));
        if (mark_lands) o->is_on = cur;
        if (inst && in) return true;
    }
    return in;
}

static inline void find_lands(sim *this, sobj **ls)
{
    /* Neighbouring cells */
    int px = (int)this->prot.x, py = (int)this->prot.y;
    int len = 0;
    sobj *o = &sim_grid(this, py, px);
    if (o->tag != 0 && apply_intsc(this, o)) ls[len++] = o;
    if (px + 1 < this->gcols) {
        o = &sim_grid(this, py, px + 1);
        if (o->tag != 0 && apply_intsc(this, o)) ls[len++] = o;
    }
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
    this->cur_time += SIM_STEPLEN;

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

    debug("\n<%.8lf %.8lf>\n", this->prot.x, this->prot.y);
    /* Respond by movement */
    double x0 = this->prot.x, y0 = this->prot.y;
    bool in = check_intsc(this, false, true);
    double dx[16], dy[16];
    sobj *land_on = NULL;
    schnitt_flush(dx, dy);
    if (in) {
        int i, dir = -1;
        double min = 10;
        for (i = 0; i < 8; ++i) {
            if (i == 4 && dir != -1) break;
            in = check_intsc_mov(this, x0 + dx[i], y0 + dy[i]);
            if (!in && dx[i] * dx[i] + dy[i] * dy[i] < min) {
                min = dx[i] * dx[i] + dy[i] * dy[i];
                dir = i;
            }
        }
        if (dir == -1) {
            puts("> <");
        } else {
            debug("= = [%d]\n", dir);
            this->prot.x = x0 + dx[dir];
            this->prot.y = y0 + dy[dir];
            if (dir == 1 || dir == 2 || dir >= 4)
                this->prot.vx = this->prot.ax = 0;
            if (dir == 0 || dir == 3 || dir >= 4)
                this->prot.vy = this->prot.ay = 0;
            if (dir == 0) {
                this->last_land = this->cur_time;
            }
        }
    }

    /* Update all objects, after collision detection */
    for (i = 0; i < this->volat_sz; ++i)
        sobj_update_post(this->volat[i], this->cur_time, &this->prot);
}

/* Tells whether a landing will happen in a given amount of time.
 * This is only an approximation, but should be sufficient.
 * The only times when problems may arise is when the protagonist
 * is around the corners of moving platforms, in which case
 * mispredictions may cause an incorrect anticipated deluge but no
 * following jump. However, the player would continue to fall anyway. */
bool sim_prophecy(sim *this, double time)
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
