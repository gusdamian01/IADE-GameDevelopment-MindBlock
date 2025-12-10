#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "sdl_utils.h"
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// =====================
//      TEXTURES
// =====================
static SDL_Texture *playerTexture        = NULL;
static SDL_Texture *playerTexture2       = NULL;
static SDL_Texture *playerTexture3       = NULL;
static SDL_Texture *playerTexture4       = NULL;
static SDL_Texture *floorTexture         = NULL;
static SDL_Texture *puzzleTexture        = NULL;
static SDL_Texture *constraintTexture1   = NULL; // purple region
static SDL_Texture *constraintTexture2   = NULL; // orange region
static SDL_Texture *constraintTexture3   = NULL; // green "=" region
// One texture per tile value 1..7
static SDL_Texture *valueTextures[7]     = { NULL };

// CONFIGURATIONS
#define MAP_ROWS   12
#define MAP_COLS   20
#define MAX_PIECES 10
#define MAP_LAYERS (1 + MAX_PIECES)

#define LAYER_WORLD       0
#define LAYER_PIECE_BASE  1
#define PIECE_LAYER(i)    (LAYER_PIECE_BASE + (i))

// Tile codes
#define TILE_EMPTY  '\0'
#define TILE_WALL   'W'
#define TILE_FLOOR  'F'
#define TILE_PUZZLE 'P' // the general puzzle area background (4x4)

#define APP_NAME "MindBlock"

// TILE SIZE: now 64x64
#define TEXTURE_WIDTH  64
#define TEXTURE_HEIGHT 64

// Window size derived from map dimensions
#define APP_WIDTH  (MAP_COLS * TEXTURE_WIDTH)   // 20 * 64 = 1280
#define APP_HEIGHT (MAP_ROWS * TEXTURE_HEIGHT)  // 12 * 64 = 768

#define MAX_LEVELS 3

// GAME STATES
typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_SETTINGS,
    STATE_CONFIRM_EXIT,
    STATE_LEVEL_COMPLETE
} GameState;

// STRUCTURES
struct Player {
    int position_x;
    int position_y;
    bool controllingPiece;
    char controlledPieceId;
    char direction;
};

struct Piece {
    char id;
    int size;
    int tiles[4][2];  // relative positions to base
    int values[4];    // per-tile values
    int baseX;
    int baseY;
    bool placed;
};

typedef struct {
    int targetSum;
} ConstraintRegion;

// GLOBALS
char map[MAP_LAYERS][MAP_ROWS][MAP_COLS];
struct Player player = {5, 5, false, '\0', 'S'};
struct Piece pieces[MAX_PIECES];
int numPieces = 0;

ConstraintRegion orangeConstraint;
ConstraintRegion purpleConstraint;

static const char *VALUE_EMOJI[7] = {
    "1️⃣ ", "2️⃣ ", "3️⃣ ", "4️⃣ ", "5️⃣ ", "6️⃣ ", "7️⃣ "
};

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;

// Current level: 1, 2, or 3
int currentLevel = 1;

// Game state & menu selections
GameState currentState   = STATE_MENU;
int       mainMenuSelection   = 0; // 0: Play, 1: Settings, 2: Exit
int       settingsSelection   = 0; // 0: Brightness, 1: Return
int       confirmSelection    = 0; // 0: Yes, 1: No
int       levelCompleteSelection = 0; // 0: Next Level, 1: Back to Main Menu

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
int  findPieceIndexById(char id);
void get_puzzle_origin(int *px0, int *py0);

bool is_tile_in_puzzle_area(int x, int y);
bool is_tile_in_orange(int x, int y);
bool is_tile_in_purple(int x, int y);
bool is_tile_in_green(int x, int y);   // green "=" region
int  sumTilesInOrange(void);
int  sumTilesInPurple(void);
bool greenConstraintSatisfied(void);
bool constraintsSatisfied(void);
bool allPiecesFitInPuzzleArea(void);
void findPieceAndTileAt(int x, int y, int *pieceIndex, int *tileIndex);
void reset_player(void);

// Menus & brightness
void load_value_textures(void);
void apply_brightness_to_textures(void);
void renderMainMenu(void);
void renderSettings(void);
void renderConfirmExit(void);
void renderLevelComplete(void);
void startGame(void);

// Helpers
static inline bool inBounds(int x, int y) {
    return (x >= 0 && x < MAP_ROWS && y >= 0 && y < MAP_COLS);
}
static inline char get_top_tile(int x, int y) {
    for (int l = MAP_LAYERS - 1; l >= 0; --l) {
        char t = map[l][x][y];
        if (t != TILE_EMPTY) return t;
    }
    return TILE_EMPTY;
}
static inline void set_tile(int layer, int x, int y, char t) {
    if (inBounds(x, y)) map[layer][x][y] = t;
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
            {
                sum += p.values[t];
            }
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
            {
                sum += p.values[t];
            }
        }
    }
    return sum;
}

