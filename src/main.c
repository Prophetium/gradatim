#include "orion/orion.h"
#include "scene.h"
#include "transition.h"

#include <SDL.h>

#include <stdbool.h>

static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static scene *g_stage;

static void draw_loop()
{
    SDL_RenderClear(g_renderer);
    scene_tick(g_stage, 1.0 / 60);
    scene_draw(g_stage);
    SDL_RenderPresent(g_renderer);
}

int main()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    g_window = SDL_CreateWindow("",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        800, 600, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (g_window == NULL) return 1;

    g_renderer = SDL_CreateRenderer(
        g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == NULL) return 1;
    SDL_RenderSetLogicalSize(g_renderer, 1080, 720);

    /*struct orion o = orion_create(44100, 2);
    orion_load_ogg(&o, 0, "sketchch.ogg");
    orion_apply_stretch(&o, 0, 1, +3000);
    orion_play_loop(&o, 1, 0, 0, -1);
    orion_overall_play(&o);*/

    g_stage = colour_scene_create(g_renderer, 0, 192, 255);

    bool running = true;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN) {
                g_stage = transition_slidedown_create(
                    g_renderer, &g_stage,
                    colour_scene_create(g_renderer, 255, 192, 192), 1.0
                );
            }
        }
        draw_loop();
    }

    /*orion_drop(&o);*/
    SDL_Quit();

    return 0;
}