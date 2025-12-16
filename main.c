#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "sdl_utils.h"
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h> // for PRIu64

// =====================
//      TEXTURES
// =====================
static SDL_Texture *playerTexture = NULL;
static SDL_Texture *playerTexture2 = NULL;
static SDL_Texture *playerTexture3 = NULL;
static SDL_Texture *playerTexture4 = NULL;
static SDL_Texture *floorTexture = NULL;
static SDL_Texture *puzzleTexture = NULL;
static SDL_Texture *constraintTexture1 = NULL;
static SDL_Texture *constraintTexture2 = NULL;
static SDL_Texture *constraintTexture3 = NULL;
static SDL_Texture *constraintTexture4 = NULL;
static SDL_Texture *constraintValueTexture = NULL;
static SDL_Texture *checkTexture = NULL;

// ✅ Main menu button textures
static SDL_Texture *startButtonTexture = NULL;
static SDL_Texture *settingsButtonTexture = NULL;
static SDL_Texture *exitButtonTexture = NULL;

static SDL_Texture *valueTextures[7] = {NULL};

// =====================
//        AUDIO
// =====================
typedef struct
{
    SDL_AudioStream *stream;
    SDL_AudioSpec wavSpec;
    Uint8 *wavData;
    Uint32 wavLen;
    bool loaded;
} SoundEffect;

static SDL_AudioDeviceID audioDevice = 0;
static SDL_AudioSpec deviceSpec;

static SoundEffect sfxSteps;
static SoundEffect sfxPickup;
static SoundEffect sfxRotate;
static SoundEffect sfxLevelComplete;

static bool load_sound_effect(SoundEffect *sfx, const char *path);
static void play_sound_effect(SoundEffect *sfx);
static void destroy_sound_effect(SoundEffect *sfx);

static bool init_sound_effects(void);
static void shutdown_sound_effects(void);

// CONFIGURATIONS
#define MAP_ROWS 12
#define MAP_COLS 20
#define MAX_PIECES 10
#define MAP_LAYERS (1 + MAX_PIECES)

#define LAYER_WORLD 0
#define LAYER_PIECE_BASE 1
#define PIECE_LAYER(i) (LAYER_PIECE_BASE + (i))

// Tile codes
#define TILE_EMPTY '\0'
#define TILE_WALL 'W'
#define TILE_FLOOR 'F'
#define TILE_PUZZLE 'P' // the general puzzle area background (4x4)

#define APP_NAME "MindBlock"

// TILE SIZE:
#define TEXTURE_WIDTH 64
#define TEXTURE_HEIGHT 64

// Window size derived from map dimensions
#define APP_WIDTH (MAP_COLS * TEXTURE_WIDTH)   // 20 * 64 = 1280
#define APP_HEIGHT (MAP_ROWS * TEXTURE_HEIGHT) // 12 * 64 = 768

#define MAX_LEVELS 5

// GAME STATES
typedef enum
{
    STATE_MENU,
    STATE_PLAYING,
    STATE_SETTINGS,
    STATE_INSTRUCTIONS, // ✅ NEW
    STATE_CONFIRM_EXIT,
    STATE_LEVEL_COMPLETE
} GameState;

// STRUCTURES
struct Player
{
    int position_x;
    int position_y;
    bool controllingPiece;
    char controlledPieceId;
    char direction;
};

struct Piece
{
    char id;
    int size;
    int tiles[4][2]; // relative positions to base
    int values[4];   // per-tile values
    int baseX;
    int baseY;
    bool placed;
};

typedef struct
{
    int targetSum;
} ConstraintRegion;

typedef struct
{
    int threshold;
    bool greater;
} PinkConstraint;

// GLOBALS
char map[MAP_LAYERS][MAP_ROWS][MAP_COLS];
struct Player player = {5, 5, false, '\0', 'S'};
struct Piece pieces[MAX_PIECES];
int numPieces = 0;

ConstraintRegion orangeConstraint;
ConstraintRegion purpleConstraint;
PinkConstraint pinkConstraint;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

// Current level:
int currentLevel = 1;

// Game state & menu selections
GameState currentState = STATE_MENU;
int mainMenuSelection = 0;      // 0: Play, 1: Settings, 2: Exit
int settingsSelection = 0;      // ✅ NOW: 0: Instructions, 1: Brightness, 2: Return
int confirmSelection = 0;       // 0: Yes, 1: No
int levelCompleteSelection = 0; // 0: Next Level, 1: Back to Main Menu

// Brightness (0.2 - 1.0)
float brightness = 1.0f;

// =====================
//  FUNCTION DECLARATIONS
// =====================
void init_world_layer(void);
void puzzle_area(void);
void init_constraints(int level);
void initPieces(int level);
void printMap(void);

void movePlayer(char dir);
void interact(void);

void placePieceOnMap(int index);
void removePieceFromMap(int index);
bool canPlace(int index);
bool canMovePiece(int index, int dx, int dy);
void movePiece(int index, int dx, int dy);
void rotatePiece(int index);
int findPieceIndexById(char id);
void get_puzzle_origin(int *px0, int *py0);

bool is_tile_in_puzzle_area(int x, int y);
bool is_tile_in_orange(int x, int y);
bool is_tile_in_purple(int x, int y);
bool is_tile_in_green(int x, int y); // green "=" region
bool is_tile_in_pink(int x, int y);  // pink "< or >" region

int sumTilesInOrange(void);
int sumTilesInPurple(void);
int sumTilesInPink(void);

bool greenConstraintSatisfied(void);
bool pinkConstraintSatisfied(void);
bool constraintsSatisfied(void);

bool allPiecesFitInPuzzleArea(void);
void findPieceAndTileAt(int x, int y, int *pieceIndex, int *tileIndex);
void reset_player(void);

// Menus & brightness
void load_value_textures(void);
void apply_brightness_to_textures(void);
void renderMainMenu(void);
void renderSettings(void);
void renderInstructions(void); // ✅ NEW
void renderConfirmExit(void);
void renderLevelComplete(void);
void startGame(void);

// Helpers
static inline bool inBounds(int x, int y)
{
    return (x >= 0 && x < MAP_ROWS && y >= 0 && y < MAP_COLS);
}
static inline char get_top_tile(int x, int y)
{
    for (int l = MAP_LAYERS - 1; l >= 0; --l)
    {
        char t = map[l][x][y];
        if (t != TILE_EMPTY)
            return t;
    }
    return TILE_EMPTY;
}
static inline void set_tile(int layer, int x, int y, char t)
{
    if (inBounds(x, y))
        map[layer][x][y] = t;
}

// =====================
//   CONSTRAINT CHECKS
// =====================
int sumTilesInOrange(void)
{
    int sum = 0;
    for (int i = 0; i < numPieces; i++)
    {
        struct Piece p = pieces[i];
        for (int t = 0; t < p.size; t++)
        {
            int x = p.baseX + p.tiles[t][0];
            int y = p.baseY + p.tiles[t][1];
            if (is_tile_in_orange(x, y))
                sum += p.values[t];
        }
    }
    return sum;
}

int sumTilesInPurple(void)
{
    int sum = 0;
    for (int i = 0; i < numPieces; i++)
    {
        struct Piece p = pieces[i];
        for (int t = 0; t < p.size; t++)
        {
            int x = p.baseX + p.tiles[t][0];
            int y = p.baseY + p.tiles[t][1];
            if (is_tile_in_purple(x, y))
                sum += p.values[t];
        }
    }
    return sum;
}

