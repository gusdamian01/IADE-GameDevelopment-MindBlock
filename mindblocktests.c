#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include "start_menu.h"

// === CONFIGURATIONS
#define MAP_ROWS 12 
#define MAP_COLS 20
#define MAP_LAYERS 2
#define MAX_PIECES 10

// Tile codes
#define TILE_WALL 'W'
#define TILE_FLOOR 'F'
#define TILE_PUZZLE 'P'
#define TILE_MATH 'M'

// Piece color emojis
static const char* PIECE_EMOJI[7] = {
    "🟥","🟦","🟨","🟩","🟪","🟧","🟫"
    //"🕐","🕑","🕒","🕓","🕔","🕕"
};

// === STRUCTURES
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
    int color;            // 0–6 index into PIECE_EMOJI
};

// === GLOBALS
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

// === FUNCTION DECLARATIONS
void printMap(void);
char readUserInput(void);

bool inBounds(int y, int x);
bool canMovePiece(int index, int dx, int dy);

void movePiece(int index, int dx, int dy);
void movePlayer(char dir);
void placePieceOnMap(int index);
void removePieceFromMap(int index);
void initPieces(void);
void interact(void);

int findPieceIndexById(char id);
bool canPlace(int index);
void rotatePiece(int index);

// Helper to get piece color emoji
static const char* emoji_for_piece_id(char id);

// === MAIN
int main(void) {
    printf("Hello Joe! 😁\n\n");

    initPieces();

    while (1) {
        printMap();

        if (player.controllingPiece)
            printf("🧩 You are moving piece %c. (WASD move, R rotate, Q place)\n> ", player.controlledPieceId);
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

                if (canMovePiece(index, dx, dy)) {
                    removePieceFromMap(index);
                    movePiece(index, dx, dy);
                    placePieceOnMap(index);
                }
            }
        } 
        else {
            if (input == 'E' || input == 'e' ) interact();
            else movePlayer(input);
        }

        printf("\n\n");
    }

    return 0;
}

// === IMPLEMENTATIONS

void printMap(void) {
    for (int x = 0; x < MAP_ROWS; x++) {
        for (int y = 0; y < MAP_COLS; y++) {
            if (!player.controllingPiece && x == player.position_x && y == player.position_y)
                printf("😁");
            else if (map[x][y] == 'W')
                printf("⬛");
            else if (map[x][y] == 'F')
                printf("⬜");
            else if (map[x][y] >= 'A' && map[x][y] <= 'Z')
                printf("%s", emoji_for_piece_id(map[x][y]));
        }
        printf("\n");
    }
}

char readUserInput(void) {
    char input;
    scanf(" %c", &input);
    return (char)toupper((unsigned char)input);
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

int findPieceIndexById(char id) {
    for (int i = 0; i < numPieces; i++) {
        if (pieces[i].id == id) return i;
    }
    return -1;
}

static const char* emoji_for_piece_id(char id) {
    int index = findPieceIndexById(id);
    if (index == -1) return "❓";
    return PIECE_EMOJI[pieces[index].color % 7];
}

// === PIECE SYSTEM

void placePieceOnMap(int index) {
    struct Piece piece = pieces[index];
    for (int i = 0; i < piece.size; i++) {
        int x = piece.baseX + piece.tiles[i][0];
        int y = piece.baseY + piece.tiles[i][1];
        if (inBounds(x, y)) map[x][y] = piece.id;
    }
}

void removePieceFromMap(int index) {
    struct Piece piece = pieces[index];
    for (int i = 0; i < piece.size; i++) {
        int x = piece.baseX + piece.tiles[i][0];
        int y = piece.baseY + piece.tiles[i][1];
        if (inBounds(x, y)) map[x][y] = TILE_FLOOR;
    }
}

bool canPlace(int index) {
    struct Piece piece = pieces[index];
    for (int i = 0; i < piece.size; i++) {
        int x = piece.baseX + piece.tiles[i][0];
        int y = piece.baseY + piece.tiles[i][1];
        if (!inBounds(x, y)) return false;
        if (map[x][y] == 'W') return false;
    }
    return true;
}

bool canMovePiece(int index, int dx, int dy) {
    struct Piece piece = pieces[index];
    for (int i = 0; i < piece.size; i++) {
        int newX = piece.baseX + piece.tiles[i][0] + dx;
        int newY = piece.baseY + piece.tiles[i][1] + dy;
        if (!inBounds(newX, newY)) return false;
        if (map[newX][newY] == 'W') return false;
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

    struct Piece square = {'A', 4, {{0,0},{0,1},{1,0},{1,1}}, 3, 3, true, 0};
    pieces[numPieces++] = square;

    struct Piece line = {'B', 4, {{0,0},{0,1},{0,2},{0,3}}, 6, 3, true, 0};
    pieces[numPieces++] = line;

    struct Piece lshape = {'C', 4, {{0,0},{1,0},{2,0},{2,1}}, 2, 15, true, 0};
    pieces[numPieces++] = lshape;

    struct Piece tshape = {'D', 4, {{0,1},{1,0},{1,1},{1,2}}, 8, 3, true, 0};
    pieces[numPieces++] = tshape;

    struct Piece sshape = {'E', 4, {{0,1},{0,2},{1,0},{1,1}}, 8, 15, true, 0};
    pieces[numPieces++] = sshape;

    for (int i = 0; i < numPieces; i++) {
        pieces[i].color = rand() % 7;
    }

    for (int i = 0; i < numPieces; i++) placePieceOnMap(i);
}
