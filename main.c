#include <aurora_app.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/gb/peanut_gb.h"

#define MAX_ROMS 48
#define ROM_DIR "/NexPlay/roms/gb"
#define SAVE_MAGIC "NPS1"

typedef struct {
    uint8_t *rom;
    size_t rom_size;
    uint8_t *cart_ram;
    size_t cart_ram_size;
    bool core_error;
} GbMachine;

static struct gb_s gb;
static GbMachine machine;
static char rom_names[MAX_ROMS][FS_NAME_MAX];
static int rom_count;
static int selected_rom;
static char status_line[64] = "Ready";
static bool running_game;
static GfxImage *gb_image;
typedef enum { VIEW_CONSOLES, VIEW_GB_LIBRARY } AppView;
static AppView app_view = VIEW_CONSOLES;
static int selected_console;
static uint32_t folder_notice_until;

/* Original monochrome Game Boy LCD shades; app theme is independent. */
static const uint32_t dmg_colors[4] = {
    RGB(15, 56, 15), RGB(48, 98, 48),
    RGB(139, 172, 15), RGB(155, 188, 15)
};

#define CONSOLE_COUNT 8
static const char *console_names[CONSOLE_COUNT] = {
    "GAME BOY", "GAME BOY COLOR", "GAME BOY ADVANCE", "NINTENDO NES",
    "SUPER NINTENDO", "SEGA MEGA DRIVE", "SEGA MASTER SYSTEM", "PLAYSTATION"
};
static const char *console_short[CONSOLE_COUNT] = {
    "GB", "GBC", "GBA", "NES", "SNES", "MD", "SMS", "PS1"
};

static uint8_t gb_rom_read(struct gb_s *ctx, const uint_fast32_t address) {
    GbMachine *m = (GbMachine *)ctx->direct.priv;
    if (!m || !m->rom || address >= m->rom_size) return 0xFF;
    return m->rom[address];
}

static uint8_t gb_cart_ram_read(struct gb_s *ctx, const uint_fast32_t address) {
    GbMachine *m = (GbMachine *)ctx->direct.priv;
    if (!m || !m->cart_ram || address >= m->cart_ram_size) return 0xFF;
    return m->cart_ram[address];
}

static void gb_cart_ram_write(struct gb_s *ctx, const uint_fast32_t address,
                              const uint8_t value) {
    GbMachine *m = (GbMachine *)ctx->direct.priv;
    if (m && m->cart_ram && address < m->cart_ram_size)
        m->cart_ram[address] = value;
}

static void gb_error(struct gb_s *ctx, const enum gb_error_e error,
                     const uint16_t address) {
    (void)ctx; (void)error; (void)address;
    machine.core_error = true;
}

static void gb_draw_line(struct gb_s *ctx, const uint8_t *pixels,
                         const uint_fast8_t line) {
    (void)ctx;
    if (line >= LCD_HEIGHT) return;
    if (!gb_image) return;
    for (int x = 0; x < LCD_WIDTH; ++x) {
        uint32_t color = dmg_colors[pixels[x] & 3];
        gb_image->pixels[line * LCD_WIDTH + x] = 0xFF000000u | color;
    }
}

static void scan_roms(void) {
    rom_count = 0;
    fs_mkdir("/NexPlay");
    fs_mkdir("/NexPlay/roms");
    fs_mkdir(ROM_DIR);
    FsDir *dir = fs_dir_open(ROM_DIR);
    if (!dir) {
        snprintf(status_line, sizeof(status_line), "ROM folder unavailable");
        return;
    }
    FsEntry entry;
    while (rom_count < MAX_ROMS && fs_dir_read(dir, &entry)) {
        if (entry.is_dir) continue;
        size_t n = strlen(entry.name);
        bool is_gb = n >= 3 && entry.name[n - 3] == '.' &&
                     (entry.name[n - 2] == 'g' || entry.name[n - 2] == 'G') &&
                     (entry.name[n - 1] == 'b' || entry.name[n - 1] == 'B');
        if (is_gb) {
            strncpy(rom_names[rom_count], entry.name, FS_NAME_MAX - 1);
            rom_names[rom_count][FS_NAME_MAX - 1] = 0;
            ++rom_count;
        }
    }
    fs_dir_close(dir);
    if (!rom_count) snprintf(status_line, sizeof(status_line), "Add .gb ROMs to /NexPlay/roms/gb");
    else snprintf(status_line, sizeof(status_line), "%d Game Boy ROM(s) found", rom_count);
}

