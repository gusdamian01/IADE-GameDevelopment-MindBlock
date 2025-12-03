#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

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
#define TILE_PUZZLE 'P'   // the general puzzle area background (4x4)

// Constraint targets (LEVEL 2)
#define ORANGE_TARGET_SUM 10
#define PURPLE_TARGET_SUM 12

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
    char pieceId;  // kept for compatibility, but unused in level 2
} ConstraintRegion;

// GLOBALS 
char map[MAP_LAYERS][MAP_ROWS][MAP_COLS];
struct Player player = {5, 5, false, '\0'};
struct Piece pieces[MAX_PIECES];
int numPieces = 0;

// Two constraints: orange & purple (multi-piece, level 2)
ConstraintRegion orangeConstraint;
ConstraintRegion purpleConstraint;

// Value emojis (1..7)
static const char* VALUE_EMOJI[7] = {
    "1️⃣ ","2️⃣ ","3️⃣ ","4️⃣ ","5️⃣ ","6️⃣ ","7️⃣ "
};

// FUNCTION DECLARATIONS
void init_world_layer(void);
void puzzle_area(void);
void init_constraints(void);
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
int  findPieceIndexById(char id);

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

// Compute top-left of the 4x4 puzzle area
void get_puzzle_origin(int *px0, int *py0) {
    const int h = 4, w = 4;
    *px0 = (MAP_ROWS - h) / 2;
    *py0 = (MAP_COLS - w) / 2;
}

// IMPLEMENTATIONS

void init_world_layer(void) {
    // Floor
    for (int x = 0; x < MAP_ROWS; x++)
        for (int y = 0; y < MAP_COLS; y++)
            map[LAYER_WORLD][x][y] = TILE_FLOOR;

    // Border walls
    for (int y = 0; y < MAP_COLS; y++) {
        map[LAYER_WORLD][0][y] = TILE_WALL;
        map[LAYER_WORLD][MAP_ROWS-1][y] = TILE_WALL;
    }
    for (int x = 0; x < MAP_ROWS; x++) {
        map[LAYER_WORLD][x][0] = TILE_WALL;
        map[LAYER_WORLD][x][MAP_COLS-1] = TILE_WALL;
    }

    // Clear piece layers
    for (int l = LAYER_PIECE_BASE; l < MAP_LAYERS; l++)
        for (int x = 0; x < MAP_ROWS; x++)
            for (int y = 0; y < MAP_COLS; y++)
                map[l][x][y] = TILE_EMPTY;
}

// Place the overall puzzle area (4x4) at the center
void puzzle_area(void) {
    const int h = 4, w = 4;
    int x0 = (MAP_ROWS - h) / 2;
    int y0 = (MAP_COLS - w) / 2;

    for (int x = x0; x < x0 + h; x++) {
        for (int y = y0; y < y0 + w; y++) {
            map[LAYER_WORLD][x][y] = TILE_PUZZLE; 
        }
    }
}

// Initialize constraints for level 2 (multi-piece)
void init_constraints(void) {
    // Orange = full top row of the 4x4 puzzle, target sum 10
    orangeConstraint.targetSum = ORANGE_TARGET_SUM;
    orangeConstraint.pieceId   = '\0'; // unused in level 2

    // Purple = bottom-right 2x2 of the puzzle, target sum 12
    purpleConstraint.targetSum = PURPLE_TARGET_SUM;
    purpleConstraint.pieceId   = '\0'; // unused in level 2
}

// Find which piece/tile occupies (x,y)
void findPieceAndTileAt(int x, int y, int *pieceIndex, int *tileIndex) {
    *pieceIndex = -1;
    *tileIndex  = -1;
    for (int i = 0; i < numPieces; ++i) {
        struct Piece *p = &pieces[i];
        for (int t = 0; t < p->size; ++t) {
            int px = p->baseX + p->tiles[t][0];
            int py = p->baseY + p->tiles[t][1];
            if (px == x && py == y) {
                *pieceIndex = i;
                *tileIndex  = t;
                return;
            }
        }
    }
}

void printMap(void) {
    for (int x = 0; x < MAP_ROWS; x++) {
        for (int y = 0; y < MAP_COLS; y++) {

            // Player (when not controlling a piece)
            if (!player.controllingPiece &&
                x == player.position_x && y == player.position_y) {
                printf("😁");
                continue;
            }

            int pIdx, tIdx;
            findPieceAndTileAt(x, y, &pIdx, &tIdx);

            // 1) Piece tiles (per-tile number) are on top of everything
            if (pIdx != -1 && tIdx != -1) {
                int val = pieces[pIdx].values[tIdx];
                if (val < 1) val = 1;
                if (val > 7) val = 7;
                printf("%s", VALUE_EMOJI[val - 1]);
                continue;
            }

            // 2) Constraint backgrounds: purple first, then orange
            if (is_tile_in_purple(x, y)) {
                printf("🟪");
                continue;
            }
            if (is_tile_in_orange(x, y)) {
                printf("🟧");
                continue;
            }

            char top = map[LAYER_WORLD][x][y];

            // 3) World / puzzle / floor
            if (top == TILE_WALL) {
                printf("⬛");
            }
            else if (top == TILE_PUZZLE) {
                printf("🔳"); // plain puzzle cells
            }
            else if (top == TILE_FLOOR || top == TILE_EMPTY) {
                printf("⬜");
            }
            else {
                printf(" ");
            }
        }
        printf("\n");
    }

    printf(
        "\nOrange 🟧 sum must be %d, current = %d\n"
        "Purple 🟪 sum must be %d, current = %d\n",
        orangeConstraint.targetSum, sumTilesInOrange(),
        purpleConstraint.targetSum, sumTilesInPurple()
    );
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
    // 90-degree rotation: (x,y) -> (y,-x)
    for (int i = 0; i < pieces[index].size; i++) {
        int x = pieces[index].tiles[i][0];
        int y = pieces[index].tiles[i][1];
        pieces[index].tiles[i][0] = y;
        pieces[index].tiles[i][1] = -x;
    }
}

