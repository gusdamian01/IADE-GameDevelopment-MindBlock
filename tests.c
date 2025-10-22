#include <stdio.h>
#include <stdbool.h>

// ====================== CONFIGURATIONS ======================
#define MAP_ROWS 12
#define MAP_COLS 20
#define MAX_PIECES 5

// Tile codes
#define TILE_WALL 'W'
#define TILE_FLOOR 'F'

// ====================== STRUCTURES ======================
struct Player {
    int level;
    int attack;
    int position_x;
    int position_y;
    int defense;
    bool holdingPiece;
    char heldPieceId;
};

struct Piece {
    char id;              // e.g. 'A', 'B', etc.
    int size;             // number of tiles (4)
    int tiles[4][2];      // relative coordinates
    bool placed;
    int baseX;
    int baseY;
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
    {'W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W','W'}
};

struct Player player = {1, 5, 5, 3, 3, false, '\0'};
struct Piece pieces[MAX_PIECES];
int numPieces = 0;

// ====================== FUNCTION DECLARATIONS ======================
void printMap(void);
void printPlayerStats(void);
char readUserInput(void);
void movePlayer(char dir);
bool isTileWalkable(char t);
bool inBounds(int y, int x);

void placePieceOnMap(struct Piece *p, int baseX, int baseY);
void removePieceFromMap(struct Piece *p);
void initPieces(void);
void interact(void);
void rotatePiece(struct Piece *p);
bool canMovePiece(struct Piece *p, int dx, int dy);
bool canPlace(struct Piece *p);

// ====================== MAIN ======================
int main(void) {
    printf("Hello Joe! 😁\n\n");

    initPieces();

    while (1) {
        printPlayerStats();    
        printMap();

        printf("\nUse W/A/S/D to move, E or Q to pick up/place, R to rotate.\n> ");
        char input = readUserInput();

        if (input == 'E' || input == 'e') interact();
        else if (input == 'Q' || input == 'q') interact();
        else if (input == 'R' || input == 'r') {
            if (player.holdingPiece) {
                for (int i = 0; i < numPieces; i++) {
                    if (pieces[i].id == player.heldPieceId) {
                        struct Piece *p = &pieces[i];

                        // Rotation safety check
                        removePieceFromMap(p);

                        int backup[4][2];
                        for (int j = 0; j < 4; j++) {
                            backup[j][0] = p->tiles[j][0];
                            backup[j][1] = p->tiles[j][1];
                        }

                        rotatePiece(p);
                        if (!canPlace(p)) {
                            // Undo rotation if invalid
                            for (int j = 0; j < 4; j++) {
                                p->tiles[j][0] = backup[j][0];
                                p->tiles[j][1] = backup[j][1];
                            }
                        }
                        placePieceOnMap(p, p->baseX, p->baseY);
                    }
                }
            }
        }
        else if (player.holdingPiece) {
            for (int i = 0; i < numPieces; i++) {
                if (pieces[i].id == player.heldPieceId) {
                    struct Piece *p = &pieces[i];
                    int dx = 0, dy = 0;
                    if (input == 'W' || input == 'w') dx = -1;
                    else if (input == 'S' || input == 's') dx = 1;
                    else if (input == 'A' || input == 'a') dy = -1;
                    else if (input == 'D' || input == 'd') dy = 1;

                    if (canMovePiece(p, dx, dy)) {
                        removePieceFromMap(p);
                        p->baseX += dx;
                        p->baseY += dy;
                        placePieceOnMap(p, p->baseX, p->baseY);
                    }
                }
            }
        }
        else movePlayer(input);

        printf("\n\n");
    }

    return 0;
}

// ====================== IMPLEMENTATIONS ======================

void printPlayerStats() {
    printf("📍 Position: [%d,%d]\n", player.position_x, player.position_y);
    if (player.holdingPiece)
        printf("🧩 Controlling piece: %c\n", player.heldPieceId);
    else
        printf("😁 Joe ready to move.\n");
}

void printMap(void) {
    for (int x = 0; x < MAP_ROWS; x++) {
        for (int y = 0; y < MAP_COLS; y++) {
            if (!player.holdingPiece && x == player.position_x && y == player.position_y)
                printf("😁");
            else if (map[x][y] == 'W')
                printf("🟥");
            else if (map[x][y] == 'F')
                printf("⬜");
            else if (map[x][y] >= 'A' && map[x][y] <= 'Z') {
                if (player.holdingPiece && map[x][y] == player.heldPieceId)
                    printf("🟦"); // active piece
                else
                    printf("🔷");
            } else
                printf("%c", map[x][y]);
        }
        printf("\n");
    }
}

char readUserInput(void) {
    char input;
    scanf(" %c", &input); // space before %c skips whitespace
    return input;
}