static bool load_game(const char *name) {
    char path[320];
    snprintf(path, sizeof(path), "%s/%s", ROM_DIR, name);
    size_t size = 0;
    uint8_t *rom = (uint8_t *)fs_read_file(path, &size);
    if (!rom || size < 0x150) {
        if (rom) free(rom);
        snprintf(status_line, sizeof(status_line), "Invalid or unreadable ROM");
        return false;
    }

    memset(&machine, 0, sizeof(machine));
    if (gb_image) gfx_image_free(gb_image);
    gb_image = gfx_image_new(LCD_WIDTH, LCD_HEIGHT);
    if (!gb_image) {
        free(rom);
        snprintf(status_line, sizeof(status_line), "Not enough memory for display");
        return false;
    }
    machine.rom = rom;
    machine.rom_size = size;
    if (gb_init(&gb, gb_rom_read, gb_cart_ram_read, gb_cart_ram_write,
                gb_error, &machine) != GB_INIT_NO_ERROR) {
        free(rom);
        machine.rom = NULL;
        snprintf(status_line, sizeof(status_line), "Unsupported ROM header");
        return false;
    }

    size_t ram_size = 0;
    if (gb_get_save_size_s(&gb, &ram_size) == 0 && ram_size) {
        machine.cart_ram = (uint8_t *)calloc(1, ram_size);
        machine.cart_ram_size = machine.cart_ram ? ram_size : 0;
    }
    gb_init_lcd(&gb, gb_draw_line);
    running_game = true;
    snprintf(status_line, sizeof(status_line), "%s", name);
    return true;
}

static void save_game(void) {
    if (!machine.cart_ram || !machine.cart_ram_size) return;
    char path[320];
    const char *name = rom_names[selected_rom];
    size_t n = strlen(name);
    snprintf(path, sizeof(path), "%s/%.*s.sav", ROM_DIR, (int)(n - 3), name);
    fs_write_file(path, machine.cart_ram, machine.cart_ram_size);
}

static void close_game(void) {
    save_game();
    if (machine.rom) free(machine.rom);
    if (machine.cart_ram) free(machine.cart_ram);
    if (gb_image) gfx_image_free(gb_image);
    gb_image = NULL;
    memset(&machine, 0, sizeof(machine));
    running_game = false;
    scan_roms();
}

static void draw_header(const char *subtitle) {
    gfx_gradient(SCREEN_TOP, 0, 0, SCREEN_TOP_WIDTH, 48,
                 RGB(25, 17, 39), RGB(9, 10, 17));
    gfx_print(SCREEN_TOP, 16, 8, FONT_TITLE, COLOR_WHITE, "NexPlay");
    gfx_print(SCREEN_TOP, 17, 32, FONT_SMALL, RGB(92, 225, 220), "%s", subtitle);
    gfx_rect(SCREEN_TOP, 0, 48, SCREEN_TOP_WIDTH, 1, RGB(126, 76, 181));
}

