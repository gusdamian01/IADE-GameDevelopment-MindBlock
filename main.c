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
static SDL_Texture *floorTexture         = NULL;
static SDL_Texture *puzzleTexture        = NULL;
static SDL_Texture *constraintTexture1   = NULL;
static SDL_Texture *constraintTexture2   = NULL;
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

// Constraint targets
#define ORANGE_TARGET_SUM 4 // piece A
#define PURPLE_TARGET_SUM 8 // piece C

// STRUCTURES
struct Player {
    int position_x;
    int position_y;
    bool controllingPiece;
    char controlledPieceId;
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
    char pieceId; // which piece this constraint applies to
} ConstraintRegion;

// GLOBALS
char map[MAP_LAYERS][MAP_ROWS][MAP_COLS];
struct Player player = {5, 5, false, '\0'};
struct Piece pieces[MAX_PIECES];
int numPieces = 0;

// Two constraints: orange (square) & purple (L-shape)
ConstraintRegion orangeConstraint;
ConstraintRegion purpleConstraint;

// (Now unused for rendering, but kept if you still print to console somewhere)
static const char *VALUE_EMOJI[7] = {
    "1️⃣ ", "2️⃣ ", "3️⃣ ", "4️⃣ ", "5️⃣ ", "6️⃣ ", "7️⃣ "
};

#define APP_NAME "MindBlock"

#define TEXTURE_WIDTH  32
#define TEXTURE_HEIGHT 32

#define APP_WIDTH  (20 * TEXTURE_WIDTH)
#define APP_HEIGHT (12 * TEXTURE_HEIGHT)

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;

// =====================
//  FUNCTION DECLARATIONS
// =====================
void init_world_layer(void);
void puzzle_area(void);
void init_constraints(void);
void initPieces(void);
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
int  sumTilesInOrange(void);
int  sumTilesInPurple(void);
bool constraintsSatisfied(void);
bool allPiecesFitInPuzzleArea(void);
void findPieceAndTileAt(int x, int y, int *pieceIndex, int *tileIndex);

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

bool constraintsSatisfied(void) {
    if (sumTilesInOrange() != orangeConstraint.targetSum) return false;
    if (sumTilesInPurple() != purpleConstraint.targetSum) return false;
    return true;
}


// SDL-specific helper
void load_value_textures(void);

