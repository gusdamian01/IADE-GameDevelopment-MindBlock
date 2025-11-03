#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// === CONFIGURATIONS ===
#define MAP_ROWS   12
#define MAP_COLS   20
#define MAX_PIECES 10
#define MAP_LAYERS (1 + MAX_PIECES)   // 0 = world, 1..MAX_PIECES = pieces

#define LAYER_WORLD       0
#define LAYER_PIECE_BASE  1
#define PIECE_LAYER(i)    (LAYER_PIECE_BASE + (i))  // i = piece index 0..MAX_PIECES-1

// Tile codes
#define TILE_EMPTY  '\0'
#define TILE_WALL   'W'
#define TILE_FLOOR  'F'
#define TILE_PUZZLE 'P'   // CENTER GRID (prints 🔳)

// === STRUCTURES ===
struct Player {
    int position_x;
    int position_y;
    bool controllingPiece;
    char controlledPieceId;
};

struct Piece {
    char id;
    int size;
    int tiles[4][2];
    int baseX;
    int baseY;
    bool placed;
    int color; // 0–6 index into PIECE_EMOJI
};

// GLOBALS 
char map[MAP_LAYERS][MAP_ROWS][MAP_COLS];
struct Player player = {5, 5, false, '\0'};
struct Piece pieces[MAX_PIECES];
int numPieces = 0;

// Piece color emojis
static const char* PIECE_EMOJI[7] = {
    "🟥","🟦","🟨","🟩","🟪","🟧","🟫"
};

// === FUNCTION DECLARATIONS ===
void init_world_layer(void);
void add_center_grid_4x4(void);
void initPieces(void);
void printMap(void);
char readUserInput(void);

void movePlayer(char dir);
void interact(void);

void placePieceOnMap(int index);
void removePieceFromMap(int index);
bool canPlace(int index);
bool canMovePiece(int index, int dx, int dy);
void movePiece(int index, int dx, int dy);
void rotatePiece(int index);
int findPieceIndexById(char id);
static const char* emoji_for_piece_id(char id);