static void draw_menu(void) {
    static const int card_x[2] = {16, 206};
    static const int card_y[3] = {82, 132, 182};
    gfx_clear(SCREEN_TOP, RGB(9, 10, 17));
    gfx_gradient(SCREEN_TOP, 0, 0, SCREEN_TOP_WIDTH, 61,
                 RGB(34, 22, 49), RGB(10, 11, 19));
    gfx_print(SCREEN_TOP, 18, 8, FONT_TITLE, COLOR_WHITE, "NEXPLAY");
    gfx_print(SCREEN_TOP, 20, 34, FONT_SMALL, RGB(92, 225, 220),
              "FUTURE-READY EMULATION HUB");
    gfx_rect(SCREEN_TOP, 16, 62, 368, 1, RGB(126, 76, 181));
    gfx_print(SCREEN_TOP, 20, 67, FONT_SMALL, RGB(190, 178, 210),
              "SELECT A PLATFORM");

    for (int i = 0; i < CONSOLE_COUNT; ++i) {
        int row = i / 2, col = i % 2;
        int x = card_x[col], y = card_y[row];
        bool selected = (i == selected_console);
        bool ready = (i == 0);
        gfx_round_rect(SCREEN_TOP, x, y, 178, 43, 7,
                       selected ? RGB(48, 32, 65) : RGB(23, 23, 33));
        gfx_rect_outline(SCREEN_TOP, x, y, 178, 43, 2,
                         selected ? RGB(92, 225, 220) : RGB(66, 49, 83));
        gfx_round_rect(SCREEN_TOP, x + 7, y + 7, 38, 29, 5,
                       ready ? RGB(20, 104, 100) : RGB(63, 45, 89));
        gfx_print(SCREEN_TOP, x + 11, y + 15, FONT_SMALL, COLOR_WHITE,
                  "%s", console_short[i]);
        gfx_print(SCREEN_TOP, x + 52, y + 7, FONT_SMALL, COLOR_WHITE,
                  "%s", console_names[i]);
        gfx_print(SCREEN_TOP, x + 52, y + 25, FONT_MONO,
                  ready ? RGB(92, 225, 170) : RGB(181, 151, 207),
                  "%s", ready ? "READY" : "COMING SOON");
    }

    gfx_clear(SCREEN_BOTTOM, RGB(12, 12, 21));
    gfx_gradient(SCREEN_BOTTOM, 0, 0, SCREEN_BOTTOM_WIDTH, 54,
                 RGB(32, 21, 46), RGB(12, 12, 21));
    gfx_print(SCREEN_BOTTOM, 14, 10, FONT_BOLD, COLOR_WHITE, "PLATFORM SELECT");
    gfx_print(SCREEN_BOTTOM, 14, 34, FONT_SMALL, RGB(92, 225, 220),
              "A / TAP  Open selected platform");
    gfx_round_rect(SCREEN_BOTTOM, 12, 86, 296, 45, 8, RGB(35, 26, 48));
    gfx_rect_outline(SCREEN_BOTTOM, 12, 86, 296, 45, 1, RGB(126, 76, 181));
    gfx_print(SCREEN_BOTTOM, 24, 101, FONT_BOLD, COLOR_WHITE,
              "D-PAD / CIRCLE PAD  Navigate");
    gfx_print(SCREEN_BOTTOM, 14, 158, FONT_SMALL, RGB(175, 165, 190),
              "Game Boy is currently playable.");
    gfx_print(SCREEN_BOTTOM, 14, 180, FONT_SMALL, RGB(175, 165, 190),
              "Other platforms are planned.");
}