int sumTilesInPink(void)
{
    int sum = 0;
    for (int i = 0; i < numPieces; i++)
    {
        struct Piece p = pieces[i];
        for (int t = 0; t < p.size; t++)
        {
            int x = p.baseX + p.tiles[t][0];
            int y = p.baseY + p.tiles[t][1];
            if (is_tile_in_pink(x, y))
                sum += p.values[t];
        }
    }
    return sum;
}

bool greenConstraintSatisfied(void)
{
    if (currentLevel != 3 && currentLevel != 5)
        return true;

    bool foundFirst = false;
    int firstVal = 0;

    for (int i = 0; i < numPieces; i++)
    {
        struct Piece p = pieces[i];
        for (int t = 0; t < p.size; t++)
        {
            int x = p.baseX + p.tiles[t][0];
            int y = p.baseY + p.tiles[t][1];
            if (is_tile_in_green(x, y))
            {
                int v = p.values[t];
                if (!foundFirst)
                {
                    foundFirst = true;
                    firstVal = v;
                }
                else
                {
                    if (v != firstVal)
                        return false;
                }
            }
        }
    }

    return true;
}

bool pinkConstraintSatisfied(void)
{
    if (currentLevel != 4 && currentLevel != 5)
        return true;

    int s = sumTilesInPink();
    if (pinkConstraint.greater)
    {
        return (s > pinkConstraint.threshold);
    }
    else
    {
        return (s < pinkConstraint.threshold);
    }
}

bool constraintsSatisfied(void)
{
    if (sumTilesInOrange() != orangeConstraint.targetSum)
        return false;
    if (sumTilesInPurple() != purpleConstraint.targetSum)
        return false;

    if (!greenConstraintSatisfied())
        return false;
    if (!pinkConstraintSatisfied())
        return false;

    return true;
}

