#ifndef VIEW_H
#define VIEW_H
#include <SDL2/SDL.h>
#include <stdbool.h>

/* Expose SDL globals for direct use in controller.c if needed */
extern SDL_Window* window;
extern SDL_Renderer* renderer;
extern int window_width, window_height;

/* Expose color constants */
extern const SDL_Color BLACK;
extern const SDL_Color RED;
extern const SDL_Color BLUE;
extern const SDL_Color GREEN;
extern const SDL_Color WHITE;
extern const SDL_Color LIGHT_GRAY;
extern const SDL_Color DARK_GRAY;

/* view.c functions */
extern int init_sdl_window();
extern void draw_cell(int x, int y, SDL_Color color);
extern void draw_drones();
extern void draw_survivors();
extern void draw_grid();
extern int draw_map();
extern int check_events();
extern void quit_all();
extern void draw_test_pattern(); 
extern void draw_diagnostic();
extern void update_window_title();
extern void draw_info_panel();

/* TTF text rendering functions */
extern void render_text(const char* text, int x, int y, SDL_Color color, bool use_bold);
extern void render_text_line(const char* text, int y, SDL_Color color, int value);

#endif