static void draw_library(void) {
    gfx_clear(SCREEN_TOP, RGB(9, 10, 17));
    draw_header("GAME BOY  /  GAME LIBRARY");
    gfx_print(SCREEN_TOP, 18, 62, FONT_BOLD, COLOR_WHITE, "GAME BOY");
    gfx_print(SCREEN_TOP, 18, 82, FONT_SMALL, RGB(105, 205, 225),
              "NINTENDO DMG  |  READY");
    gfx_round_rect(SCREEN_TOP, 16, 108, 368, 112, 9, RGB(23, 23, 33));
    gfx_rect_outline(SCREEN_TOP, 16, 108, 368, 112, 1, RGB(66, 49, 83));
    gfx_print(SCREEN_TOP, 28, 120, FONT_BOLD, RGB(91, 211, 230), "ROM LIBRARY");
    if (!rom_count) {
        gfx_print(SCREEN_TOP, 28, 148, FONT_REGULAR, COLOR_WHITE, "No games found");
        gfx_print(SCREEN_TOP, 28, 172, FONT_SMALL, RGB(165, 190, 210),
                  "Add .gb files to /NexPlay/roms/gb");
    } else {
        int first = selected_rom > 4 ? selected_rom - 4 : 0;
        for (int i = first; i < rom_count && i < first + 5; ++i) {
            int y = 145 + (i - first) * 14;
            if (i == selected_rom)
                gfx_round_rect(SCREEN_TOP, 24, y - 2, 350, 14, 3, RGB(48, 32, 65));
            gfx_print(SCREEN_TOP, 32, y, FONT_SMALL,
                      i == selected_rom ? COLOR_WHITE : RGB(175, 198, 215),
                      "%s", rom_names[i]);
        }
    }

    gfx_clear(SCREEN_BOTTOM, RGB(12, 12, 21));
    gfx_gradient(SCREEN_BOTTOM, 0, 0, SCREEN_BOTTOM_WIDTH, 48,
                 RGB(32, 21, 46), RGB(12, 12, 21));
    gfx_print(SCREEN_BOTTOM, 14, 12, FONT_BOLD, COLOR_WHITE, "GAME LIBRARY");
    gfx_print(SCREEN_BOTTOM, 14, 34, FONT_SMALL, RGB(115, 206, 225), "%s", status_line);
    gfx_round_rect(SCREEN_BOTTOM, 12, 74, 296, 42, 7, RGB(35, 26, 48));
    gfx_print(SCREEN_BOTTOM, 25, 87, FONT_BOLD, COLOR_WHITE, "A / TAP  Open Game");
    gfx_round_rect(SCREEN_BOTTOM, 12, 124, 296, 42, 7, RGB(19, 52, 72));
    gfx_print(SCREEN_BOTTOM, 25, 137, FONT_BOLD, COLOR_WHITE, "Y  Refresh Library");
    gfx_print(SCREEN_BOTTOM, 14, 190, FONT_SMALL, RGB(149, 176, 198),
              "D-Pad / Circle Pad: Select   B: Back");
    if ((int32_t)(folder_notice_until - sys_millis()) > 0) {
        gfx_round_rect(SCREEN_BOTTOM, 8, 211, 304, 24, 5, RGB(22, 81, 103));
        gfx_print(SCREEN_BOTTOM, 13, 217, FONT_SMALL, COLOR_WHITE,
                  "ROM FOLDER: /NexPlay/roms/gb/");
    }
}

static void draw_game(void) {
    gfx_clear(SCREEN_TOP, RGB(9, 10, 17));
    gfx_round_rect(SCREEN_TOP, 105, 32, 190, 176, 8, RGB(23, 23, 33));
    gfx_rect_outline(SCREEN_TOP, 105, 32, 190, 176, 2, RGB(126, 76, 181));
    if (gb_image)
        gfx_image_scaled(SCREEN_TOP, gb_image, 120, 48, LCD_WIDTH, LCD_HEIGHT);
    gfx_print(SCREEN_TOP, 10, 10, FONT_SMALL, RGB(180, 200, 215), "NEXPLAY  /  GAME BOY");
    gfx_clear(SCREEN_BOTTOM, RGB(8, 16, 30));
    gfx_print(SCREEN_BOTTOM, 14, 12, FONT_BOLD, COLOR_WHITE, "GAME CONTROLS");
    gfx_print(SCREEN_BOTTOM, 14, 42, FONT_REGULAR, RGB(180, 205, 220),
              "D-Pad: Move");
    gfx_print(SCREEN_BOTTOM, 14, 66, FONT_REGULAR, RGB(180, 205, 220),
              "A: Game Boy A    B: Game Boy B");
    gfx_print(SCREEN_BOTTOM, 14, 90, FONT_REGULAR, RGB(180, 205, 220),
              "X: Start    SELECT: Select");
    gfx_round_rect(SCREEN_BOTTOM, 12, 145, 296, 44, 7, RGB(42, 56, 75));
    gfx_print(SCREEN_BOTTOM, 24, 160, FONT_BOLD, COLOR_WHITE, "X + Y: Return to Library");
}