// =====================
//        MAIN
// =====================
int main(void)
{
    window = sdl_initialize_window(APP_NAME, APP_WIDTH, APP_HEIGHT);
    renderer = sdl_initialize_renderer(window);
    sdl_initialize_audio();

    srand((unsigned)time(NULL));

    init_world_layer();
    puzzle_area();
    init_constraints(currentLevel);
    initPieces(currentLevel);
    reset_player();

    // Load Sprites
    playerTexture = sdl_load_texture(renderer, "sprites/Joe.png");
    playerTexture2 = sdl_load_texture(renderer, "sprites/JoeLeft.png");
    playerTexture3 = sdl_load_texture(renderer, "sprites/JoeRight.png");
    playerTexture4 = sdl_load_texture(renderer, "sprites/JoeUp.png");
    floorTexture = sdl_load_texture(renderer, "sprites/PlayArea.png");
    puzzleTexture = sdl_load_texture(renderer, "sprites/PuzzleArea.png");
    constraintTexture1 = sdl_load_texture(renderer, "sprites/Constraint1.png");
    constraintTexture2 = sdl_load_texture(renderer, "sprites/Constraint2.png");
    constraintTexture3 = sdl_load_texture(renderer, "sprites/Constraint3.png"); // GREEN "="
    constraintTexture4 = sdl_load_texture(renderer, "sprites/Constraint4.png"); // PINK < >
    constraintValueTexture = sdl_load_texture(renderer, "sprites/ConstraintValue.png");
    checkTexture = sdl_load_texture(renderer, "sprites/CheckMark.png");

    // Main menu buttons
    startButtonTexture = sdl_load_texture(renderer, "sprites/StartButton.png");
    settingsButtonTexture = sdl_load_texture(renderer, "sprites/SettingsButton.png");
    exitButtonTexture = sdl_load_texture(renderer, "sprites/ExitButton.png");

    // Load tile-value textures
    load_value_textures();

    // Apply initial brightness
    apply_brightness_to_textures();

    if (!init_sound_effects())
    {
        SDL_Log("Warning: sound effects failed to initialize: %s", SDL_GetError());
    }

    currentState = STATE_MENU;
    mainMenuSelection = 0;
    settingsSelection = 0;
    confirmSelection = 0;
    levelCompleteSelection = 0;

    int running = 1;
    const Uint32 FRAME_MS = 16; // ~60 FPS
    while (running)
    {
        Uint64 frame_start = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                running = 0;

            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                SDL_Keycode key = event.key.key;

                // ===== MAIN MENU =====
                if (currentState == STATE_MENU)
                {
                    int numItems = 3;
                    if (key == SDLK_W || key == SDLK_UP)
                    {
                        mainMenuSelection = (mainMenuSelection - 1 + numItems) % numItems;
                    }
                    else if (key == SDLK_S || key == SDLK_DOWN)
                    {
                        mainMenuSelection = (mainMenuSelection + 1) % numItems;
                    }
                    else if (key == SDLK_RETURN || key == SDLK_SPACE)
                    {
                        if (mainMenuSelection == 0)
                            startGame();
                        else if (mainMenuSelection == 1)
                        {
                            currentState = STATE_SETTINGS;
                            settingsSelection = 0;
                        }
                        else if (mainMenuSelection == 2)
                        {
                            currentState = STATE_CONFIRM_EXIT;
                            confirmSelection = 0;
                        }
                    }
                }

                // ===== SETTINGS =====
                else if (currentState == STATE_SETTINGS)
                {
                    int numItems = 3; // ✅ Instructions, Brightness, Return

                    if (key == SDLK_W || key == SDLK_UP)
                    {
                        settingsSelection = (settingsSelection - 1 + numItems) % numItems;
                    }
                    else if (key == SDLK_S || key == SDLK_DOWN)
                    {
                        settingsSelection = (settingsSelection + 1) % numItems;
                    }
                    // Brightness adjust only when "Brightness" is selected (index 1)
                    else if ((key == SDLK_A || key == SDLK_LEFT) && settingsSelection == 1)
                    {
                        brightness -= 0.1f;
                        if (brightness < 0.2f)
                            brightness = 0.2f;
                        apply_brightness_to_textures();
                    }
                    else if ((key == SDLK_D || key == SDLK_RIGHT) && settingsSelection == 1)
                    {
                        brightness += 0.1f;
                        if (brightness > 1.0f)
                            brightness = 1.0f;
                        apply_brightness_to_textures();
                    }
                    else if (key == SDLK_RETURN || key == SDLK_SPACE)
                    {
                        if (settingsSelection == 0)
                        {
                            currentState = STATE_INSTRUCTIONS;
                        }
                        else if (settingsSelection == 2)
                        {
                            currentState = STATE_MENU;
                        }
                    }
                    else if (key == SDLK_ESCAPE)
                    {
                        currentState = STATE_MENU;
                    }
                }

                // ===== INSTRUCTIONS PAGE =====
                else if (currentState == STATE_INSTRUCTIONS)
                {
                    if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_ESCAPE)
                    {
                        currentState = STATE_SETTINGS;
                    }
                }

                // ===== CONFIRM EXIT =====
                else if (currentState == STATE_CONFIRM_EXIT)
                {
                    if (key == SDLK_W || key == SDLK_S ||
                        key == SDLK_UP || key == SDLK_DOWN ||
                        key == SDLK_A || key == SDLK_D ||
                        key == SDLK_LEFT || key == SDLK_RIGHT)
                    {
                        confirmSelection = 1 - confirmSelection;
                    }
                    else if (key == SDLK_RETURN || key == SDLK_SPACE)
                    {
                        if (confirmSelection == 0)
                            running = 0;
                        else
                            currentState = STATE_MENU;
                    }
                    else if (key == SDLK_ESCAPE)
                    {
                        currentState = STATE_MENU;
                    }
                }

                // ===== LEVEL COMPLETE =====
                else if (currentState == STATE_LEVEL_COMPLETE)
                {
                    if (key == SDLK_W || key == SDLK_S ||
                        key == SDLK_UP || key == SDLK_DOWN)
                    {
                        levelCompleteSelection = 1 - levelCompleteSelection;
                    }
                    else if (key == SDLK_RETURN || key == SDLK_SPACE)
                    {
                        if (levelCompleteSelection == 0)
                        {
                            if (currentLevel < MAX_LEVELS)
                            {
                                currentLevel++;
                                init_world_layer();
                                puzzle_area();
                                init_constraints(currentLevel);
                                initPieces(currentLevel);
                                reset_player();
                                currentState = STATE_PLAYING;
                            }
                            else
                            {
                                currentLevel = 1;
                                init_world_layer();
                                puzzle_area();
                                init_constraints(currentLevel);
                                initPieces(currentLevel);
                                reset_player();
                                currentState = STATE_MENU;
                            }
                        }
                        else
                        {
                            currentLevel = 1;
                            init_world_layer();
                            puzzle_area();
                            init_constraints(currentLevel);
                            initPieces(currentLevel);
                            reset_player();
                            currentState = STATE_MENU;
                        }
                    }
                    else if (key == SDLK_ESCAPE)
                    {
                        currentLevel = 1;
                        init_world_layer();
                        puzzle_area();
                        init_constraints(currentLevel);
                        initPieces(currentLevel);
                        reset_player();
                        currentState = STATE_MENU;
                    }
                }

                // ===== PLAYING =====
                else if (currentState == STATE_PLAYING)
                {
                    if (key == SDLK_W || key == SDLK_A || key == SDLK_S || key == SDLK_D)
                    {
                        play_sound_effect(&sfxSteps);
                    }

                    if (key == SDLK_W)
                        movePlayer('W');
                    if (key == SDLK_A)
                        movePlayer('A');
                    if (key == SDLK_S)
                        movePlayer('S');
                    if (key == SDLK_D)
                        movePlayer('D');

                    if (player.controllingPiece)
                    {
                        int index = findPieceIndexById(player.controlledPieceId);
                        if (index == -1)
                            continue;

                        if (key == SDLK_Q)
                        {
                            player.controllingPiece = false;
                            player.controlledPieceId = '\0';
                            player.position_x = pieces[index].baseX;
                            player.position_y = pieces[index].baseY;
                            printf("You placed the piece and returned to Joe form.\n");

                            if (allPiecesFitInPuzzleArea() && constraintsSatisfied())
                            {
                                printf("🎉 Level %d Complete!\n", currentLevel);
                                play_sound_effect(&sfxLevelComplete);

                                currentState = STATE_LEVEL_COMPLETE;
                                levelCompleteSelection = 0;
                            }
                        }
                        else if (key == SDLK_R)
                        {
                            play_sound_effect(&sfxRotate);

                            removePieceFromMap(index);
                            rotatePiece(index);
                            if (!canPlace(index))
                            {
                                for (int i = 0; i < 3; i++)
                                    rotatePiece(index);
                            }
                            placePieceOnMap(index);
                        }
                        else
                        {
                            int dx = 0, dy = 0;
                            if (key == SDLK_D)
                                dy = 1;
                            else if (key == SDLK_A)
                                dy = -1;
                            else if (key == SDLK_S)
                                dx = 1;
                            else if (key == SDLK_W)
                                dx = -1;

                            if (dx != 0 || dy != 0)
                            {
                                if (canMovePiece(index, dx, dy))
                                {
                                    removePieceFromMap(index);
                                    movePiece(index, dx, dy);
                                    placePieceOnMap(index);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (key == SDLK_E)
                        {
                            play_sound_effect(&sfxPickup);
                            interact();
                        }
                    }
                }
            }
        }

        // ===== STATE-BASED RENDER =====
        if (currentState == STATE_PLAYING)
        {
            printMap();
        }
        else if (currentState == STATE_MENU)
        {
            renderMainMenu();
        }
        else if (currentState == STATE_SETTINGS)
        {
            renderSettings();
        }
        else if (currentState == STATE_INSTRUCTIONS)
        {
            renderInstructions();
        }
        else if (currentState == STATE_CONFIRM_EXIT)
        {
            renderConfirmExit();
        }
        else if (currentState == STATE_LEVEL_COMPLETE)
        {
            renderLevelComplete();
        }

        Uint64 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < FRAME_MS)
            SDL_Delay((Uint32)(FRAME_MS - elapsed));
    }

    for (int i = 0; i < 7; ++i)
    {
        if (valueTextures[i])
        {
            SDL_DestroyTexture(valueTextures[i]);
            valueTextures[i] = NULL;
        }
    }

    if (startButtonTexture)
    {
        SDL_DestroyTexture(startButtonTexture);
        startButtonTexture = NULL;
    }
    if (settingsButtonTexture)
    {
        SDL_DestroyTexture(settingsButtonTexture);
        settingsButtonTexture = NULL;
    }
    if (exitButtonTexture)
    {
        SDL_DestroyTexture(exitButtonTexture);
        exitButtonTexture = NULL;
    }

    shutdown_sound_effects();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

// =====================
//   SDL HELPER: LOAD VALUE TILE TEXTURES
// =====================
void load_value_textures(void)
{
    const char *paths[7] = {
        "sprites/Tile1.png",
        "sprites/Tile2.png",
        "sprites/Tile3.png",
        "sprites/Tile4.png",
        "sprites/Tile5.png",
        "sprites/Tile6.png",
        "sprites/Tile7.png"};

    for (int i = 0; i < 7; ++i)
    {
        valueTextures[i] = sdl_load_texture(renderer, paths[i]);
        if (!valueTextures[i])
        {
            SDL_Log("Failed to load value texture %d from '%s'", i + 1, paths[i]);
        }
    }
}

void apply_brightness_to_textures(void)
{
    Uint8 mod = (Uint8)(brightness * 255.0f);

    SDL_Texture *texList[] = {
        playerTexture, playerTexture2, playerTexture3, playerTexture4,
        floorTexture, puzzleTexture};

    int texCount = (int)(sizeof(texList) / sizeof(texList[0]));
    for (int i = 0; i < texCount; ++i)
    {
        if (texList[i])
            SDL_SetTextureColorMod(texList[i], mod, mod, mod);
    }

    for (int i = 0; i < 7; ++i)
    {
        if (valueTextures[i])
            SDL_SetTextureColorMod(valueTextures[i], mod, mod, mod);
    }
}

