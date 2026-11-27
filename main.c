#include <aurora_app.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/gb/peanut_gb.h"

#define MAX_ROMS 48
#define ROM_DIR "/roms/gb"
#define SAVE_MAGIC "NPS1"

typedef struct {
    uint8_t *rom;
    size_t rom_size;
    uint8_t *cart_ram;
    size_t cart_ram_size;
    uint8_t frame[GB_LCD_HEIGHT][GB_LCD_WIDTH];
    bool core_error;
} GbMachine;

static struct gb_s gb;
static GbMachine machine;
static char rom_names[MAX_ROMS][FS_NAME_MAX];
static int rom_count;
static int selected_rom;
static char status_line[64] = "Ready";
static bool running_game;

static const uint32_t dmg_colors[4] = {
    RGB(15, 56, 15), RGB(48, 98, 48),
    RGB(139, 172, 15), RGB(155, 188, 15)
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
    if (line >= GB_LCD_HEIGHT) return;
    for (int x = 0; x < GB_LCD_WIDTH; ++x)
        machine.frame[line][x] = pixels[x] & 3;
}

static void scan_roms(void) {
    rom_count = 0;
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
        if (n < 4) continue;
        const char *ext = entry.name + n - 4;
        if ((ext[0] == '.' || ext[0] == '.') &&
            ((ext[1] == 'g' || ext[1] == 'G') &&
             (ext[2] == 'b' || ext[2] == 'B') &&
             (ext[3] == 'c' || ext[3] == 'C' ||
              ext[3] == 'b' || ext[3] == 'B'))) {
            strncpy(rom_names[rom_count], entry.name, FS_NAME_MAX - 1);
            rom_names[rom_count][FS_NAME_MAX - 1] = 0;
            ++rom_count;
        }
    }
    fs_dir_close(dir);
    if (!rom_count) snprintf(status_line, sizeof(status_line), "Add .gb ROMs to /roms/gb");
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
    memset(&machine, 0, sizeof(machine));
    running_game = false;
    scan_roms();
}

static void draw_header(const char *subtitle) {
    gfx_gradient(SCREEN_TOP, 0, 0, SCREEN_TOP_WIDTH, 48,
                 RGB(24, 34, 55), RGB(13, 19, 34));
    gfx_print(SCREEN_TOP, 16, 8, FONT_TITLE, COLOR_WHITE, "NexPlay");
    gfx_print(SCREEN_TOP, 17, 32, FONT_SMALL, RGB(150, 190, 210), "%s", subtitle);
    gfx_rect(SCREEN_TOP, 0, 48, SCREEN_TOP_WIDTH, 1, RGB(65, 88, 116));
}