bool isTileWalkable(char t) {
    if (t == TILE_FLOOR) return true;
    // allow wizard to step on a piece tile if not controlling a piece
    if (!player.holdingPiece && t >= 'A' && t <= 'Z') return true;
    return false;
}

bool inBounds(int y, int x) {
    return (y >= 0 && y < MAP_ROWS && x >= 0 && x < MAP_COLS);
}

void movePlayer(char dir) {
    int newX = player.position_x;
    int newY = player.position_y;

    switch (dir) {
        case 'A': case 'a': newY--; break;
        case 'D': case 'd': newY++; break;
        case 'W': case 'w': newX--; break;
        case 'S': case 's': newX++; break;
        default: return;
    }

    if (inBounds(newX, newY) && isTileWalkable(map[newX][newY])) {
        player.position_x = newX;
        player.position_y = newY;
    }
}

// ====================== PIECE SYSTEM ======================

void placePieceOnMap(struct Piece *p, int baseX, int baseY) {
    for (int i = 0; i < p->size; i++) {
        int x = baseX + p->tiles[i][0];
        int y = baseY + p->tiles[i][1];
        if (inBounds(x, y))
            map[x][y] = p->id;
    }
    p->baseX = baseX;
    p->baseY = baseY;
    p->placed = true;
}

void removePieceFromMap(struct Piece *p) {
    for (int i = 0; i < p->size; i++) {
        int x = p->baseX + p->tiles[i][0];
        int y = p->baseY + p->tiles[i][1];
        if (inBounds(x, y) && map[x][y] == p->id)
            map[x][y] = TILE_FLOOR;
    }
}

bool canMovePiece(struct Piece *p, int dx, int dy) {
    for (int i = 0; i < p->size; i++) {
        int newX = p->baseX + p->tiles[i][0] + dx;
        int newY = p->baseY + p->tiles[i][1] + dy;

        if (!inBounds(newX, newY)) return false;

        char tile = map[newX][newY];
        bool isOwnTile = false;
        for (int j = 0; j < p->size; j++) {
            int oldX = p->baseX + p->tiles[j][0];
            int oldY = p->baseY + p->tiles[j][1];
            if (oldX == newX && oldY == newY) { isOwnTile = true; break; }
        }
        if (!isOwnTile && tile != TILE_FLOOR) return false;
    }
    return true;
}

bool canPlace(struct Piece *p) {
    for (int i = 0; i < p->size; i++) {
        int x = p->baseX + p->tiles[i][0];
        int y = p->baseY + p->tiles[i][1];
        if (!inBounds(x, y)) return false;
        char tile = map[x][y];
        bool isOwnTile = false;
        for (int j = 0; j < p->size; j++) {
            int oldX = p->baseX + p->tiles[j][0];
            int oldY = p->baseY + p->tiles[j][1];
            if (oldX == x && oldY == y) { isOwnTile = true; break; }
        }
        if (!isOwnTile && tile != TILE_FLOOR) return false;
    }
    return true;
}

void rotatePiece(struct Piece *p) {
    for (int i = 0; i < p->size; i++) {
        int temp = p->tiles[i][0];
        p->tiles[i][0] = p->tiles[i][1];
        p->tiles[i][1] = -temp;
    }
}

void initPieces(void) {
    // Square (A)
    struct Piece square = {'A', 4, {{0,0},{0,1},{1,0},{1,1}}, true, 3, 3};
    pieces[numPieces++] = square;
    placePieceOnMap(&pieces[0], 2, 2);

    // Line (B)
    struct Piece line = {'B', 4, {{0,0},{1,0},{2,0},{3,0}}, true, 6, 3};
    pieces[numPieces++] = line;
    placePieceOnMap(&pieces[1], 2, 6);

    // T-shape (C)
    struct Piece tee = {'C', 4, {{0,0},{0,1},{0,2},{1,1}}, true, 2, 15};
    pieces[numPieces++] = tee;
    placePieceOnMap(&pieces[2], 6, 3);

    // L-shape (D)
    struct Piece ell = {'D', 4, {{0,0},{1,0},{2,0},{2,1}}, true, 8, 3};
    pieces[numPieces++] = ell;
    placePieceOnMap(&pieces[3], 7, 8);

    // Z-shape (E)
    struct Piece zee = {'E', 4, {{0,0},{0,1},{1,1},{1,2}}, true, 8, 5};
    pieces[numPieces++] = zee;
    placePieceOnMap(&pieces[4], 4, 5);
}

void interact(void) {
    if (!player.holdingPiece) {
        char tile = map[player.position_x][player.position_y];
        if (tile >= 'A' && tile <= 'Z') {
            player.holdingPiece = true;
            player.heldPieceId = tile;
            printf("You are now controlling piece %c!\n", tile);
        }
    } else {
        player.holdingPiece = false;
        player.heldPieceId = '\0';
        printf("You return to wizard form.\n");
    }
}