// =====================
//        MAIN
// =====================
int main(void)
{
    // Initialize SDL Systems.
    window   = sdl_initialize_window(APP_NAME, APP_HEIGHT, APP_WIDTH);
    renderer = sdl_initialize_renderer(window);
    sdl_initialize_audio();

    init_world_layer();
    puzzle_area();
    init_constraints();
    initPieces();

    // Load Sprites
    playerTexture      = sdl_load_texture(renderer, "sprites/Joe.png");
    floorTexture       = sdl_load_texture(renderer, "sprites/PlayArea.png");
    puzzleTexture      = sdl_load_texture(renderer, "sprites/PuzzleArea.png");
    constraintTexture1 = sdl_load_texture(renderer, "sprites/Constraint1.png");
    constraintTexture2 = sdl_load_texture(renderer, "sprites/Constraint2.png");

    // Load tile-value textures (1..7)
    load_value_textures();

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
                if (event.key.key == SDLK_W) movePlayer('W');
                if (event.key.key == SDLK_A) movePlayer('A');
                if (event.key.key == SDLK_S) movePlayer('S');
                if (event.key.key == SDLK_D) movePlayer('D');

                if (player.controllingPiece) {
            int index = findPieceIndexById(player.controlledPieceId);
            if (index == -1) continue;

            if (event.key.key == SDLK_Q) {
                player.controllingPiece = false;
                player.controlledPieceId = '\0';
                player.position_x = pieces[index].baseX;
                player.position_y = pieces[index].baseY;
                printf("You placed the piece and returned to Joe form.\n");
            } 
            else if (event.key.key == SDLK_R) {
                removePieceFromMap(index);
                rotatePiece(index);
                if (!canPlace(index)) { for (int i = 0; i < 3; i++) rotatePiece(index); }
                placePieceOnMap(index);
            }
            else {
                int dx = 0, dy = 0;
                if (event.key.key == SDLK_D) dx = 1;
                else if (event.key.key == SDLK_A) dx = -1;
                else if (event.key.key == SDLK_S) dy = 1;
                else if (event.key.key == SDLK_W) dy = -1;

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
            if (event.key.key == SDLK_E) interact();
            else movePlayer(event.key.key);
        }
            }
        }

        // Victory: all tiles inside 4x4 puzzle area AND both constraints satisfied
        if (allPiecesFitInPuzzleArea() && constraintsSatisfied()) {
            printMap();
            printf("🎉 Level Complete. Both constraints satisfied!\n");
            break;
        }

        printMap();

        // Delay to maintain frame rate
        Uint32 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < FRAME_MS)
            SDL_Delay(FRAME_MS - elapsed);
    }

    // Cleanup textures
    for (int i = 0; i < 7; ++i) {
        if (valueTextures[i]) {
            SDL_DestroyTexture(valueTextures[i]);
            valueTextures[i] = NULL;
        }
    }
    if (playerTexture)      SDL_DestroyTexture(playerTexture);
    if (floorTexture)       SDL_DestroyTexture(floorTexture);
    if (puzzleTexture)      SDL_DestroyTexture(puzzleTexture);
    if (constraintTexture1) SDL_DestroyTexture(constraintTexture1);
    if (constraintTexture2) SDL_DestroyTexture(constraintTexture2);

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
    };

    for (int i = 0; i < 7; ++i)
    {
        valueTextures[i] = sdl_load_texture(renderer, paths[i]);
        if (!valueTextures[i]) {
            SDL_Log("Failed to load value texture %d from '%s'", i + 1, paths[i]);
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

    for (int x = 0; x < MAP_ROWS; x++)
    {
        for (int y = 0; y < MAP_COLS; y++)
        {
            // Player (when not controlling a piece)
            if (!player.controllingPiece &&
                x == player.position_x && y == player.position_y)
            {
                SDL_FRect playerRect = {
                    x * TEXTURE_WIDTH,
                    y * TEXTURE_HEIGHT,
                    TEXTURE_WIDTH,
                    TEXTURE_HEIGHT
                };
                SDL_RenderTexture(renderer, playerTexture, NULL, &playerRect);
                continue;
            }

            int pIdx, tIdx;
            findPieceAndTileAt(x, y, &pIdx, &tIdx);

            // 1) Piece tiles (per-tile number) are on top of everything
            if (pIdx != -1 && tIdx != -1)
            {
                int val = pieces[pIdx].values[tIdx];
                if (val < 1) val = 1;
                if (val > 7) val = 7;

                SDL_Texture *tex = valueTextures[val - 1];
                if (tex != NULL)
                {
                    SDL_FRect valRect = {
                        x * TEXTURE_WIDTH,
                        y * TEXTURE_HEIGHT,
                        TEXTURE_WIDTH,
                        TEXTURE_HEIGHT
                    };
                    SDL_RenderTexture(renderer, tex, NULL, &valRect);
                }
                else
                {
                    // Fallback: draw floor if texture missing
                    SDL_FRect fallbackRect = {
                        x * TEXTURE_WIDTH,
                        y * TEXTURE_HEIGHT,
                        TEXTURE_WIDTH,
                        TEXTURE_HEIGHT
                    };
                    SDL_RenderTexture(renderer, floorTexture, NULL, &fallbackRect);
                }
                continue;
            }

            // 2) Constraint backgrounds: purple first, then orange
            if (is_tile_in_purple(x, y))
            {
                SDL_FRect cRect = {
                    x * TEXTURE_WIDTH,
                    y * TEXTURE_HEIGHT,
                    TEXTURE_WIDTH,
                    TEXTURE_HEIGHT
                };
                SDL_RenderTexture(renderer, constraintTexture1, NULL, &cRect);
                continue;
            }

            
            if (is_tile_in_orange(x, y))
            {
                SDL_FRect cRect = {
                    x * TEXTURE_WIDTH,
                    y * TEXTURE_HEIGHT,
                    TEXTURE_WIDTH,
                    TEXTURE_HEIGHT
                };
                SDL_RenderTexture(renderer, constraintTexture2, NULL, &cRect);
                continue;
            }

            char top = map[LAYER_WORLD][x][y];

            // 3) World / puzzle / floor
            if (top == TILE_WALL)
            {
                // You can draw a wall texture here if you add one.
                // For now, do nothing (black background).
            }
            else if (top == TILE_PUZZLE)
            {
                SDL_FRect floorRect = {
                    x * TEXTURE_WIDTH,
                    y * TEXTURE_HEIGHT,
                    TEXTURE_WIDTH,
                    TEXTURE_HEIGHT
                };
                SDL_RenderTexture(renderer, puzzleTexture, NULL, &floorRect);
            }
            else if (top == TILE_FLOOR || top == TILE_EMPTY)
            {
                SDL_FRect floorRect = {
                    x * TEXTURE_WIDTH,
                    y * TEXTURE_HEIGHT,
                    TEXTURE_WIDTH,
                    TEXTURE_HEIGHT
                };
                SDL_RenderTexture(renderer, floorTexture, NULL, &floorRect);
            }
        }
    }

    showText(renderer, 100, 0, "MindBlock!", (SDL_Color){255, 255, 255, SDL_ALPHA_OPAQUE});
    SDL_RenderDebugTextFormat(
        renderer,
        (float)((APP_WIDTH - (charsize * 46)) / 2),
        APP_HEIGHT - charsize,
        "(This program has been running for %" SDL_PRIu64 " seconds.)",
        SDL_GetTicks() / 1000
    );

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

// =====================
//    GAME LOGIC
// =====================
void movePlayer(char dir)
{
    int newX = player.position_x;
    int newY = player.position_y;
    // NOTE: you swapped axes here compared to original text version.
    if (dir == 'W') newY--;
    else if (dir == 'S') newY++;
    else if (dir == 'A') newX--;
    else if (dir == 'D') newX++;
    else return;

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

int sumTilesInOrange(void)
{
    int sum = 0;
    for (int i = 0; i < numPieces; i++)
    {
        if (pieces[i].id != orangeConstraint.pieceId)
            continue;
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

// Orange region: 2x2 block in center of 4x4
// Coordinates relative to puzzle 4x4:
// (1,1),(1,2),(2,1),(2,2)
bool is_tile_in_orange(int x, int y)
{
    int px0, py0;
    get_puzzle_origin(&px0, &py0);
    return ((x == px0 + 1 && (y == py0 + 1 || y == py0 + 2)) ||
            (x == px0 + 2 && (y == py0 + 1 || y == py0 + 2)));
}

int findPieceIndexById(char id)
{
    for (int i = 0; i < numPieces; i++)
        if (pieces[i].id == id)
            return i;
    return -1;
}

// Purple region: L-shape described by:
// row0: 🟪 🟪 🟪 .
// row1: 🟪 .  .  .
// (relative to top-left of puzzle)
bool is_tile_in_purple(int x, int y)
{
    int px0, py0;
    get_puzzle_origin(&px0, &py0);
    // top row of L
    if (x == px0 && (y == py0 || y == py0 + 1 || y == py0 + 2))
        return true;
    // vertical down from left
    if (x == px0 + 1 && y == py0)
        return true;
    return false;
}

int sumTilesInPurple(void)
{
    int sum = 0;
    for (int i = 0; i < numPieces; i++)
    {
        if (pieces[i].id != purpleConstraint.pieceId)
            continue;
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
        map[LAYER_WORLD][0][y]          = TILE_WALL;
        map[LAYER_WORLD][MAP_ROWS - 1][y] = TILE_WALL;
    }
    for (int x = 0; x < MAP_ROWS; x++)
    {
        map[LAYER_WORLD][x][0]          = TILE_WALL;
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

void init_constraints(void)
{
    orangeConstraint.pieceId   = 'A';
    orangeConstraint.targetSum = ORANGE_TARGET_SUM;

    purpleConstraint.pieceId   = 'C';
    purpleConstraint.targetSum = PURPLE_TARGET_SUM;
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

void initPieces(void)
{
    srand((unsigned)time(NULL));
    numPieces = 0;

    // 2x2 Square (piece A) – all 1's so sum=4 in orange area
    struct Piece square = {
        'A', 4,
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
        {1, 1, 1, 1},
        3, 3, true
    };

    // 1x4 Line (piece B) – varied values: 2 3 4 5
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