// =====================
//     RENDERING
// =====================
void printMap(void)
{
    const int charsize = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    for (int x = 0; x < MAP_ROWS; x++)
    {
        for (int y = 0; y < MAP_COLS; y++)
        {
            float pixelX = (float)(y * TEXTURE_WIDTH);
            float pixelY = (float)(x * TEXTURE_HEIGHT);

            int pIdx, tIdx;
            findPieceAndTileAt(x, y, &pIdx, &tIdx);

            if (pIdx != -1 && tIdx != -1)
            {
                int val = pieces[pIdx].values[tIdx];
                if (val < 1)
                    val = 1;
                if (val > 7)
                    val = 7;

                SDL_Texture *tex = valueTextures[val - 1];
                SDL_FRect valRect = {pixelX, pixelY, TEXTURE_WIDTH, TEXTURE_HEIGHT};

                if (tex != NULL)
                    SDL_RenderTexture(renderer, tex, NULL, &valRect);
                else
                    SDL_RenderTexture(renderer, floorTexture, NULL, &valRect);
                continue;
            }

            char top = map[LAYER_WORLD][x][y];

            if (top == TILE_PUZZLE)
            {
                SDL_FRect floorRect = {pixelX, pixelY, TEXTURE_WIDTH, TEXTURE_HEIGHT};
                SDL_RenderTexture(renderer, puzzleTexture, NULL, &floorRect);
            }
            else
            {
                SDL_FRect floorRect = {pixelX, pixelY, TEXTURE_WIDTH, TEXTURE_HEIGHT};
                SDL_RenderTexture(renderer, floorTexture, NULL, &floorRect);
            }
        }
    }

    if (!player.controllingPiece)
    {
        float pixelX = (float)(player.position_y * TEXTURE_WIDTH);
        float pixelY = (float)(player.position_x * TEXTURE_HEIGHT);

        SDL_FRect playerRect = {pixelX, pixelY, TEXTURE_WIDTH, TEXTURE_HEIGHT};

        if (player.direction == 'S')
            SDL_RenderTexture(renderer, playerTexture, NULL, &playerRect);
        else if (player.direction == 'A')
            SDL_RenderTexture(renderer, playerTexture2, NULL, &playerRect);
        else if (player.direction == 'D')
            SDL_RenderTexture(renderer, playerTexture3, NULL, &playerRect);
        else if (player.direction == 'W')
            SDL_RenderTexture(renderer, playerTexture4, NULL, &playerRect);
        else

            SDL_RenderTexture(renderer, playerTexture, NULL, &playerRect);
    }

    // Ugly but works.
    int highest_purple_x = -1;
    int highest_purple_y = -1;
    int highest_orange_x = -1;
    int highest_orange_y = -1;
    int highest_green_x = -1;
    int highest_green_y = -1;
    int highest_pink_x = -1;
    int highest_pink_y = -1;

    for (int x = 0; x < MAP_ROWS; x++)
    {
        for (int y = 0; y < MAP_COLS; y++)
        {
            float pixelX = (float)(y * TEXTURE_WIDTH);
            float pixelY = (float)(x * TEXTURE_HEIGHT);
            SDL_FRect cRect = {pixelX, pixelY, TEXTURE_WIDTH, TEXTURE_HEIGHT};

            if (is_tile_in_purple(x, y))
            {
                SDL_RenderTexture(renderer, constraintTexture1, NULL, &cRect);
                if (y > highest_purple_x)
                {
                    highest_purple_x = y;
                    highest_purple_y = x;
                }
                else if (y == highest_purple_x && x > highest_purple_y)
                {
                    highest_purple_y = x;
                }
            }
            if (is_tile_in_orange(x, y))
            {
                SDL_RenderTexture(renderer, constraintTexture2, NULL, &cRect);
                if (y > highest_orange_x)
                {
                    highest_orange_x = y;
                    highest_orange_y = x;
                }
                else if (y == highest_orange_x && x > highest_orange_y)
                {
                    highest_orange_y = x;
                }
            }
            if (is_tile_in_green(x, y))
            {
                SDL_RenderTexture(renderer, constraintTexture3, NULL, &cRect);
                if (y > highest_green_x)
                {
                    highest_green_x = y;
                    highest_green_y = x;
                }
                else if (y == highest_green_x && x > highest_green_y)
                {
                    highest_green_y = x;
                }
            }
            if (is_tile_in_pink(x, y))
            {
                SDL_RenderTexture(renderer, constraintTexture4, NULL, &cRect);
                if (y > highest_pink_x)
                {
                    highest_pink_x = y;
                    highest_pink_y = x;
                }
                else if (y == highest_pink_x && x > highest_pink_y)
                {
                    highest_pink_y = x;
                }
            }
        }
    }

    // Render purple constraint value at bottom-right of region
    float pixelX = (float)(highest_purple_x * TEXTURE_WIDTH);
    float pixelY = (float)(highest_purple_y * TEXTURE_HEIGHT);
    SDL_FRect symbol1 = {pixelX + 32 + TEXTURE_WIDTH / 4, pixelY + 32 + TEXTURE_HEIGHT / 4, TEXTURE_WIDTH / 3, TEXTURE_HEIGHT / 3};
    SDL_SetTextureColorMod(constraintValueTexture, 0, 0, 254);
    SDL_RenderTexture(renderer, constraintValueTexture, NULL, &symbol1);

    char purpleConstraintText[16];
    sprintf(purpleConstraintText, "%d", purpleConstraint.targetSum);
    showText(renderer, (int)(pixelX + 36 + TEXTURE_WIDTH / 4), (int)(pixelY + 40 + TEXTURE_HEIGHT / 4),
             purpleConstraintText,
             (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE});

    if (sumTilesInPurple() == purpleConstraint.targetSum)
        SDL_RenderTexture(renderer, checkTexture, NULL, &symbol1);

    // Render orange constraint value at bottom-right of region
    pixelX = (float)(highest_orange_x * TEXTURE_WIDTH);
    pixelY = (float)(highest_orange_y * TEXTURE_HEIGHT);
    SDL_FRect symbol2 = {pixelX + 32 + TEXTURE_WIDTH / 4, pixelY + 32 + TEXTURE_HEIGHT / 4, TEXTURE_WIDTH / 3, TEXTURE_HEIGHT / 3};

    SDL_SetTextureColorMod(constraintValueTexture, 0, 104, 55);
    SDL_RenderTexture(renderer, constraintValueTexture, NULL, &symbol2);
    char orangeConstraintText[16];
    sprintf(orangeConstraintText, "=%d", orangeConstraint.targetSum);
    showText(renderer, (int)(pixelX + 40 + TEXTURE_WIDTH / 4), (int)(pixelY + 40 + TEXTURE_HEIGHT / 4),
             orangeConstraintText,
             (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE});

    if (sumTilesInOrange() == orangeConstraint.targetSum)
        SDL_RenderTexture(renderer, checkTexture, NULL, &symbol2);

    // // Render green constraint value at bottom-right of region
    pixelX = (float)(highest_green_x * TEXTURE_WIDTH);
    pixelY = (float)(highest_green_y * TEXTURE_HEIGHT);
    SDL_FRect symbol3 = {pixelX + 32 + TEXTURE_WIDTH / 4,
                         pixelY + 32 + TEXTURE_HEIGHT / 4, TEXTURE_WIDTH / 3, TEXTURE_HEIGHT / 3};
    SDL_SetTextureColorMod(constraintValueTexture, 116, 75, 36);
    char greenConstraintText[16];
    sprintf(greenConstraintText, "="); // Dummy value for green
    SDL_RenderTexture(renderer, constraintValueTexture, NULL, &symbol3);
    showText(renderer, (int)(pixelX + 40 + TEXTURE_WIDTH / 4), (int)(pixelY + 40 + TEXTURE_HEIGHT / 4),
             greenConstraintText,
             (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE});

    if (greenConstraintSatisfied())
        SDL_RenderTexture(renderer, checkTexture, NULL, &symbol3);

    // // Render pink constraint value at bottom-right of region
    pixelX = (float)(highest_pink_x * TEXTURE_WIDTH);
    pixelY = (float)(highest_pink_y * TEXTURE_HEIGHT);
    SDL_FRect symbol4 = {pixelX + 32 + TEXTURE_WIDTH / 4,
                         pixelY + 32 + TEXTURE_HEIGHT / 4, TEXTURE_WIDTH / 3, TEXTURE_HEIGHT / 3};
    SDL_SetTextureColorMod(constraintValueTexture, 236, 28, 36);
    SDL_RenderTexture(renderer, constraintValueTexture, NULL, &symbol4);
    char pinkConstraintText[16];
    sprintf(pinkConstraintText, ">%d", pinkConstraint.threshold);
    showText(renderer, (int)(pixelX + 40 + TEXTURE_WIDTH / 4), (int)(pixelY + 40 + TEXTURE_HEIGHT / 4),
             pinkConstraintText,
             (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE});

    if (pinkConstraintSatisfied())
        SDL_RenderTexture(renderer, checkTexture, NULL, &symbol4);

    int orangeSum = sumTilesInOrange();
    int purpleSum = sumTilesInPurple();
    bool greenOK = greenConstraintSatisfied();

    char hudLine1[128];
    char hudLine2[256];
    char hudLine3[256];
    char hudLine4[256];

    SDL_snprintf(hudLine1, sizeof(hudLine1), "Level %d", currentLevel);
    SDL_snprintf(
        hudLine2, sizeof(hudLine2),
        "Green: %d / %d    Blue: %d / %d",
        orangeSum, orangeConstraint.targetSum,
        purpleSum, purpleConstraint.targetSum);

    showText(renderer, 10, 0, hudLine1, (SDL_Color){0, 0, 0, SDL_ALPHA_OPAQUE});
    showText(renderer, 10, 20, hudLine2, (SDL_Color){0, 0, 0, SDL_ALPHA_OPAQUE});

    if (currentLevel == 3 || currentLevel == 5)
    {
        SDL_snprintf(hudLine3, sizeof(hudLine3), "Beige (=): %s", greenOK ? "OK" : "Not OK");
        showText(renderer, 10, 40, hudLine3, (SDL_Color){0, 0, 0, SDL_ALPHA_OPAQUE});
    }

    if (currentLevel == 4 || currentLevel == 5)
    {
        int pinkSum = sumTilesInPink();
        const char *sign = pinkConstraint.greater ? ">" : "<";
        bool pinkOK = pinkConstraintSatisfied();
        SDL_snprintf(hudLine4, sizeof(hudLine4),
                     "Pink (%s %d): %d   (%s)",
                     sign, pinkConstraint.threshold, pinkSum, pinkOK ? "OK" : "Not OK");
        showText(renderer, 10, (currentLevel == 5 ? 60 : 40), hudLine4, (SDL_Color){255, 200, 230, SDL_ALPHA_OPAQUE});
    }

    SDL_RenderDebugTextFormat(
        renderer,
        10,
        APP_HEIGHT - charsize,
        "Running for %" PRIu64 " seconds",
        (uint64_t)(SDL_GetTicks() / 1000));

    SDL_RenderPresent(renderer);
}

