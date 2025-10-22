#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>

// ====================== CONFIGURATIONS ======================
#define MAP_ROWS 12 
#define MAP_COLS 20
#define MAX_PIECES 10

// Tile codes
#define TILE_WALL 'W'
#define TILE_FLOOR 'F'
#define TILE_PUZZLE 'P'
#define TILE_MATH 'M'

// ====================== STRUCTURES ======================
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
};

// ====================== GLOBALS ======================
char map[MAP_ROWS][MAP_COLS] = {
    {'W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W'},
    {'W','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','W'},
    {'W','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','W'},
    {'W','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','W'},
    {'W','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','W'},
    {'W','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','W'},
    {'W','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','W'},
    {'W','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','W'},
    {'W','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','W'},
    {'W','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','W'},
    {'W','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','F','W'},
    {'W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W'},
};

struct Player player = {5, 5, false, '\0'};
struct Piece pieces[MAX_PIECES];
int numPieces = 0;

// ====================== FUNCTION DECLARATIONS ======================
void printMap(void);
char readUserInput(void);

bool inBounds(int y, int x);
bool canMovePiece(struct Piece *p, int dx, int dy);

void movePiece(struct Piece *p, int dx, int dy);
void movePlayer(char dir);
void placePieceOnMap(struct Piece *p);
void removePieceFromMap(struct Piece *p);
void initPieces(void);
void interact(void);

struct Piece* findPieceById(char id);
bool canPlace(struct Piece *p);

void rotatePiece(struct Piece *p);

// ====================== MAIN ======================
int main(void) {
    printf("Hello Wizard! 🧙\n\n");

    initPieces();

    while (1) {
        printMap();

        if (player.controllingPiece)
            printf("🧩 You are moving piece %c. (WASD move, R rotate, Q place)\n> ", player.controlledPieceId);
        else
            printf("🧙 You are the wizard. (WASD move, E control piece)\n> ");

        char input = readUserInput();

        if (player.controllingPiece) {
            struct Piece *p = findPieceById(player.controlledPieceId);
            if (!p) continue;

            if (input == 'Q') {
                player.controllingPiece = false;
                player.controlledPieceId = '\0';
                player.position_x = p->baseX;
                player.position_y = p->baseY;
                printf("You placed the piece and returned to wizard form.\n");
            } 
            else if (input == 'R') {
                removePieceFromMap(p);
                rotatePiece(p);
                if (!canPlace(p)) { // undo if invalid
                    for (int i = 0; i < 3; i++) rotatePiece(p);
                }
                placePieceOnMap(p);
            }
            else {
                int dx = 0, dy = 0;
                if (input == 'W') dx = -1;
                else if (input == 'S') dx = 1;
                else if (input == 'A') dy = -1;
                else if (input == 'D') dy = 1;

                if (canMovePiece(p, dx, dy)) {
                    removePieceFromMap(p);
                    movePiece(p, dx, dy);
                    placePieceOnMap(p);
                }
            }
        } 
        else {
            if (input == 'E' || input == 'e') interact();
            else movePlayer(input);
        }

        printf("\n\n");
    }

    return 0;
}

// ====================== IMPLEMENTATIONS ======================

void printMap(void) {
    for (int x = 0; x < MAP_ROWS; x++) {
        for (int y = 0; y < MAP_COLS; y++) {
            if (!player.controllingPiece && x == player.position_x && y == player.position_y)
                printf("😁");
            else if (map[x][y] == 'W')
                printf("🟥");
            else if (map[x][y] == 'F')
                printf("⬜");
            else if (map[x][y] >= 'A' && map[x][y] <= 'Z')
                printf("🔷");
        }
        printf("\n");
    }
}

char readUserInput(void) {
    char input;
    scanf(" %c", &input);
    return toupper(input);
}

bool inBounds(int y, int x) {
    return (y >= 0 && y < MAP_ROWS && x >= 0 && x < MAP_COLS);
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
    if (map[newX][newY] == 'W') return;

    player.position_x = newX;
    player.position_y = newY;
}

void interact(void) {
    char tile = map[player.position_x][player.position_y];
    if (tile >= 'A' && tile <= 'Z') {
        player.controllingPiece = true;
        player.controlledPieceId = tile;
        printf("You are now controlling piece %c!\n", tile);
    }
}

struct Piece* findPieceById(char id) {
    for (int i = 0; i < numPieces; i++) {
        if (pieces[i].id == id) return &pieces[i];
    }
    return NULL;
}

// =============== PIECE SYSTEM ===============

void placePieceOnMap(struct Piece *p) {
    for (int i = 0; i < p->size; i++) {
        int x = p->baseX + p->tiles[i][0];
        int y = p->baseY + p->tiles[i][1];
        if (inBounds(x, y)) map[x][y] = p->id;
    }
}

void removePieceFromMap(struct Piece *p) {
    for (int i = 0; i < p->size; i++) {
        int x = p->baseX + p->tiles[i][0];
        int y = p->baseY + p->tiles[i][1];
        if (inBounds(x, y)) map[x][y] = TILE_FLOOR;
    }
}

bool canPlace(struct Piece *p) {
    for (int i = 0; i < p->size; i++) {
        int x = p->baseX + p->tiles[i][0];
        int y = p->baseY + p->tiles[i][1];
        if (!inBounds(x, y)) return false;
        if (map[x][y] == 'W') return false;
    }
    return true;
}

bool canMovePiece(struct Piece *p, int dx, int dy) {
    for (int i = 0; i < p->size; i++) {
        int newX = p->baseX + p->tiles[i][0] + dx;
        int newY = p->baseY + p->tiles[i][1] + dy;
        if (!inBounds(newX, newY)) return false;
        if (map[newX][newY] == 'W') return false;
    }
    return true;
}

void movePiece(struct Piece *p, int dx, int dy) {
    p->baseX += dx;
    p->baseY += dy;
}

void rotatePiece(struct Piece *p) {
    for (int i = 0; i < p->size; i++) {
        int x = p->tiles[i][0];
        int y = p->tiles[i][1];
        p->tiles[i][0] = y;
        p->tiles[i][1] = -x;
    }
}

void initPieces(void) {
    // A - Square (O)
    struct Piece square = {'A', 4, {{0,0},{0,1},{1,0},{1,1}}, 3, 3, true};
    pieces[numPieces++] = square;

    // B - Line (I)
    struct Piece line = {'B', 4, {{0,0},{0,1},{0,2},{0,3}}, 6, 3, true};
    pieces[numPieces++] = line;

    // C - L shape
    struct Piece lshape = {'C', 4, {{0,0},{1,0},{2,0},{2,1}}, 2, 15, true};
    pieces[numPieces++] = lshape;

    // D - T shape
    struct Piece tshape = {'D', 4, {{0,1},{1,0},{1,1},{1,2}}, 8, 3, true};
    pieces[numPieces++] = tshape;

    // E - S shape
    struct Piece sshape = {'E', 4, {{0,1},{0,2},{1,0},{1,1}}, 8, 15, true};
    pieces[numPieces++] = sshape;

    // Place all pieces
    for (int i = 0; i < numPieces; i++) placePieceOnMap(&pieces[i]);
}