// Green: all tiles in green region must have same value (level 3 only)
bool greenConstraintSatisfied(void)
{
    if (currentLevel != 3) return true; // not used in levels 1–2

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
                if (!foundFirst) {
                    foundFirst = true;
                    firstVal = v;
                } else {
                    if (v != firstVal) return false;
                }
            }
        }
    }

    return true;
}

bool constraintsSatisfied(void) {
    // Orange and purple are always enforced on all levels
    if (sumTilesInOrange() != orangeConstraint.targetSum) return false;
    if (sumTilesInPurple() != purpleConstraint.targetSum) return false;

    // On level 3, the green "=" constraint is also enforced
    if (currentLevel == 3 && !greenConstraintSatisfied()) return false;

    return true;
}

// =====================
//        MAIN
// =====================
int main(void)
{
    // Initialize SDL Systems.
    // NOTE: width first, then height
    window   = sdl_initialize_window(APP_NAME, APP_WIDTH, APP_HEIGHT);
    renderer = sdl_initialize_renderer(window);
    sdl_initialize_audio();

    srand((unsigned)time(NULL));

    init_world_layer();
    puzzle_area();
    init_constraints(currentLevel);
    initPieces(currentLevel);
    reset_player();

    // Load Sprites
    playerTexture      = sdl_load_texture(renderer, "sprites/Joe.png");
    playerTexture2     = sdl_load_texture(renderer, "sprites/JoeLeft.png");
    playerTexture3     = sdl_load_texture(renderer, "sprites/JoeRight.png");
    playerTexture4     = sdl_load_texture(renderer, "sprites/JoeUp.png");
    floorTexture       = sdl_load_texture(renderer, "sprites/PlayArea.png");
    puzzleTexture      = sdl_load_texture(renderer, "sprites/PuzzleArea.png");
    constraintTexture1 = sdl_load_texture(renderer, "sprites/Constraint1.png");
    constraintTexture2 = sdl_load_texture(renderer, "sprites/Constraint2.png");
    constraintTexture3 = sdl_load_texture(renderer, "sprites/Constraint3.png"); // GREEN "="

    // Load tile-value textures (1..7)
    load_value_textures();

    // Apply initial brightness
    apply_brightness_to_textures();

    // Start in main menu
    currentState = STATE_MENU;
    mainMenuSelection = 0;
    settingsSelection = 0;
    confirmSelection  = 0;
    levelCompleteSelection = 0;

    // Game Loop
    int running = 1;
    const Uint32 FRAME_MS = 16; // ~60 FPS
    while (running)
    {
        Uint32 frame_start = SDL_GetTicks();
        // Capture Events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                running = 0;

            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                SDL_Keycode key = event.key.key;

                // ===== MENU STATE HANDLING =====
                if (currentState == STATE_MENU) {
                    int numItems = 3;
                    if (key == SDLK_W || key == SDLK_UP) {
                        mainMenuSelection = (mainMenuSelection - 1 + numItems) % numItems;
                    } else if (key == SDLK_S || key == SDLK_DOWN) {
                        mainMenuSelection = (mainMenuSelection + 1) % numItems;
                    } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        if (mainMenuSelection == 0) {
                            // Play
                            startGame();
                        } else if (mainMenuSelection == 1) {
                            // Settings
                            currentState = STATE_SETTINGS;
                            settingsSelection = 0;
                        } else if (mainMenuSelection == 2) {
                            // Exit
                            currentState = STATE_CONFIRM_EXIT;
                            confirmSelection = 0;
                        }
                    }
                }
                else if (currentState == STATE_SETTINGS) {
                    int numItems = 2;
                    if (key == SDLK_W || key == SDLK_UP) {
                        settingsSelection = (settingsSelection - 1 + numItems) % numItems;
                    } else if (key == SDLK_S || key == SDLK_DOWN) {
                        settingsSelection = (settingsSelection + 1) % numItems;
                    } else if ((key == SDLK_A || key == SDLK_LEFT) && settingsSelection == 0) {
                        // Decrease brightness
                        brightness -= 0.1f;
                        if (brightness < 0.2f) brightness = 0.2f;
                        apply_brightness_to_textures();
                    } else if ((key == SDLK_D || key == SDLK_RIGHT) && settingsSelection == 0) {
                        // Increase brightness
                        brightness += 0.1f;
                        if (brightness > 1.0f) brightness = 1.0f;
                        apply_brightness_to_textures();
                    } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        if (settingsSelection == 1) {
                            // Return to main menu
                            currentState = STATE_MENU;
                        }
                    } else if (key == SDLK_ESCAPE) {
                        currentState = STATE_MENU;
                    }
                }
                else if (currentState == STATE_CONFIRM_EXIT) {
                    // Toggle selection Yes/No with arrows or WASD
                    if (key == SDLK_W || key == SDLK_S ||
                        key == SDLK_UP || key == SDLK_DOWN ||
                        key == SDLK_A || key == SDLK_D ||
                        key == SDLK_LEFT || key == SDLK_RIGHT) {
                        confirmSelection = 1 - confirmSelection; // toggle 0 <-> 1
                    } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        if (confirmSelection == 0) {
                            // Yes
                            running = 0;
                        } else {
                            // No
                            currentState = STATE_MENU;
                        }
                    } else if (key == SDLK_ESCAPE) {
                        currentState = STATE_MENU;
                    }
                }
                else if (currentState == STATE_LEVEL_COMPLETE) {
                    // Level complete screen: Next Level / Back to Main Menu
                    if (key == SDLK_W || key == SDLK_S ||
                        key == SDLK_UP || key == SDLK_DOWN) {
                        levelCompleteSelection = 1 - levelCompleteSelection; // toggle 0 <-> 1
                    } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        if (levelCompleteSelection == 0) {
                            // Next Level
                            if (currentLevel < MAX_LEVELS) {
                                currentLevel++;
                                init_world_layer();
                                puzzle_area();
                                init_constraints(currentLevel);
                                initPieces(currentLevel);
                                reset_player();
                                currentState = STATE_PLAYING;
                            } else {
                                // At last level, "Next Level" just returns to main menu
                                currentLevel = 1;
                                init_world_layer();
                                puzzle_area();
                                init_constraints(currentLevel);
                                initPieces(currentLevel);
                                reset_player();
                                currentState = STATE_MENU;
                            }
                        } else {
                            // Back to Main Menu
                            currentLevel = 1;
                            init_world_layer();
                            puzzle_area();
                            init_constraints(currentLevel);
                            initPieces(currentLevel);
                            reset_player();
                            currentState = STATE_MENU;
                        }
                    } else if (key == SDLK_ESCAPE) {
                        // Esc behaves like Back to Main Menu
                        currentLevel = 1;
                        init_world_layer();
                        puzzle_area();
                        init_constraints(currentLevel);
                        initPieces(currentLevel);
                        reset_player();
                        currentState = STATE_MENU;
                    }
                }
                else if (currentState == STATE_PLAYING) {
                    // ===== IN-GAME CONTROLS =====

                    if (key == SDLK_W) movePlayer('W');
                    if (key == SDLK_A) movePlayer('A');
                    if (key == SDLK_S) movePlayer('S');
                    if (key == SDLK_D) movePlayer('D');

                    if (player.controllingPiece) {
                        int index = findPieceIndexById(player.controlledPieceId);
                        if (index == -1) continue;

                        if (key == SDLK_Q) {
                            player.controllingPiece = false;
                            player.controlledPieceId = '\0';
                            player.position_x = pieces[index].baseX;
                            player.position_y = pieces[index].baseY;
                            printf("You placed the piece and returned to Joe form.\n");
                        } 
                        else if (key == SDLK_R) {
                            removePieceFromMap(index);
                            rotatePiece(index);
                            if (!canPlace(index)) { for (int i = 0; i < 3; i++) rotatePiece(index); }
                            placePieceOnMap(index);
                        }
                        else {
                            int dx = 0, dy = 0;
                            if (key == SDLK_D) dy = 1;
                            else if (key == SDLK_A) dy = -1;
                            else if (key == SDLK_S) dx = 1;
                            else if (key == SDLK_W) dx = -1;

                            if (dx != 0 || dy != 0) {
                                if (canMovePiece(index, dx, dy)) {
                                    removePieceFromMap(index);
                                    movePiece(index, dx, dy);
                                    placePieceOnMap(index);
                                }
                            }
                        }
                    } 
                    else {
                        if (key == SDLK_E) interact();
                        else movePlayer(key);
                    }
                }
            }
        }

        // ===== STATE-BASED UPDATE & RENDER =====
        if (currentState == STATE_PLAYING) {
            // Victory: all tiles inside 4x4 puzzle area AND all constraints satisfied
            if (allPiecesFitInPuzzleArea() && constraintsSatisfied()) {
                printf("🎉 Level %d Complete!\n", currentLevel);
                currentState = STATE_LEVEL_COMPLETE;
                levelCompleteSelection = 0;
            } else {
                printMap();
            }
        } else if (currentState == STATE_MENU) {
            renderMainMenu();
        } else if (currentState == STATE_SETTINGS) {
            renderSettings();
        } else if (currentState == STATE_CONFIRM_EXIT) {
            renderConfirmExit();
        } else if (currentState == STATE_LEVEL_COMPLETE) {
            renderLevelComplete();
        }

        // Delay to maintain frame rate
        Uint32 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < FRAME_MS)
            SDL_Delay(FRAME_MS - elapsed);
    }

    // Cleanup value textures
    for (int i = 0; i < 7; ++i) {
        if (valueTextures[i]) {
            SDL_DestroyTexture(valueTextures[i]);
            valueTextures[i] = NULL;
        }
    }

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
        "sprites/Tile7.png"  // optional; only values up to 7
    };

    for (int i = 0; i < 7; ++i)
    {
        valueTextures[i] = sdl_load_texture(renderer, paths[i]);
        if (!valueTextures[i]) {
            SDL_Log("Failed to load value texture %d from '%s'", i + 1, paths[i]);
        }
    }
}

