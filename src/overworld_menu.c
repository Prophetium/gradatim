#include "overworld_menu.h"
#include "global.h"
#include "label.h"
#include "mod.h"
#include "loading.h"
#include "gameplay.h"

static const double MOV_X = 30;
static const double SCALE = 1.3;

static const double FADEIN_DUR = 0.3;
static const double MENU_W = WIN_W * 0.382;

static const double MENU_TR_DUR = 0.15;
static const double BLINK_DUR = 0.75;

static const double ITEM_H = 0.1;
static const double START_Y = 0.85;
static const double ITEM_OFFSET_Y = 0.4;

static int glob_menu_val[N_MODS] = { 0 };

static inline void owm_tick(overworld_menu *this, double dt)
{
    scene_tick(&this->bg->_base, dt);
    this->time += dt;
    if (this->quit_time >= 0 && this->time >= this->quit_time + FADEIN_DUR) {
        g_stage = (scene *)this->bg;
        scene_drop(this);
    }
}

static inline void owm_draw(overworld_menu *this)
{
    /* Underlying scene */
    scene_draw(&this->bg->_base);

    double t;
    if (this->time < FADEIN_DUR) {
        t = this->time / FADEIN_DUR;
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    } else if (this->time < this->quit_time + FADEIN_DUR) {
        t = (this->time - this->quit_time) / FADEIN_DUR;
        t = 1 - t * t * t;
    } else {
        t = 1;
    }
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, round(t * 192));

    /* Draw background */
    int delta_x = round(MENU_W / 2 * (1 - t));
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){
        round(WIN_W - MENU_W + delta_x), 0, WIN_W, WIN_H
    });

    /* Draw mod colour */
    int i;
    for (i = 0; i < N_MODS; ++i) {
        int c = MODS[i][this->menu_val[i]].colour;
        SDL_SetRenderDrawColor(g_renderer, c >> 16, (c >> 8) & 0xff, c & 0xff, round(t * t * 192));
        SDL_RenderFillRect(g_renderer, &(SDL_Rect){
            round(WIN_W - MENU_W + delta_x), WIN_H * (ITEM_OFFSET_Y - ITEM_H / 2 + i * ITEM_H),
            WIN_W, WIN_H * ITEM_H
        });
    }

    /* Draw highlight */
    double cur_y = (this->menu_idx == N_MODS ?
        (START_Y - ITEM_H / 2) : (ITEM_OFFSET_Y - ITEM_H / 2 + this->menu_idx * ITEM_H));
    if (this->time < this->menu_time + MENU_TR_DUR) {
        double p = (this->time - this->menu_time) / MENU_TR_DUR;
        p = 1 - (1 - p) * (1 - p) * (1 - p);
        double last_y = (this->last_menu_idx == N_MODS ?
            (START_Y - ITEM_H / 2) : (ITEM_OFFSET_Y - ITEM_H / 2 + this->last_menu_idx * ITEM_H));
        cur_y = last_y + p * (cur_y - last_y);
    }
    double phase = fabs(BLINK_DUR - fmod(this->time, BLINK_DUR * 2)) / BLINK_DUR;
    phase = ease_quad_inout(phase);
    int opacity = 128 + round(phase * 24);
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 128, opacity);
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){
        round(WIN_W - MENU_W + delta_x), round(cur_y * WIN_H), WIN_W, WIN_H * ITEM_H
    });

    /* Draw children */
    sprite *el;
    opacity = round(t * 255);
    for bekter_each(this->_base.children, i, el) {
        el->alpha = opacity;
        el->_base.dim.x += delta_x;
    }
    scene_draw_children((scene *)this);
    for bekter_each(this->_base.children, i, el) {
        el->_base.dim.x -= delta_x;
    }
}

static inline void owm_drop(overworld_menu *this)
{
}

static inline scene *run_stage(overworld_menu *this)
{
    int mods = 0, i;
    for (i = 0; i < N_MODS; ++i)
        mods |= (glob_menu_val[i]) << (i * 2);
    gameplay_scene *gp = gameplay_scene_create((scene *)this->bg,
        bekter_at(this->bg->chaps, this->bg->cur_chap_idx, struct chap_rec *),
        this->bg->cur_stage_idx, mods);
    return (scene *)gp;
}

static inline void init_stage(overworld_menu *this, gameplay_scene *gp)
{
    gameplay_run_leadin(gp);
    scene_drop(this);
}

static inline void owm_key(overworld_menu *this, SDL_KeyboardEvent *ev)
{
    if (ev->state != SDL_PRESSED) return;
    switch (ev->keysym.sym) {
        case SDLK_ESCAPE:
            if (this->quit_time < 0) {
                this->quit_time = this->time;
                this->bg->cam_targx -= MOV_X;
                this->bg->cam_targscale /= SCALE;
            }
            break;
        case SDLK_RETURN:
            if (this->menu_idx == N_MODS) {
                g_stage = (scene *)loading_create(&g_stage,
                    (loading_routine)run_stage, (loading_postroutine)init_stage, this);
                this->bg->cam_targx -= MOV_X;
                this->bg->cam_targscale /= SCALE;
            } else {
                ev->keysym.sym = SDLK_RIGHT;
                owm_key(this, ev);
            }
            break;
        case SDLK_UP:
            if (this->menu_idx > 0) {
                this->last_menu_idx = this->menu_idx;
                this->menu_idx--;
                this->menu_time = this->time;
            }
            break;
        case SDLK_DOWN:
            if (this->menu_idx < N_MODS) {
                this->last_menu_idx = this->menu_idx;
                this->menu_idx++;
                this->menu_time = this->time;
            }
            break;
        case SDLK_LEFT:
            this->menu_val[this->menu_idx] =
                (this->menu_val[this->menu_idx] - 1 + N_MODSTATES) % N_MODSTATES;
            break;
        case SDLK_RIGHT:
            this->menu_val[this->menu_idx] =
                (this->menu_val[this->menu_idx] + 1) % N_MODSTATES;
            break;
    }
    if (this->menu_idx == N_MODS) return;
    glob_menu_val[this->menu_idx] = this->menu_val[this->menu_idx];
    this->mod_icon[this->menu_idx]->tex =
        retrieve_texture(MODS[this->menu_idx][this->menu_val[this->menu_idx]].icon);
    label_set_text(this->mod_title[this->menu_idx],
        MODS[this->menu_idx][this->menu_val[this->menu_idx]].title);
    label_set_text(this->mod_desc[this->menu_idx],
        MODS[this->menu_idx][this->menu_val[this->menu_idx]].desc);
}