static void update_game_input(void) {
    uint32_t held = hid_keys_held();
    gb.direct.joypad_bits.right = (held & KEY_RIGHT) != 0;
    gb.direct.joypad_bits.left  = (held & KEY_LEFT) != 0;
    gb.direct.joypad_bits.up    = (held & KEY_UP) != 0;
    gb.direct.joypad_bits.down  = (held & KEY_DOWN) != 0;
    gb.direct.joypad_bits.a     = (held & KEY_A) != 0;
    gb.direct.joypad_bits.b     = (held & KEY_B) != 0;
    gb.direct.joypad_bits.start = (held & KEY_X) != 0;
    gb.direct.joypad_bits.select= (held & KEY_SELECT) != 0;
}

int main(void) {
    scan_roms();
    while (app_loop()) {
        uint32_t down = hid_keys_down();
        if (running_game) {
            if ((down & KEY_X) && (hid_keys_held() & KEY_Y)) {
                close_game();
                app_view = VIEW_GB_LIBRARY;
                continue;
            }
            update_game_input();
            gb_run_frame(&gb);
            if (machine.core_error) {
                close_game();
                app_view = VIEW_GB_LIBRARY;
                snprintf(status_line, sizeof(status_line), "Game stopped: core error");
            }
            draw_game();
        } else if (app_view == VIEW_CONSOLES) {
            if ((down & (KEY_UP | KEY_CPAD_UP)) && selected_console >= 2)
                selected_console -= 2;
            if ((down & (KEY_DOWN | KEY_CPAD_DOWN)) && selected_console + 2 < CONSOLE_COUNT)
                selected_console += 2;
            if ((down & (KEY_LEFT | KEY_CPAD_LEFT)) && (selected_console % 2) == 1)
                --selected_console;
            if ((down & (KEY_RIGHT | KEY_CPAD_RIGHT)) &&
                (selected_console % 2) == 0 && selected_console + 1 < CONSOLE_COUNT)
                ++selected_console;
            if ((down & KEY_A) && selected_console == 0) {
                app_view = VIEW_GB_LIBRARY;
                selected_rom = 0;
                scan_roms();
                folder_notice_until = sys_millis() + 8000;
            } else if (down & KEY_A) {
                snprintf(status_line, sizeof(status_line), "%s is coming soon",
                         console_names[selected_console]);
            }
            if (hid_touch_down()) {
                int tx = hid_touch_x(), ty = hid_touch_y();
                if (ty >= 82 && ty <= 225) {
                    int row = (ty - 82) / 50;
                    int col = tx < 200 ? 0 : 1;
                    int choice = row * 2 + col;
                    if (choice >= 0 && choice < CONSOLE_COUNT) {
                        selected_console = choice;
                        if (choice == 0) {
                            app_view = VIEW_GB_LIBRARY;
                            selected_rom = 0;
                            scan_roms();
                            folder_notice_until = sys_millis() + 8000;
                        } else {
                            snprintf(status_line, sizeof(status_line), "%s is coming soon",
                                     console_names[choice]);
                        }
                    }
                }
            }
            draw_menu();
        } else {
            if (down & KEY_B) {
                app_view = VIEW_CONSOLES;
                selected_rom = 0;
            }
            if ((down & (KEY_UP | KEY_CPAD_UP)) && selected_rom > 0) --selected_rom;
            if ((down & (KEY_DOWN | KEY_CPAD_DOWN)) && selected_rom + 1 < rom_count) ++selected_rom;
            if (down & KEY_Y) scan_roms();
            if ((down & KEY_A) && rom_count > 0) load_game(rom_names[selected_rom]);
            if (hid_touch_down()) {
                int ty = hid_touch_y();
                if (ty >= 74 && ty <= 116 && rom_count > 0)
                    load_game(rom_names[selected_rom]);
                else if (ty >= 124 && ty <= 166)
                    scan_roms();
            }
            draw_library();
        }
    }
    if (running_game) close_game();
    return 0;
}