static inline bool inBounds(int y, int x) {
    return (y >= 0 && y < MAP_ROWS && x >= 0 && x < MAP_COLS);
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

//IMPLEMENTATIONS

void init_world_layer(void) {
    // World Floor everywhere
    for (int x = 0; x < MAP_ROWS; x++)
        for (int y = 0; y < MAP_COLS; y++)
            map[LAYER_WORLD][x][y] = TILE_FLOOR;

    // Border Walls
    for (int y = 0; y < MAP_COLS; y++) {
        map[LAYER_WORLD][0][y] = TILE_WALL;
        map[LAYER_WORLD][MAP_ROWS-1][y] = TILE_WALL;
    }
    for (int x = 0; x < MAP_ROWS; x++) {
        map[LAYER_WORLD][x][0] = TILE_WALL;
        map[LAYER_WORLD][x][MAP_COLS-1] = TILE_WALL;
    }

    // Clear all piece layers
    for (int l = LAYER_PIECE_BASE; l < MAP_LAYERS; l++)
        for (int x = 0; x < MAP_ROWS; x++)
            for (int y = 0; y < MAP_COLS; y++)
                map[l][x][y] = TILE_EMPTY;
}

// 🔳 
void add_center_grid_4x4(void) {
    const int h = 4, w = 4;
    int x0 = (MAP_ROWS - h+1) / 2;
    int y0 = (MAP_COLS - w) / 2;

    for (int x = x0; x < x0 + h; x++) {
        for (int y = y0; y < y0 + w; y++) {
            map[LAYER_WORLD][x][y] = TILE_PUZZLE; 
        }
    }
}

void printMap(void) {
    for (int x = 0; x < MAP_ROWS; x++) {
        for (int y = 0; y < MAP_COLS; y++) {
            if (!player.controllingPiece && x == player.position_x && y == player.position_y) {
                printf("😁");
                continue;
            }
            char top = get_top_tile(x, y);
            if (top == TILE_WALL)              printf("⬛");
            else if (top == TILE_PUZZLE)       printf("🔳");  
            else if (top == TILE_FLOOR || top == TILE_EMPTY) printf("⬜");
            else if (top >= 'A' && top <= 'Z') printf("%s", emoji_for_piece_id(top));
            else                                printf(" ");
        }
        printf("\n");
    }
}

char readUserInput(void) {
    char input;
    scanf(" %c", &input);
    return (char)toupper((unsigned char)input);
}

void movePlayer(char dir) {
    int newX = player.position_x;
    int newY = player.position_y;
    if (dir == 'W') newX--;
    else if (dir == 'S') newX++;
    else if (dir == 'A') newY--;
    else if (dir == 'D') newY++;
    else return;

    if (!inBounds(newX, newY)) return;
    if (map[LAYER_WORLD][newX][newY] == TILE_WALL) return;

    player.position_x = newX;
    player.position_y = newY;
}

void interact(void) {
    // topmost piece at this cell
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

int findPieceIndexById(char id) {
    for (int i = 0; i < numPieces; i++)
        if (pieces[i].id == id) return i;
    return -1;
}

static const char* emoji_for_piece_id(char id) {
    int index = findPieceIndexById(id);
    if (index == -1) return "❓";
    return PIECE_EMOJI[pieces[index].color % 7];
}

void placePieceOnMap(int index) {
    int layer = PIECE_LAYER(index);
    struct Piece p = pieces[index];
    for (int i = 0; i < p.size; i++) {
        int x = p.baseX + p.tiles[i][0];
        int y = p.baseY + p.tiles[i][1];
        if (inBounds(x, y)) set_tile(layer, x, y, p.id);
    }
}

void removePieceFromMap(int index) {
    int layer = PIECE_LAYER(index);
    struct Piece p = pieces[index];
    for (int i = 0; i < p.size; i++) {
        int x = p.baseX + p.tiles[i][0];
        int y = p.baseY + p.tiles[i][1];
        if (inBounds(x, y) && map[layer][x][y] == p.id)
            set_tile(layer, x, y, TILE_EMPTY);
    }
}

//Movement rules
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

bool canMovePiece(int index, int dx, int dy) {
    struct Piece p = pieces[index];
    for (int i = 0; i < p.size; i++) {
        int nx = p.baseX + p.tiles[i][0] + dx;
        int ny = p.baseY + p.tiles[i][1] + dy;
        if (!inBounds(nx, ny)) return false;
        if (map[LAYER_WORLD][nx][ny] == TILE_WALL) return false;
    }
    return true;
}

void movePiece(int index, int dx, int dy) {
    pieces[index].baseX += dx;
    pieces[index].baseY += dy;
}

void rotatePiece(int index) {
    for (int i = 0; i < pieces[index].size; i++) {
        int x = pieces[index].tiles[i][0];
        int y = pieces[index].tiles[i][1];
        pieces[index].tiles[i][0] = y;
        pieces[index].tiles[i][1] = -x;
    }
}

void initPieces(void) {
    srand((unsigned)time(NULL));
    numPieces = 0;

    struct Piece square = {'A', 4, {{0,0},{0,1},{1,0},{1,1}}, 3, 3, true, 0};
    struct Piece line   = {'B', 4, {{0,0},{0,1},{0,2},{0,3}}, 6, 3, true, 0};
    struct Piece lshape = {'C', 4, {{0,0},{1,0},{2,0},{2,1}}, 2,15, true, 0};
    struct Piece tshape = {'D', 4, {{0,1},{1,0},{1,1},{1,2}}, 8, 3, true, 0};
    struct Piece sshape = {'E', 4, {{0,1},{0,2},{1,0},{1,1}}, 8,15, true, 0};

    pieces[numPieces++] = square;
    pieces[numPieces++] = line;
    pieces[numPieces++] = lshape;
    pieces[numPieces++] = tshape;
    pieces[numPieces++] = sshape;

    for (int i = 0; i < numPieces; i++)
        pieces[i].color = rand() % 7;

    // Clear all piece layers
    for (int l = LAYER_PIECE_BASE; l < MAP_LAYERS; ++l)
        for (int x = 0; x < MAP_ROWS; ++x)
            for (int y = 0; y < MAP_COLS; ++y)
                map[l][x][y] = TILE_EMPTY;

    // Place each piece in its own layer
    for (int i = 0; i < numPieces; i++) placePieceOnMap(i);
}

//MAIN LOOP
int main(void) {
    printf("Hello Joe! 😁\n\n");

    init_world_layer();
    add_center_grid_4x4();
    initPieces();

    while (1) {
        printMap();

        if (player.controllingPiece)
            printf("🧩 You are moving piece %c. (WASD move, R rotate, Q place)\n> ",
                   player.controlledPieceId);
        else
            printf("😁 You are Joe. (WASD move, E control piece)\n> ");

        char input = readUserInput();

        if (player.controllingPiece) {
            int index = findPieceIndexById(player.controlledPieceId);
            if (index == -1) continue;

            if (input == 'Q') {
                player.controllingPiece = false;
                player.controlledPieceId = '\0';
                player.position_x = pieces[index].baseX;
                player.position_y = pieces[index].baseY;
                printf("You placed the piece and returned to Joe form.\n");
            } 
            else if (input == 'R') {
                removePieceFromMap(index);
                rotatePiece(index);
                if (!canPlace(index)) { for (int i = 0; i < 3; i++) rotatePiece(index); }
                placePieceOnMap(index);
            }
            else {
                int dx = 0, dy = 0;
                if (input == 'W') dx = -1;
                else if (input == 'S') dx = 1;
                else if (input == 'A') dy = -1;
                else if (input == 'D') dy = 1;

                if (canMovePiece(index, dx, dy)) {
                    removePieceFromMap(index);
                    movePiece(index, dx, dy);
                    placePieceOnMap(index);
                }
            }
        } 
        else {
            if (input == 'E') interact();
            else movePlayer(input);
        }

        printf("\n\n");
    }

    return 0;
}