void apply_brightness_to_textures(void)
{
    Uint8 mod = (Uint8)(brightness * 255.0f);

    SDL_Texture *texList[] = {
        playerTexture, playerTexture2, playerTexture3, playerTexture4,
        floorTexture, puzzleTexture,
        constraintTexture1, constraintTexture2, constraintTexture3
    };

    int texCount = (int)(sizeof(texList) / sizeof(texList[0]));
    for (int i = 0; i < texCount; ++i) {
        if (texList[i]) {
            SDL_SetTextureColorMod(texList[i], mod, mod, mod);
        }
    }

    for (int i = 0; i < 7; ++i) {
        if (valueTextures[i]) {
            SDL_SetTextureColorMod(valueTextures[i], mod, mod, mod);
        }
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

    // ========= FIRST PASS =========
    // Draw world, puzzle area, pieces (NO player here)
    for (int x = 0; x < MAP_ROWS; x++)
    {
        for (int y = 0; y < MAP_COLS; y++)
        {
            // Convert grid coords (x=row, y=col) to pixel coords
            float pixelX = y * TEXTURE_WIDTH;
            float pixelY = x * TEXTURE_HEIGHT;

            int pIdx, tIdx;
            findPieceAndTileAt(x, y, &pIdx, &tIdx);

            // 1) Piece tiles (per-tile number) are on top of world/puzzle
            if (pIdx != -1 && tIdx != -1)
            {
                int val = pieces[pIdx].values[tIdx];
                if (val < 1) val = 1;
                if (val > 7) val = 7;

                SDL_Texture *tex = valueTextures[val - 1];
                SDL_FRect valRect = {
                    pixelX,
                    pixelY,
                    TEXTURE_WIDTH,
                    TEXTURE_HEIGHT
                };
                if (tex != NULL)
                {
                    SDL_RenderTexture(renderer, tex, NULL, &valRect);
                }
                else
                {
                    SDL_RenderTexture(renderer, floorTexture, NULL, &valRect);
                }
                continue;
            }

            char top = map[LAYER_WORLD][x][y];

            // 2) World / puzzle / floor (no constraints here)
            if (top == TILE_WALL)
            {
                // Optional: draw a wall texture here.
            }
            else if (top == TILE_PUZZLE)
            {
                SDL_FRect floorRect = {
                    pixelX,
                    pixelY,
                    TEXTURE_WIDTH,
                    TEXTURE_HEIGHT
                };
                SDL_RenderTexture(renderer, puzzleTexture, NULL, &floorRect);
            }
            else if (top == TILE_FLOOR || top == TILE_EMPTY)
            {
                SDL_FRect floorRect = {
                    pixelX,
                    pixelY,
                    TEXTURE_WIDTH,
                    TEXTURE_HEIGHT
                };
                SDL_RenderTexture(renderer, floorTexture, NULL, &floorRect);
            }
        }
    }

    // ========= PLAYER PASS =========
    // Draw player above tiles/pieces, but under constraint overlays
    if (!player.controllingPiece)
    {
        float pixelX = player.position_y * TEXTURE_WIDTH;
        float pixelY = player.position_x * TEXTURE_HEIGHT;

        SDL_FRect playerRect = {
            pixelX,
            pixelY,
            TEXTURE_WIDTH,
            TEXTURE_HEIGHT
        };

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

    // ========= SECOND PASS =========
    // Draw constraints as hovering overlays (on top of pieces & player)
    for (int x = 0; x < MAP_ROWS; x++)
    {
        for (int y = 0; y < MAP_COLS; y++)
        {
            float pixelX = y * TEXTURE_WIDTH;
            float pixelY = x * TEXTURE_HEIGHT;

            SDL_FRect cRect = {
                pixelX,
                pixelY,
                TEXTURE_WIDTH,
                TEXTURE_HEIGHT
            };

            // Purple constraint region (uses constraintTexture1)
            if (is_tile_in_purple(x, y))
            {
                SDL_RenderTexture(renderer, constraintTexture1, NULL, &cRect);
            }

            // Orange constraint region (uses constraintTexture2)
            if (is_tile_in_orange(x, y))
            {
                SDL_RenderTexture(renderer, constraintTexture2, NULL, &cRect);
            }

            // Green equality region (uses constraintTexture3)
            if (is_tile_in_green(x, y))
            {
                SDL_RenderTexture(renderer, constraintTexture3, NULL, &cRect);
            }
        }
    }

    // ========= HUD: constraints status =========
    int orangeSum = sumTilesInOrange();
    int purpleSum = sumTilesInPurple();
    bool greenOK = greenConstraintSatisfied();

    char hudLine1[128];
    char hudLine2[256];
    char hudLine3[128];

    SDL_snprintf(
        hudLine1, sizeof(hudLine1),
        "Level %d", currentLevel
    );
    SDL_snprintf(
        hudLine2, sizeof(hudLine2),
        "Green: %d / %d   Blue: %d / %d",
        orangeSum, orangeConstraint.targetSum,
        purpleSum, purpleConstraint.targetSum
    );

    showText(renderer, 10, 0,  hudLine1, (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE});
    showText(renderer, 10, 20, hudLine2, (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE});

    if (currentLevel == 3) {
        SDL_snprintf(
            hudLine3, sizeof(hudLine3),
            "Beige (=): %s",
            greenOK ? "OK" : "Not OK"
        );
        showText(renderer, 10, 40, hudLine3, (SDL_Color){200, 255, 200, SDL_ALPHA_OPAQUE});
    }

    SDL_RenderDebugTextFormat(
        renderer,
        10,
        APP_HEIGHT - charsize,
        "Running for %" SDL_PRIu64 " seconds",
        SDL_GetTicks() / 1000
    );

    SDL_RenderPresent(renderer);
}

// =====================
//  MENU & POPUP RENDERING
// =====================
void renderMainMenu(void)
{
    SDL_SetRenderDrawColor(renderer, 10, 10, 30, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_Color titleColor = (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE};
    SDL_Color normal     = (SDL_Color){200, 200, 200, SDL_ALPHA_OPAQUE};
    SDL_Color selected   = (SDL_Color){255, 255, 0,   SDL_ALPHA_OPAQUE};

    showText(renderer, APP_WIDTH / 2 - 120, 100, "MindBlock", titleColor);

    const char *options[3] = { "Play", "Settings", "Exit" };
    for (int i = 0; i < 3; ++i) {
        SDL_Color c = (i == mainMenuSelection) ? selected : normal;
        showText(renderer, APP_WIDTH / 2 - 80, 200 + i * 40, options[i], c);
    }

    showText(renderer, 20, APP_HEIGHT - 40,
             "Use W/S or Up/Down to select, Enter to confirm",
             (SDL_Color){180, 180, 180, SDL_ALPHA_OPAQUE});

    SDL_RenderPresent(renderer);
}

void renderSettings(void)
{
    SDL_SetRenderDrawColor(renderer, 20, 20, 40, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_Color titleColor = (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE};
    SDL_Color normal     = (SDL_Color){200, 200, 200, SDL_ALPHA_OPAQUE};
    SDL_Color selected   = (SDL_Color){255, 255, 0,   SDL_ALPHA_OPAQUE};

    showText(renderer, APP_WIDTH / 2 - 120, 80, "Settings", titleColor);

    int percent = (int)(brightness * 100.0f + 0.5f);
    char lineBrightness[128];
    SDL_snprintf(lineBrightness, sizeof(lineBrightness),
                 "Brightness: %d%%", percent);

    SDL_Color c0 = (settingsSelection == 0) ? selected : normal;
    SDL_Color c1 = (settingsSelection == 1) ? selected : normal;

    showText(renderer, APP_WIDTH / 2 - 140, 180, lineBrightness, c0);
    showText(renderer, APP_WIDTH / 2 - 140, 220, "Return to Main Menu", c1);

    showText(renderer, 20, APP_HEIGHT - 60,
             "W/S to move, A/D to change brightness",
             (SDL_Color){180, 180, 180, SDL_ALPHA_OPAQUE});
    showText(renderer, 20, APP_HEIGHT - 40,
             "Enter to select, Esc to go back",
             (SDL_Color){180, 180, 180, SDL_ALPHA_OPAQUE});

    SDL_RenderPresent(renderer);
}

void renderConfirmExit(void)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_Color titleColor = (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE};
    SDL_Color normal     = (SDL_Color){200, 200, 200, SDL_ALPHA_OPAQUE};
    SDL_Color selected   = (SDL_Color){255, 255, 0,   SDL_ALPHA_OPAQUE};

    showText(renderer, APP_WIDTH / 2 - 220, 160,
             "Are you sure you want to close the game?",
             titleColor);

    SDL_Color yesColor = (confirmSelection == 0) ? selected : normal;
    SDL_Color noColor  = (confirmSelection == 1) ? selected : normal;

    showText(renderer, APP_WIDTH / 2 - 60, 220, "Yes", yesColor);
    showText(renderer, APP_WIDTH / 2 + 20, 220, "No",  noColor);

    showText(renderer, 20, APP_HEIGHT - 40,
             "Use arrows or WASD to choose, Enter to confirm, Esc to cancel",
             (SDL_Color){180, 180, 180, SDL_ALPHA_OPAQUE});

    SDL_RenderPresent(renderer);
}

void renderLevelComplete(void)
{
    // Full-screen level complete screen (no board)
    SDL_SetRenderDrawColor(renderer, 10, 10, 30, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_Color titleColor   = (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE};
    SDL_Color normalColor  = (SDL_Color){200, 200, 200, SDL_ALPHA_OPAQUE};
    SDL_Color selectColor  = (SDL_Color){255, 255, 0,   SDL_ALPHA_OPAQUE};

    // Main title
    showText(renderer,
             APP_WIDTH / 2 - 180,
             APP_HEIGHT / 2 - 120,
             "Level Complete, Well done",
             titleColor);

    // Subtitle with level number
    char levelLine[64];
    SDL_snprintf(levelLine, sizeof(levelLine), "Level %d Complete", currentLevel);
    showText(renderer,
             APP_WIDTH / 2 - 120,
             APP_HEIGHT / 2 - 80,
             levelLine,
             normalColor);

    // Options
    SDL_Color nextColor = (levelCompleteSelection == 0) ? selectColor : normalColor;
    SDL_Color backColor = (levelCompleteSelection == 1) ? selectColor : normalColor;

    showText(renderer,
             APP_WIDTH / 2 - 100,
             APP_HEIGHT / 2,
             "Next Level",
             nextColor);

    showText(renderer,
             APP_WIDTH / 2 - 100,
             APP_HEIGHT / 2 + 40,
             "Back to Main Menu",
             backColor);

    showText(renderer,
             20,
             APP_HEIGHT - 40,
             "Use W/S or Up/Down to select, Enter to confirm, Esc for Main Menu",
             (SDL_Color){180, 180, 180, SDL_ALPHA_OPAQUE});

    SDL_RenderPresent(renderer);
}

// =====================
//       HELPERS
// =====================
// Compute top-left of the 4x4 puzzle area
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

    if (dir == 'W') newX--;
    else if (dir == 'S') newX++;
    else if (dir == 'A') newY--;
    else if (dir == 'D') newY++;
    else return;

    player.direction = dir;
    if (!inBounds(newX, newY)) return;
    if (map[LAYER_WORLD][newX][newY] == TILE_WALL) return;

    player.position_x = newX;
    player.position_y = newY;
}

void interact(void) {
    for (int l = MAP_LAYERS - 1; l >= LAYER_PIECE_BASE; --l) {
        char tile = map[l][player.position_x][player.position_y];
        if (tile >= 'A' && tile <= 'Z') {
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

// Regions depend on level
bool is_tile_in_orange(int x, int y)
{
    int px0, py0;
    get_puzzle_origin(&px0, &py0);

    if (currentLevel == 1) {
        // Level 1: 2x2 block in center of 4x4
        return ((x == px0 + 1 && (y == py0 + 1 || y == py0 + 2)) ||
                (x == px0 + 2 && (y == py0 + 1 || y == py0 + 2)));
    } else if (currentLevel == 2) {
        // Level 2: horizontal line along bottom row of puzzle (4 cells)
        if (x == px0 + 3 && y >= py0 && y <= py0 + 3)
            return true;
        return false;
    } else { 
        // Level 3: TOP row of puzzle (4 cells)
        if (x == px0 && y >= py0 && y <= py0 + 3)
            return true;
        return false;
    }
}

bool is_tile_in_purple(int x, int y)
{
    int px0, py0;
    get_puzzle_origin(&px0, &py0);

    if (currentLevel == 1) {
        // Level 1: L-shape
        if (x == px0 && (y == py0 || y == py0 + 1 || y == py0 + 2))
            return true;
        if (x == px0 + 1 && y == py0)
            return true;
        return false;
    } else if (currentLevel == 2) {
        // Level 2: L-shape matching piece D orientation
        if (x == px0     && y == py0 + 2) return true;
        if (x == px0 + 1 && y == py0 + 2) return true;
        if (x == px0 + 2 && (y == py0 + 2 || y == py0 + 3)) return true;
        return false;
    } else { 
        // Level 3: BOTTOM row of puzzle (4 cells)
        if (x == px0 + 3 && y >= py0 && y <= py0 + 3)
            return true;
        return false;
    }
}

bool is_tile_in_green(int x, int y)
{
    if (currentLevel != 3) return false;

    int px0, py0;
    get_puzzle_origin(&px0, &py0);
    // Level 3: 2x2 block in the center of the 4x4
    if (x >= px0 + 1 && x <= px0 + 2 &&
        y >= py0 + 1 && y <= py0 + 2)
        return true;

    return false;
}

int findPieceIndexById(char id)
{
    for (int i = 0; i < numPieces; i++)
        if (pieces[i].id == id)
            return i;
    return -1;
}

// Find which piece/tile occupies (x,y)
void findPieceAndTileAt(int x, int y, int *pieceIndex, int *tileIndex)
{
    *pieceIndex = -1;
    *tileIndex  = -1;
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
                *tileIndex  = t;
                return;
            }
        }
    }
}

bool is_tile_in_puzzle_area(int x, int y) {
    int x0, y0;
    get_puzzle_origin(&x0, &y0);
    const int h = 4, w = 4;
    return (x >= x0 && x < x0 + h && y >= y0 && y < y0 + w);
}

bool allPiecesFitInPuzzleArea(void) {
    for (int i = 0; i < numPieces; i++) {
        struct Piece p = pieces[i];
        for (int t = 0; t < p.size; t++) {
            int x = p.baseX + p.tiles[t][0];
            int y = p.baseY + p.tiles[t][1];

            // Must be inside puzzle area
            if (!is_tile_in_puzzle_area(x, y)) return false;

            // The top tile must be the piece itself (no overlaps)
            char top = get_top_tile(x, y);
            if (top != p.id) return false;
        }
    }
    return true;
}

void init_world_layer(void)
{
    // Floor
    for (int x = 0; x < MAP_ROWS; x++)
        for (int y = 0; y < MAP_COLS; y++)
            map[LAYER_WORLD][x][y] = TILE_FLOOR;

    // Border walls
    for (int y = 0; y < MAP_COLS; y++)
    {
        map[LAYER_WORLD][0][y]            = TILE_WALL;
        map[LAYER_WORLD][MAP_ROWS - 1][y] = TILE_WALL;
    }
    for (int x = 0; x < MAP_ROWS; x++)
    {
        map[LAYER_WORLD][x][0]            = TILE_WALL;
        map[LAYER_WORLD][x][MAP_COLS - 1] = TILE_WALL;
    }

    // Clear piece layers
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

bool canPlace(int index) {
    struct Piece p = pieces[index];
    for (int i = 0; i < p.size; i++) {
        int x = p.baseX + p.tiles[i][0];
        int y = p.baseY + p.tiles[i][1];
        if (!inBounds(x, y)) return false;
        if (map[LAYER_WORLD][x][y] == TILE_WALL) return false;
    }
    return true;
}

void puzzle_area(void)
{
    const int h = 4, w = 4;
    int x0 = (MAP_ROWS - h) / 2;
    int y0 = (MAP_COLS - w) / 2;

    for (int x = x0; x < x0 + h; x++)
    {
        for (int y = y0; y < y0 + w; y++)
        {
            map[LAYER_WORLD][x][y] = TILE_PUZZLE;
        }
    }
}

void init_constraints(int level)
{
    if (level == 1) {
        orangeConstraint.targetSum = 4;   // level 1 design
        purpleConstraint.targetSum = 9;
    } else if (level == 2) {
        orangeConstraint.targetSum = 14;  // level 2 design
        purpleConstraint.targetSum = 6;
    } else { // level 3
        // From the chosen solvable arrangement with 4 T-shaped pieces:
        orangeConstraint.targetSum = 11;  // top row sum
        purpleConstraint.targetSum = 14;  // bottom row sum
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
            SDL_Log("Clean");
            set_tile(layer, x, y, TILE_EMPTY);
        }
    }
}

void rotatePiece(int index) {
    // 90-degree rotation: (x,y) -> (y,-x)
    for (int i = 0; i < pieces[index].size; i++) {
        int x = pieces[index].tiles[i][0];
        int y = pieces[index].tiles[i][1];
        pieces[index].tiles[i][0] = y;
        pieces[index].tiles[i][1] = -x;
    }
}

void initPieces(int level)
{
    numPieces = 0;

    if (level == 1) {
        // ===== LEVEL 1 PIECES =====
        // 2x2 Square (piece A) – all 1's so sum=4 in orange area
        struct Piece square = {
            'A', 4,
            {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
            {1, 1, 1, 1},
            3, 3, true
        };

        // 1x4 Line (piece B) – 2 3 4 5
        struct Piece line = {
            'B', 4,
            {{0, 0}, {0, 1}, {0, 2}, {0, 3}},
            {2, 3, 4, 5},
            6, 3, true
        };

        // L-shape (piece C) – all 2's so sum=8 in purple L
        struct Piece lshape1 = {
            'C', 4,
            {{0, 1}, {1, 1}, {2, 1}, {2, 0}},
            {2, 2, 2, 2},
            2, 15, true
        };

        // Mirrored L-shape (piece D)
        struct Piece lshape2 = {
            'D', 4,
            {{0, 0}, {1, 0}, {2, 0}, {2, 1}},
            {2, 2, 3, 2},
            8, 15, true
        };

        pieces[numPieces++] = square;
        pieces[numPieces++] = line;
        pieces[numPieces++] = lshape1;
        pieces[numPieces++] = lshape2;
    } else if (level == 2) {
        // ===== LEVEL 2 PIECES =====

        // 2x2 Square (piece A)
        struct Piece square = {
            'A', 4,
            {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
            {1, 1, 1, 1},
            3, 3, true
        };

        // 1x4 Line (piece B) – 1,4,4,5
        struct Piece line = {
            'B', 4,
            {{0, 0}, {0, 1}, {0, 2}, {0, 3}},
            {1, 4, 4, 5},
            6, 3, true
        };

        // L-shape (piece C)
        struct Piece lshape1 = {
            'C', 4,
            {{0, 1}, {1, 1}, {2, 1}, {2, 0}},
            {3, 1, 2, 2},
            2, 15, true
        };

        // Mirrored L-shape (piece D) – 1,2,3,4
        struct Piece lshape2 = {
            'D', 4,
            {{0, 0}, {1, 0}, {2, 0}, {2, 1}},
            {1, 2, 3, 4},
            8, 15, true
        };

        pieces[numPieces++] = square;
        pieces[numPieces++] = line;
        pieces[numPieces++] = lshape1;
        pieces[numPieces++] = lshape2;
    } else {
        // ===== LEVEL 3 PIECES =====
        // All T-shaped, with values chosen so that:
        // - all central 2x2 "green" cells share the same value (2)
        // - orange (top row) sum = 11
        // - purple (bottom row) sum = 14
        //
        // T-shape canonical local coords (like Tetris T):
        //   . X .
        //   X X X
        // tiles[] = { {0,1}, {1,0}, {1,1}, {1,2} }

        struct Piece pieceA = {
            'A', 4,
            {{0, 1}, {1, 0}, {1, 1}, {1, 2}},
            {2, 1, 3, 4},   // index 0 (green cell) = 2
            3, 3, true
        };

        struct Piece pieceB = {
            'B', 4,
            {{0, 1}, {1, 0}, {1, 1}, {1, 2}},
            {2, 5, 1, 1},   // index 0 = 2
            6, 3, true
        };

        struct Piece pieceC = {
            'C', 4,
            {{0, 1}, {1, 0}, {1, 1}, {1, 2}},
            {2, 1, 2, 3},   // index 0 = 2
            2, 15, true
        };

        struct Piece pieceD = {
            'D', 4,
            {{0, 1}, {1, 0}, {1, 1}, {1, 2}},
            {2, 6, 1, 1},   // index 0 = 2
            8, 15, true
        };

        pieces[numPieces++] = pieceA;
        pieces[numPieces++] = pieceB;
        pieces[numPieces++] = pieceC;
        pieces[numPieces++] = pieceD;
    }

    // Clear piece layers
    for (int l = LAYER_PIECE_BASE; l < MAP_LAYERS; ++l)
        for (int x = 0; x < MAP_ROWS; ++x)
            for (int y = 0; y < MAP_COLS; ++y)
                map[l][x][y] = TILE_EMPTY;

    // Place each piece
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
