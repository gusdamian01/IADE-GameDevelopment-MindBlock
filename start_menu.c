// main_menu.c

#include <stdio.h>
#include <stdlib.h>

// Displays the start menu
static void display_start_menu(void) {
    printf("=== Welcome to Mindblock! ===\n");
    printf("1. Play\n");
    printf("2. Settings\n");
    printf("3. Quit\n");
}

// Gets the user's menu choice (1–3)
int get_menu_choice(void) {
    int option;
    char c; 
    while (1) {
        display_start_menu();
        printf("Option (1-3): ");

        if (scanf("%d", &option) != 1) {
            while ((c = getchar()) != '\n' && c != EOF);
            printf("Invalid input. Please enter a number between 1 and 3.\n\n");
            continue;
        }

        if (option >= 1 && option <= 3) {
            return option;
        } else {
            printf("Invalid choice. Please enter a number between 1 and 3.\n\n");
        }
    }
}
