#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "sdl_utils.h"

#define APP_NAME "MindBlock"

#define TEXTURE_WIDTH 32
#define TEXTURE_HEIGHT 32

// 12 by 12 tiles. Each tile having 16 pixels by 16 pixels.
#define APP_WIDTH 20 * TEXTURE_WIDTH
#define APP_HEIGHT 12 * TEXTURE_HEIGHT


static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

void renderGame(void);

int main(void)
{
    // Initialize SDL Systems.
    window = sdl_initialize_window(APP_NAME, APP_WIDTH, APP_HEIGHT);
    renderer = sdl_initialize_renderer(window);
    sdl_initialize_audio();

    

    // Game Loop
    int running = 1;
    const Uint32 FRAME_MS = 16; // ~60 FPS
    while (running)
    {
        Uint32 frame_start = SDL_GetTicks();

        renderGame();

        // Delay to maintain frame rate
        Uint32 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < FRAME_MS)
            SDL_Delay(FRAME_MS - elapsed);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}


void renderGame(void)
{
    const int charsize = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;

    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE); /* black, full alpha */
    SDL_RenderClear(renderer);                                   /* start with a blank canvas. */

    showText(renderer, 100, 0, "Potion Collector!", (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE});
    SDL_RenderDebugTextFormat(renderer, (float)((APP_WIDTH - (charsize * 46)) / 2), APP_HEIGHT - charsize, "(This program has been running for %" SDL_PRIu64 " seconds.)", SDL_GetTicks() / 1000);

    SDL_RenderPresent(renderer); /* put it all on the screen! */
}