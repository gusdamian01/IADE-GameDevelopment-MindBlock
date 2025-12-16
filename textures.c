#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "sdl_utils.h"
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h> 

// =====================
//      TEXTURES
// =====================
static SDL_Texture *playerTexture        = NULL;
static SDL_Texture *playerTexture2       = NULL;
static SDL_Texture *playerTexture3       = NULL;
static SDL_Texture *playerTexture4       = NULL;
static SDL_Texture *floorTexture         = NULL;
static SDL_Texture *puzzleTexture        = NULL;
static SDL_Texture *constraintTexture1   = NULL;
static SDL_Texture *constraintTexture2   = NULL;
static SDL_Texture *constraintTexture3   = NULL;
static SDL_Texture *constraintTexture4   = NULL;

// ✅ Main menu button textures
static SDL_Texture *startButtonTexture    = NULL;
static SDL_Texture *settingsButtonTexture = NULL;
static SDL_Texture *exitButtonTexture     = NULL;

static SDL_Texture *valueTextures[7]     = { NULL };