// LEVEL 2 PIECES
void initPieces(void) {
    srand((unsigned)time(NULL));
    numPieces = 0;

    // LEVEL 2 uses 4 pieces, 4 tiles each => 16 tiles total.

    // Piece A – 1x4 L-shaped line, sums to 10 (good candidate for orange row)
    struct Piece A = {
        'A', 4,
        {{0,0},{1,0},{2,0},{2,1}},
        {2,2,3,3},    // total = 10
        3, 3, true
    };

    // Piece B – vertical line, mixed values
    struct Piece B = {
        'B', 4,
        {{0,0},{1,0},{2,0},{3,0}},
        {1,4,1,4},
        3, 14, true
    };

    // Piece C – 2x2 square, each 3 (good candidate for purple 2x2 = 12)
    struct Piece C = {
        'C', 4,
        {{0,0},{0,1},{1,0},{1,1}},
        {3,3,3,3},    // total = 12
        7, 3, true
    };

    // Piece D – L-shape, filler piece with varied values
    struct Piece D = {
        'D', 4,
        {{0,0},{1,0},{2,0},{2,1}},
        {1,2,2,1},
        7, 14, true
    };

    pieces[numPieces++] = A;
    pieces[numPieces++] = B;
    pieces[numPieces++] = C;
    pieces[numPieces++] = D;

    // Clear piece layers
    for (int l = LAYER_PIECE_BASE; l < MAP_LAYERS; ++l)
        for (int x = 0; x < MAP_ROWS; ++x)
            for (int y = 0; y < MAP_COLS; ++y)
                map[l][x][y] = TILE_EMPTY;

    // Place each piece
    for (int i = 0; i < numPieces; i++) placePieceOnMap(i);
}

bool is_tile_in_puzzle_area(int x, int y) {
    int x0, y0;
    get_puzzle_origin(&x0, &y0);
    const int h = 4, w = 4;
    return (x >= x0 && x < x0 + h && y >= y0 && y < y0 + w);
}

// LEVEL 2: Orange region = entire top row of the 4x4 puzzle
bool is_tile_in_orange(int x, int y) {
    int px0, py0;
    get_puzzle_origin(&px0, &py0);
    // top row: x == px0, y in [py0, py0+3]
    return (x == px0 && y >= py0 && y < py0 + 4);
}

// LEVEL 2: Purple region = bottom-right 2x2 of the puzzle
bool is_tile_in_purple(int x, int y) {
    int px0, py0;
    get_puzzle_origin(&px0, &py0);
    // rows px0+2, px0+3 and cols py0+2, py0+3
    if ((x == px0+2 || x == px0+3) &&
        (y == py0+2 || y == py0+3)) {
        return true;
    }
    return false;
}

// LEVEL 2: sum over ALL pieces (constraints are not tied to a single piece)
int sumTilesInOrange(void) {
    int sum = 0;
    for (int i = 0; i < numPieces; i++) {
        struct Piece p = pieces[i];
        for (int t = 0; t < p.size; t++) {
            int x = p.baseX + p.tiles[t][0];
            int y = p.baseY + p.tiles[t][1];
            if (is_tile_in_orange(x, y)) {
                sum += p.values[t];
            }
        }
    }
    return sum;
}

int sumTilesInPurple(void) {
    int sum = 0;
    for (int i = 0; i < numPieces; i++) {
        struct Piece p = pieces[i];
        for (int t = 0; t < p.size; t++) {
            int x = p.baseX + p.tiles[t][0];
            int y = p.baseY + p.tiles[t][1];
            if (is_tile_in_purple(x, y)) {
                sum += p.values[t];
            }
        }
    }
    return sum;
}

bool constraintsSatisfied(void) {
    if (sumTilesInOrange() != orangeConstraint.targetSum) return false;
    if (sumTilesInPurple() != purpleConstraint.targetSum) return false;
    return true;
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

// MAIN LOOP
int main(void) {
    printf("Hello Joe! 😁\n\n");
    printf("MindBlock – Level 2\n\n");

    init_world_layer();
    puzzle_area();
    init_constraints();
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
                if (!canPlace(index)) {
                    // rotate back 3 times (total 360°)
                    for (int i = 0; i < 3; i++) rotatePiece(index);
                }
                placePieceOnMap(index);
            }
            else {
                int dx = 0, dy = 0;
                if (input == 'W') dx = -1;
                else if (input == 'S') dx = 1;
                else if (input == 'A') dy = -1;
                else if (input == 'D') dy = 1;

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
            if (input == 'E') interact();
            else movePlayer(input);
        }

        // Victory: all tiles inside 4x4 puzzle area AND both constraints satisfied
        if (allPiecesFitInPuzzleArea() && constraintsSatisfied()) {
            printMap();
            printf("🎉 Level Complete. Both constraints satisfied!\n");
            break;
        }

        printf("\n\n");
    }

    return 0;
}