// =====================
//  MENU & POPUP RENDERING
// =====================
void renderMainMenu(void)
{
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_Color titleColor = (SDL_Color){0, 0, 0, SDL_ALPHA_OPAQUE};

    // Bigger-looking title (simple multi-draw trick)
    int titleX = APP_WIDTH / 2 - 170;
    int titleY = 80;
    showText(renderer, titleX, titleY, "MindBlock", titleColor);
    showText(renderer, titleX + 1, titleY, "MindBlock", titleColor);
    showText(renderer, titleX, titleY + 1, "MindBlock", titleColor);
    showText(renderer, titleX + 1, titleY + 1, "MindBlock", titleColor);

    const float bw = 360.0f;
    const float bh = 110.0f;
    const float gap = 28.0f;

    float bx = (APP_WIDTH - bw) / 2.0f;
    float by = 210.0f;

    SDL_FRect rectStart = {bx, by + 0 * (bh + gap), bw, bh};
    SDL_FRect rectSettings = {bx, by + 1 * (bh + gap), bw, bh};
    SDL_FRect rectExit = {bx, by + 2 * (bh + gap), bw, bh};

    if (startButtonTexture)
        SDL_RenderTexture(renderer, startButtonTexture, NULL, &rectStart);
    if (settingsButtonTexture)
        SDL_RenderTexture(renderer, settingsButtonTexture, NULL, &rectSettings);
    if (exitButtonTexture)
        SDL_RenderTexture(renderer, exitButtonTexture, NULL, &rectExit);

    SDL_FRect selectedRect = rectStart;
    if (mainMenuSelection == 1)
        selectedRect = rectSettings;
    if (mainMenuSelection == 2)
        selectedRect = rectExit;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 230, 120, 110);
    SDL_RenderFillRect(renderer, &selectedRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderRect(renderer, &selectedRect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    showText(renderer, 20, APP_HEIGHT - 40,
             "Use W/S or Up/Down to select, Enter to confirm",
             (SDL_Color){0, 0, 0, SDL_ALPHA_OPAQUE});

    SDL_RenderPresent(renderer);
}

// ✅ Settings now white bg + black text + 3 options
void renderSettings(void)
{
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_Color black = (SDL_Color){0, 0, 0, SDL_ALPHA_OPAQUE};

    showText(renderer, APP_WIDTH / 2 - 120, 80, "Settings", black);

    // Option 0: Instructions
    showText(renderer, APP_WIDTH / 2 - 220, 170,
             (settingsSelection == 0) ? "> Instructions" : "  Instructions",
             black);

    // Option 1: Brightness
    int percent = (int)(brightness * 100.0f + 0.5f);
    char lineBrightness[128];
    SDL_snprintf(lineBrightness, sizeof(lineBrightness), "  Brightness: %d%%", percent);
    if (settingsSelection == 1)
        lineBrightness[0] = '>';

    showText(renderer, APP_WIDTH / 2 - 220, 210, lineBrightness, black);

    // Option 2: Return
    showText(renderer, APP_WIDTH / 2 - 220, 250,
             (settingsSelection == 2) ? "> Return to Main Menu" : "  Return to Main Menu",
             black);

    showText(renderer, 20, APP_HEIGHT - 60,
             "W/S to move, A/D to change brightness",
             black);
    showText(renderer, 20, APP_HEIGHT - 40,
             "Enter to select, Esc to go back",
             black);

    SDL_RenderPresent(renderer);
}

// ✅ NEW Instructions page (white bg, black text, Back button)
void renderInstructions(void)
{
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_Color black = (SDL_Color){0, 0, 0, SDL_ALPHA_OPAQUE};

    showText(renderer, 60, 60, "Instructions", black);

    showText(renderer, 60, 120, "W A S D - Movement", black);
    showText(renderer, 60, 140, "E - Pick up Piece", black);
    showText(renderer, 60, 160, "R - Rotate Piece", black);
    showText(renderer, 60, 180, "Q - Place Piece", black);

    // Paragraph (split into lines for your existing text renderer)
    showText(renderer, 60, 240,
             "In MindBlock your objective is to put all the pieces in the play area (White)",
             black);
    showText(renderer, 60, 260,
             "into the puzzle area (Grey) while at the same time using the values in your pieces",
             black);
    showText(renderer, 60, 280,
             "to solve mathematical constrains that appear in the puzzle area.",
             black);
    showText(renderer, 60, 320,
             "It's important to remember that more than one piece can be placed inside the constraints.",
             black);
    showText(renderer, 60, 340,
             "It's about logic but also creativity.",
             black);
    showText(renderer, 60, 380,
             "The level will only be complete if all the pieces are completely placed inside the puzzle area",
             black);
    showText(renderer, 60, 400,
             "and the math constraints are satisfied.",
             black);

    // "Back button" (visual + key hint)
    SDL_FRect backRect = {60.0f, 470.0f, 180.0f, 60.0f};
    SDL_SetRenderDrawColor(renderer, 255, 230, 120, 160);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(renderer, &backRect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderRect(renderer, &backRect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    showText(renderer, 110, 490, "Back", black);
    showText(renderer, 60, APP_HEIGHT - 40, "Enter/Esc to go back", black);

    SDL_RenderPresent(renderer);
}

// ✅ Confirm Exit now white bg + black text
void renderConfirmExit(void)
{
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_Color black = (SDL_Color){0, 0, 0, SDL_ALPHA_OPAQUE};

    showText(renderer, APP_WIDTH / 2 - 260, 160,
             "Are you sure you want to close the game?",
             black);

    // Use an arrow indicator instead of color
    showText(renderer, APP_WIDTH / 2 - 80, 220,
             (confirmSelection == 0) ? "> Yes" : "  Yes",
             black);
    showText(renderer, APP_WIDTH / 2 + 20, 220,
             (confirmSelection == 1) ? "> No" : "  No",
             black);

    showText(renderer, 20, APP_HEIGHT - 40,
             "Use arrows or WASD to choose, Enter to confirm, Esc to cancel",
             black);

    SDL_RenderPresent(renderer);
}

// ✅ Level Complete now white bg + black text
void renderLevelComplete(void)
{
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_Color black = (SDL_Color){0, 0, 0, SDL_ALPHA_OPAQUE};

    showText(renderer, APP_WIDTH / 2 - 220, APP_HEIGHT / 2 - 120, "Level Complete, Well done", black);

    char levelLine[64];
    SDL_snprintf(levelLine, sizeof(levelLine), "Level %d Complete", currentLevel);
    showText(renderer, APP_WIDTH / 2 - 140, APP_HEIGHT / 2 - 80, levelLine, black);

    showText(renderer, APP_WIDTH / 2 - 140, APP_HEIGHT / 2,
             (levelCompleteSelection == 0) ? "> Next Level" : "  Next Level",
             black);
    showText(renderer, APP_WIDTH / 2 - 140, APP_HEIGHT / 2 + 40,
             (levelCompleteSelection == 1) ? "> Back to Main Menu" : "  Back to Main Menu",
             black);

    showText(renderer, 20, APP_HEIGHT - 40,
             "Use W/S or Up/Down to select, Enter to confirm, Esc for Main Menu",
             black);

    SDL_RenderPresent(renderer);
}

// =====================
//       HELPERS
// =====================
void get_puzzle_origin(int *px0, int *py0)
{
    const int h = 4, w = 4;
    *px0 = (MAP_ROWS - h) / 2;
    *py0 = (MAP_COLS - w) / 2;
}

void reset_player(void)
{
    player.position_x = 5;
    player.position_y = 5;
    player.controllingPiece = false;
    player.controlledPieceId = '\0';
    player.direction = 'S';
}

void startGame(void)
{
    currentLevel = 1;
    init_world_layer();
    puzzle_area();
    init_constraints(currentLevel);
    initPieces(currentLevel);
    reset_player();
    currentState = STATE_PLAYING;
}

// =====================
//    GAME LOGIC
// =====================
void movePlayer(char dir)
{
    int newX = player.position_x;
    int newY = player.position_y;

    if (dir == 'W')
        newX--;
    else if (dir == 'S')
        newX++;
    else if (dir == 'A')
        newY--;
    else if (dir == 'D')
        newY++;
    else
        return;

    player.direction = dir;
    if (!inBounds(newX, newY))
        return;
    if (map[LAYER_WORLD][newX][newY] == TILE_WALL)
        return;

    player.position_x = newX;
    player.position_y = newY;
}

void interact(void)
{
    for (int l = MAP_LAYERS - 1; l >= LAYER_PIECE_BASE; --l)
    {
        char tile = map[l][player.position_x][player.position_y];
        if (tile >= 'A' && tile <= 'Z')
        {
            player.controllingPiece = true;
            player.controlledPieceId = tile;
            printf("You are now controlling piece %c!\n", tile);
            return;
        }
    }
}

bool canMovePiece(int index, int dx, int dy)
{
    struct Piece p = pieces[index];
    for (int i = 0; i < p.size; i++)
    {
        int nx = p.baseX + p.tiles[i][0] + dx;
        int ny = p.baseY + p.tiles[i][1] + dy;
        if (!inBounds(nx, ny))
            return false;
        if (map[LAYER_WORLD][nx][ny] == TILE_WALL)
            return false;
    }
    return true;
}

bool is_tile_in_orange(int x, int y)
{
    int px0, py0;
    get_puzzle_origin(&px0, &py0);

    if (currentLevel == 1)
    {
        return ((x == px0 + 1 && (y == py0 + 1 || y == py0 + 2)) ||
                (x == px0 + 2 && (y == py0 + 1 || y == py0 + 2)));
    }
    else if (currentLevel == 2)
    {
        return (x == px0 + 3 && y >= py0 && y <= py0 + 3);
    }
    else if (currentLevel == 3)
    {
        return (x == px0 && y >= py0 && y <= py0 + 3);
    }
    else if (currentLevel == 4)
    {
        return (x == px0 + 0 && (y == py0 + 0 || y == py0 + 1));
    }
    else
    {
        return (x == px0 + 0 && (y == py0 + 0 || y == py0 + 1));
    }
}

bool is_tile_in_purple(int x, int y)
{
    int px0, py0;
    get_puzzle_origin(&px0, &py0);

    if (currentLevel == 1)
    {
        if (x == px0 && (y == py0 || y == py0 + 1 || y == py0 + 2))
            return true;
        if (x == px0 + 1 && y == py0)
            return true;
        return false;
    }
    else if (currentLevel == 2)
    {
        if (x == px0 && y == py0 + 2)
            return true;
        if (x == px0 + 1 && y == py0 + 2)
            return true;
        if (x == px0 + 2 && (y == py0 + 2 || y == py0 + 3))
            return true;
        return false;
    }
    else if (currentLevel == 3)
    {
        return (x == px0 + 3 && y >= py0 && y <= py0 + 3);
    }
    else if (currentLevel == 4)
    {
        return (x == px0 + 3 && (y == py0 + 2 || y == py0 + 3));
    }
    else
    {
        if (x == px0 + 1 && (y == py0 + 0 || y == py0 + 1))
            return true;
        if (x == px0 + 2 && y == py0 + 0)
            return true;
        return false;
    }
}

bool is_tile_in_green(int x, int y)
{
    if (currentLevel != 3 && currentLevel != 5)
        return false;

    int px0, py0;
    get_puzzle_origin(&px0, &py0);

    if (currentLevel == 3)
    {
        return (x >= px0 + 1 && x <= px0 + 2 &&
                y >= py0 + 1 && y <= py0 + 2);
    }
    else
    {
        return (x == px0 + 0 && (y == py0 + 2 || y == py0 + 3));
    }
}

bool is_tile_in_pink(int x, int y)
{
    if (currentLevel != 4 && currentLevel != 5)
        return false;

    int px0, py0;
    get_puzzle_origin(&px0, &py0);

    if (currentLevel == 4)
    {
        return ((y == py0 + 0) && (x == px0 + 1 || x == px0 + 2 || x == px0 + 3));
    }
    else
    {
        if (x == px0 + 2 && y == py0 + 3)
            return true;
        if (x == px0 + 3 && (y == py0 + 2 || y == py0 + 3))
            return true;
        return false;
    }
}

int findPieceIndexById(char id)
{
    for (int i = 0; i < numPieces; i++)
        if (pieces[i].id == id)
            return i;
    return -1;
}

void findPieceAndTileAt(int x, int y, int *pieceIndex, int *tileIndex)
{
    *pieceIndex = -1;
    *tileIndex = -1;
    for (int i = 0; i < numPieces; ++i)
    {
        struct Piece *p = &pieces[i];
        for (int t = 0; t < p->size; ++t)
        {
            int px = p->baseX + p->tiles[t][0];
            int py = p->baseY + p->tiles[t][1];
            if (px == x && py == y)
            {
                *pieceIndex = i;
                *tileIndex = t;
                return;
            }
        }
    }
}

bool is_tile_in_puzzle_area(int x, int y)
{
    int x0, y0;
    get_puzzle_origin(&x0, &y0);
    const int h = 4, w = 4;
    return (x >= x0 && x < x0 + h && y >= y0 && y < y0 + w);
}

bool allPiecesFitInPuzzleArea(void)
{
    for (int i = 0; i < numPieces; i++)
    {
        struct Piece p = pieces[i];
        for (int t = 0; t < p.size; t++)
        {
            int x = p.baseX + p.tiles[t][0];
            int y = p.baseY + p.tiles[t][1];

            if (!is_tile_in_puzzle_area(x, y))
                return false;

            char top = get_top_tile(x, y);
            if (top != p.id)
                return false;
        }
    }
    return true;
}

void init_world_layer(void)
{
    for (int x = 0; x < MAP_ROWS; x++)
        for (int y = 0; y < MAP_COLS; y++)
            map[LAYER_WORLD][x][y] = TILE_FLOOR;

    for (int y = 0; y < MAP_COLS; y++)
    {
        map[LAYER_WORLD][0][y] = TILE_WALL;
        map[LAYER_WORLD][MAP_ROWS - 1][y] = TILE_WALL;
    }
    for (int x = 0; x < MAP_ROWS; x++)
    {
        map[LAYER_WORLD][x][0] = TILE_WALL;
        map[LAYER_WORLD][x][MAP_COLS - 1] = TILE_WALL;
    }

    for (int l = LAYER_PIECE_BASE; l < MAP_LAYERS; l++)
        for (int x = 0; x < MAP_ROWS; x++)
            for (int y = 0; y < MAP_COLS; y++)
                map[l][x][y] = TILE_EMPTY;
}

void movePiece(int index, int dx, int dy)
{
    pieces[index].baseX += dx;
    pieces[index].baseY += dy;
}

bool canPlace(int index)
{
    struct Piece p = pieces[index];
    for (int i = 0; i < p.size; i++)
    {
        int x = p.baseX + p.tiles[i][0];
        int y = p.baseY + p.tiles[i][1];
        if (!inBounds(x, y))
            return false;
        if (map[LAYER_WORLD][x][y] == TILE_WALL)
            return false;
    }
    return true;
}

void puzzle_area(void)
{
    const int h = 4, w = 4;
    int x0 = (MAP_ROWS - h) / 2;
    int y0 = (MAP_COLS - w) / 2;

    for (int x = x0; x < x0 + h; x++)
        for (int y = y0; y < y0 + w; y++)
            map[LAYER_WORLD][x][y] = TILE_PUZZLE;
}

void init_constraints(int level)
{
    if (level == 1)
    {
        orangeConstraint.targetSum = 4;
        purpleConstraint.targetSum = 9;
        pinkConstraint.threshold = 0;
        pinkConstraint.greater = true;
    }
    else if (level == 2)
    {
        orangeConstraint.targetSum = 14;
        purpleConstraint.targetSum = 6;
        pinkConstraint.threshold = 0;
        pinkConstraint.greater = true;
    }
    else if (level == 3)
    {
        orangeConstraint.targetSum = 11;
        purpleConstraint.targetSum = 14;
        pinkConstraint.threshold = 0;
        pinkConstraint.greater = true;
    }
    else if (level == 4)
    {
        orangeConstraint.targetSum = 6;
        purpleConstraint.targetSum = 7;
        pinkConstraint.threshold = 8;
        pinkConstraint.greater = true;
    }
    else
    {
        orangeConstraint.targetSum = 7;
        purpleConstraint.targetSum = 9;
        pinkConstraint.threshold = 10;
        pinkConstraint.greater = true;
    }
}

void removePieceFromMap(int index)
{
    int layer = PIECE_LAYER(index);
    struct Piece p = pieces[index];
    for (int i = 0; i < p.size; i++)
    {
        int x = p.baseX + p.tiles[i][0];
        int y = p.baseY + p.tiles[i][1];
        if (inBounds(x, y) && map[layer][x][y] == p.id)
        {
            set_tile(layer, x, y, TILE_EMPTY);
        }
    }
}

void rotatePiece(int index)
{
    for (int i = 0; i < pieces[index].size; i++)
    {
        int x = pieces[index].tiles[i][0];
        int y = pieces[index].tiles[i][1];
        pieces[index].tiles[i][0] = y;
        pieces[index].tiles[i][1] = -x;
    }
}

void initPieces(int level)
{
    numPieces = 0;

    if (level == 1)
    {
        struct Piece square = {
            'A', 4, {{0, 0}, {0, 1}, {1, 0}, {1, 1}}, {1, 1, 1, 1}, 3, 3, true};

        struct Piece line = {
            'B', 4, {{0, 0}, {0, 1}, {0, 2}, {0, 3}}, {2, 3, 4, 5}, 6, 3, true};

        struct Piece lshape1 = {
            'C', 4, {{0, 1}, {1, 1}, {2, 1}, {2, 0}}, {2, 2, 2, 2}, 2, 15, true};

        struct Piece lshape2 = {
            'D', 4, {{0, 0}, {1, 0}, {2, 0}, {2, 1}}, {2, 2, 3, 2}, 8, 15, true};

        pieces[numPieces++] = square;
        pieces[numPieces++] = line;
        pieces[numPieces++] = lshape1;
        pieces[numPieces++] = lshape2;
    }
    else if (level == 2)
    {
        struct Piece square = {
            'A', 4, {{0, 0}, {0, 1}, {1, 0}, {1, 1}}, {1, 1, 1, 1}, 3, 3, true};

        struct Piece line = {
            'B', 4, {{0, 0}, {0, 1}, {0, 2}, {0, 3}}, {1, 4, 4, 5}, 6, 3, true};

        struct Piece lshape1 = {
            'C', 4, {{0, 1}, {1, 1}, {2, 1}, {2, 0}}, {3, 1, 2, 2}, 2, 15, true};

        struct Piece lshape2 = {
            'D', 4, {{0, 0}, {1, 0}, {2, 0}, {2, 1}}, {1, 2, 3, 4}, 8, 15, true};

        pieces[numPieces++] = square;
        pieces[numPieces++] = line;
        pieces[numPieces++] = lshape1;
        pieces[numPieces++] = lshape2;
    }
    else if (level == 3)
    {
        struct Piece pieceA = {
            'A', 4, {{0, 1}, {1, 0}, {1, 1}, {1, 2}}, {2, 1, 3, 4}, 3, 3, true};

        struct Piece pieceB = {
            'B', 4, {{0, 1}, {1, 0}, {1, 1}, {1, 2}}, {2, 5, 1, 1}, 6, 3, true};

        struct Piece pieceC = {
            'C', 4, {{0, 1}, {1, 0}, {1, 1}, {1, 2}}, {2, 1, 2, 3}, 2, 15, true};

        struct Piece pieceD = {
            'D', 4, {{0, 1}, {1, 0}, {1, 1}, {1, 2}}, {2, 6, 1, 1}, 8, 15, true};

        pieces[numPieces++] = pieceA;
        pieces[numPieces++] = pieceB;
        pieces[numPieces++] = pieceC;
        pieces[numPieces++] = pieceD;
    }
    else if (level == 4)
    {
        struct Piece pieceA = {
            'A', 4, {{0, 0}, {0, 1}, {0, 2}, {0, 3}}, {2, 4, 1, 3}, 3, 3, true};

        struct Piece pieceB = {
            'B', 4, {{0, 0}, {0, 1}, {1, 0}, {2, 0}}, {3, 1, 4, 2}, 6, 3, true};

        struct Piece pieceC = {
            'C', 4, {{0, 0}, {0, 1}, {1, -1}, {1, 0}}, {1, 6, 2, 5}, 2, 15, true};

        struct Piece pieceD = {
            'D', 4, {{0, 2}, {1, 0}, {1, 1}, {1, 2}}, {5, 2, 3, 4}, 8, 15, true};

        pieces[numPieces++] = pieceA;
        pieces[numPieces++] = pieceB;
        pieces[numPieces++] = pieceC;
        pieces[numPieces++] = pieceD;
    }
    else
    {
        struct Piece pieceA = {
            'A', 4, {{0, 0}, {0, 1}, {0, 2}, {1, 0}}, {1, 4, 3, 1}, 3, 3, true};

        struct Piece pieceB = {
            'B', 4, {{0, 0}, {0, 1}, {1, 1}, {1, 2}}, {6, 1, 2, 3}, 6, 3, true};

        struct Piece pieceC = {
            'C', 4, {{0, 0}, {1, 0}, {2, 0}, {2, 1}}, {6, 5, 2, 2}, 2, 15, true};

        struct Piece pieceD = {
            'D', 4, {{0, 1}, {1, 1}, {1, 0}, {2, 0}}, {2, 4, 6, 1}, 8, 15, true};

        pieces[numPieces++] = pieceA;
        pieces[numPieces++] = pieceB;
        pieces[numPieces++] = pieceC;
        pieces[numPieces++] = pieceD;
    }

    for (int l = LAYER_PIECE_BASE; l < MAP_LAYERS; ++l)
        for (int x = 0; x < MAP_ROWS; ++x)
            for (int y = 0; y < MAP_COLS; ++y)
                map[l][x][y] = TILE_EMPTY;

    for (int i = 0; i < numPieces; i++)
        placePieceOnMap(i);
}

void placePieceOnMap(int index)
{
    int layer = PIECE_LAYER(index);
    struct Piece p = pieces[index];
    for (int i = 0; i < p.size; i++)
    {
        int x = p.baseX + p.tiles[i][0];
        int y = p.baseY + p.tiles[i][1];
        if (inBounds(x, y))
            set_tile(layer, x, y, p.id);
    }
}

// =====================
//        AUDIO (SDL3 FIXED)
// =====================
static bool init_sound_effects(void)
{
    SDL_zero(deviceSpec);
    deviceSpec.freq = 48000;
    deviceSpec.format = SDL_AUDIO_F32;
    deviceSpec.channels = 2;

    audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &deviceSpec);
    if (audioDevice == 0)
    {
        SDL_Log("SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    SDL_ResumeAudioDevice(audioDevice);

    SDL_zero(sfxSteps);
    SDL_zero(sfxPickup);
    SDL_zero(sfxRotate);
    SDL_zero(sfxLevelComplete);

    bool ok = true;
    ok &= load_sound_effect(&sfxSteps, "audio/Steps.wav");
    ok &= load_sound_effect(&sfxPickup, "audio/Pickup.wav");
    ok &= load_sound_effect(&sfxRotate, "audio/Rotate.wav");
    ok &= load_sound_effect(&sfxLevelComplete, "audio/LevelComplete.wav");

    return ok;
}

static bool load_sound_effect(SoundEffect *sfx, const char *path)
{
    SDL_AudioSpec wavSpec;
    Uint8 *wavData = NULL;
    Uint32 wavLen = 0;

    if (!SDL_LoadWAV(path, &wavSpec, &wavData, &wavLen))
    {
        SDL_Log("Failed to load WAV '%s': %s", path, SDL_GetError());
        sfx->loaded = false;
        return false;
    }

    SDL_AudioStream *stream = SDL_CreateAudioStream(&wavSpec, &deviceSpec);
    if (!stream)
    {
        SDL_Log("Failed to create audio stream for '%s': %s", path, SDL_GetError());
        SDL_free(wavData);
        sfx->loaded = false;
        return false;
    }

    if (!SDL_BindAudioStream(audioDevice, stream))
    {
        SDL_Log("Failed to bind audio stream for '%s': %s", path, SDL_GetError());
        SDL_DestroyAudioStream(stream);
        SDL_free(wavData);
        sfx->loaded = false;
        return false;
    }

    sfx->wavSpec = wavSpec;
    sfx->wavData = wavData;
    sfx->wavLen = wavLen;
    sfx->stream = stream;
    sfx->loaded = true;

    return true;
}

static void play_sound_effect(SoundEffect *sfx)
{
    if (!sfx || !sfx->loaded || !sfx->stream)
        return;

    SDL_ClearAudioStream(sfx->stream);

    SDL_PutAudioStreamData(
        sfx->stream,
        sfx->wavData,
        (int)sfx->wavLen);

    SDL_FlushAudioStream(sfx->stream);
}

static void destroy_sound_effect(SoundEffect *sfx)
{
    if (!sfx)
        return;

    if (sfx->stream)
    {
        SDL_UnbindAudioStream(sfx->stream);
        SDL_DestroyAudioStream(sfx->stream);
        sfx->stream = NULL;
    }
    if (sfx->wavData)
    {
        SDL_free(sfx->wavData);
        sfx->wavData = NULL;
    }
    sfx->wavLen = 0;
    sfx->loaded = false;
}

static void shutdown_sound_effects(void)
{
    destroy_sound_effect(&sfxSteps);
    destroy_sound_effect(&sfxPickup);
    destroy_sound_effect(&sfxRotate);
    destroy_sound_effect(&sfxLevelComplete);

    if (audioDevice != 0)
    {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
}