/* This is dumb and works only for 1~39, but who cares */
static inline void gen_roman_numeral(char *s, int n)
{
    while (n >= 10) {
        *(s++) = 'x';
        n -= 10;
    }
    if (n >= 5 && n <= 8) *(s++) = 'v';
    switch (n) {
        case 3: case 8: *(s++) = 'i';
        case 2: case 7: *(s++) = 'i';
        case 1: case 6: *(s++) = 'i';
        case 0: case 5: default: break;
        case 4: *(s++) = 'i'; *(s++) = 'v'; break;
        case 9: *(s++) = 'i'; *(s++) = 'x'; break;
    }
    *s = '\0';
}

overworld_menu *overworld_menu_create(overworld_scene *bg)
{
    overworld_menu *ret = malloc(sizeof(overworld_menu));
    ret->_base.children = bekter_create();
    ret->_base.tick = (scene_tick_func)owm_tick;
    ret->_base.draw = (scene_draw_func)owm_draw;
    ret->_base.drop = (scene_drop_func)owm_drop;
    ret->_base.key_handler = (scene_key_func)owm_key;
    ret->bg = bg;
    ret->time = 0;
    ret->quit_time = -1;
    ret->menu_idx = ret->last_menu_idx = N_MODS;
    ret->menu_time = 0;

    char s[64];
    strcpy(s, "Chapter X - ");
    s[8] = '1' + bg->cur_chap_idx;
    gen_roman_numeral(s + 12, bg->cur_stage_idx + 1);
    label *l = label_create("KiteOne-Regular.ttf", 40,
        (SDL_Color){255, 255, 255}, WIN_W, s);
    element_place_anchored((element *)l, WIN_W - 12, 12, 1, 0);
    bekter_pushback(ret->_base.children, l);

    sprite *sp = sprite_create("clock.png");
    sp->_base.dim.w /= 2;
    sp->_base.dim.h /= 2;
    element_place_anchored((element *)sp, WIN_W - MENU_W + 24, WIN_H * 0.175, 0, 0.5);
    bekter_pushback(ret->_base.children, sp);

    sp = sprite_create("retry_count.png");
    sp->_base.dim.w /= 2;
    sp->_base.dim.h /= 2;
    element_place_anchored((element *)sp, WIN_W - MENU_W + 24, WIN_H * 0.275, 0, 0.5);
    bekter_pushback(ret->_base.children, sp);

    l = label_create("KiteOne-Regular.ttf", 32,
        (SDL_Color){255, 255, 255}, WIN_W, "02:50:52");
    element_place_anchored((element *)l,
        WIN_W - MENU_W + WIN_W / 10, WIN_H * 0.175, 0, 0.5);
    bekter_pushback(ret->_base.children, l);

    l = label_create("KiteOne-Regular.ttf", 32,
        (SDL_Color){255, 255, 255}, WIN_W, "1665");
    element_place_anchored((element *)l,
        WIN_W - MENU_W + WIN_W / 10, WIN_H * 0.275, 0, 0.5);
    bekter_pushback(ret->_base.children, l);

    int i;
    for (i = 0; i < N_MODS; ++i) {
        ret->menu_val[i] = glob_menu_val[i];

        sp = sprite_create(MODS[i][ret->menu_val[i]].icon);
        sp->_base.dim.w /= 2;
        sp->_base.dim.h /= 2;
        element_place_anchored((element *)sp, WIN_W - MENU_W + 24, WIN_H * (ITEM_OFFSET_Y + i * ITEM_H), 0, 0.5);
        bekter_pushback(ret->_base.children, sp);
        ret->mod_icon[i] = sp;

        l = label_create("KiteOne-Regular.ttf", 28,
            (SDL_Color){255, 255, 255}, WIN_W, MODS[i][ret->menu_val[i]].title);
        element_place_anchored((element *)l,
            WIN_W - MENU_W + WIN_W / 10, WIN_H * (ITEM_OFFSET_Y + i * ITEM_H), 0, 0.8);
        bekter_pushback(ret->_base.children, l);
        ret->mod_title[i] = l;

        l = label_create("KiteOne-Regular.ttf", 18,
            (SDL_Color){255, 255, 255}, WIN_W, MODS[i][ret->menu_val[i]].desc);
        element_place_anchored((element *)l,
            WIN_W - MENU_W + WIN_W / 10, WIN_H * (ITEM_OFFSET_Y + i * ITEM_H), 0, -0.2);
        bekter_pushback(ret->_base.children, l);
        ret->mod_desc[i] = l;
    }

    l = label_create("KiteOne-Regular.ttf", 40,
        (SDL_Color){255, 255, 255}, WIN_W, "Start");
    element_place_anchored((element *)l,
        WIN_W - MENU_W / 2, WIN_H * START_Y, 0.5, 0.5);
    bekter_pushback(ret->_base.children, l);

    bg->cam_targx += MOV_X;
    bg->cam_targscale *= SCALE;

    return ret;
}