static void draw_library(void) {
    gfx_clear(SCREEN_TOP, RGB(12, 17, 29));
    draw_header("MULTISYSTEM GAME LIBRARY");
    gfx_print(SCREEN_TOP, 18, 62, FONT_BOLD, COLOR_WHITE, "Game Boy");
    gfx_print(SCREEN_TOP, 18, 82, FONT_SMALL, RGB(150, 165, 185),
              "Nintendo Game Boy (DMG)");
    gfx_round_rect(SCREEN_TOP, 16, 108, 368, 112, 9, RGB(24, 34, 52));
    gfx_print(SCREEN_TOP, 28, 120, FONT_BOLD, RGB(120, 220, 190), "ROM LIBRARY");
    if (!rom_count) {
        gfx_print(SCREEN_TOP, 28, 148, FONT_REGULAR, COLOR_WHITE, "No games found");
        gfx_print(SCREEN_TOP, 28, 172, FONT_SMALL, RGB(165, 180, 198),
                  "Place .gb or .gbc files in /roms/gb");
    } else {
        int first = selected_rom > 4 ? selected_rom - 4 : 0;
        for (int i = first; i < rom_count && i < first + 5; ++i) {
            int y = 145 + (i - first) * 14;
            if (i == selected_rom)
                gfx_round_rect(SCREEN_TOP, 24, y - 2, 350, 14, 3, RGB(48, 82, 104));
            gfx_print(SCREEN_TOP, 32, y, FONT_SMALL,
                      i == selected_rom ? COLOR_WHITE : RGB(175, 190, 205),
                      "%s", rom_names[i]);
        }
    }

    gfx_clear(SCREEN_BOTTOM, RGB(18, 25, 39));
    gfx_print(SCREEN_BOTTOM, 14, 12, FONT_BOLD, COLOR_WHITE, "LIBRARY");
    gfx_print(SCREEN_BOTTOM, 14, 34, FONT_SMALL, RGB(145, 170, 190), "%s", status_line);
    gfx_round_rect(SCREEN_BOTTOM, 12, 74, 296, 42, 7, RGB(34, 52, 72));
    gfx_print(SCREEN_BOTTOM, 25, 87, FONT_BOLD, COLOR_WHITE, "A / TAP  Open Game");
    gfx_round_rect(SCREEN_BOTTOM, 12, 124, 296, 42, 7, RGB(34, 52, 72));
    gfx_print(SCREEN_BOTTOM, 25, 137, FONT_BOLD, COLOR_WHITE, "Y  Refresh Library");
    gfx_print(SCREEN_BOTTOM, 14, 190, FONT_SMALL, RGB(135, 155, 180),
              "D-Pad: Select   B: Back");
}

static void draw_game(void) {
    gfx_clear(SCREEN_TOP, RGB(10, 15, 24));
    gfx_round_rect(SCREEN_TOP, 105, 32, 190, 176, 8, RGB(32, 43, 56));
    gfx_rect(SCREEN_TOP, 120, 48, GB_LCD_WIDTH, GB_LCD_HEIGHT, dmg_colors[3]);
    for (int y = 0; y < GB_LCD_HEIGHT; ++y)
        for (int x = 0; x < GB_LCD_WIDTH; ++x)
            gfx_pixel(SCREEN_TOP, 120 + x, 48 + y, dmg_colors[machine.frame[y][x] & 3]);
    gfx_print(SCREEN_TOP, 10, 10, FONT_SMALL, RGB(180, 200, 215), "NEXPLAY  /  GAME BOY");
    gfx_clear(SCREEN_BOTTOM, RGB(18, 25, 39));
    gfx_print(SCREEN_BOTTOM, 14, 12, FONT_BOLD, COLOR_WHITE, "GAME CONTROLS");
    gfx_print(SCREEN_BOTTOM, 14, 42, FONT_REGULAR, RGB(180, 205, 220),
              "D-Pad: Move");
    gfx_print(SCREEN_BOTTOM, 14, 66, FONT_REGULAR, RGB(180, 205, 220),
              "A: Game Boy A    B: Game Boy B");
    gfx_print(SCREEN_BOTTOM, 14, 90, FONT_REGULAR, RGB(180, 205, 220),
              "X: Start    Y: Select");
    gfx_round_rect(SCREEN_BOTTOM, 12, 145, 296, 44, 7, RGB(42, 56, 75));
    gfx_print(SCREEN_BOTTOM, 24, 160, FONT_BOLD, COLOR_WHITE, "B: Return to Library");
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
    gb.direct.joypad_bits.select= (held & KEY_Y) != 0;
}

int main(void) {
    scan_roms();
    while (app_loop()) {
        uint32_t down = hid_keys_down();
        if (running_game) {
            if (down & KEY_B) {
                close_game();
                continue;
            }
            update_game_input();
            gb_run_frame(&gb);
            if (machine.core_error) {
                close_game();
                snprintf(status_line, sizeof(status_line), "Game stopped: core error");
            }
            draw_game();
        } else {
            if (down & KEY_UP && selected_rom > 0) --selected_rom;
            if (down & KEY_DOWN && selected_rom + 1 < rom_count) ++selected_rom;
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
