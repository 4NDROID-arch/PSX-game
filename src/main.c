/*
 * VIZO TOP DOWN SHOOTER - PS1 homebrew prototype
 * Target: original PlayStation / PSn00bSDK
 * Controls:
 *   D-Pad       Move
 *   Cross       Fire in the last facing direction
 *   Start       Pause / resume
 *
 * This is a playable first milestone: isometric-style ground, player,
 * approaching enemies, projectiles, health, kills and endless waves.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psxgpu.h>
#include <psxpad.h>
#include <psxetc.h>
#include <psxapi.h>

#define SCREEN_W 320
#define SCREEN_H 240
#define OT_LEN  8
#define PACKET_LEN 8192
#define ENEMY_MAX 8
#define BULLET_MAX 12

typedef struct {
    DISPENV disp;
    DRAWENV draw;
    u_long ot[OT_LEN];
    u_char packets[PACKET_LEN];
} DB;

typedef struct {
    int x, y;
    int active;
    int hp;
    int speed;
} Enemy;

typedef struct {
    int x, y;
    int vx, vy;
    int active;
} Bullet;

static DB db[2];
static int db_active = 0;
static char *next_prim;
static char pad_buffer[2][34];

static int player_x = 160;
static int player_y = 128;
static int player_hp = 100;
static int facing_x = 0;
static int facing_y = -1;
static int kills = 0;
static int wave_kills = 0;
static int wave = 1;
static int wave_spawned = 0;
static int wave_total = 4;
static int fire_cooldown = 0;
static int paused = 0;
static int start_was_pressed = 0;
static unsigned int rng_state = 0x13579BDF;

static Enemy enemies[ENEMY_MAX];
static Bullet bullets[BULLET_MAX];

static int random_range(int max_value) {
    rng_state = rng_state * 1664525u + 1013904223u;
    if (max_value <= 0) return 0;
    return (int)((rng_state >> 16) % (unsigned int)max_value);
}

static void graphics_init(void) {
    ResetGraph(0);

    SetDefDispEnv(&db[0].disp, 0, 0, SCREEN_W, SCREEN_H);
    SetDefDrawEnv(&db[0].draw, SCREEN_W, 0, SCREEN_W, SCREEN_H);
    setRGB0(&db[0].draw, 10, 18, 25);
    db[0].draw.isbg = 1;
    db[0].draw.dtd = 1;

    SetDefDispEnv(&db[1].disp, SCREEN_W, 0, SCREEN_W, SCREEN_H);
    SetDefDrawEnv(&db[1].draw, 0, 0, SCREEN_W, SCREEN_H);
    setRGB0(&db[1].draw, 10, 18, 25);
    db[1].draw.isbg = 1;
    db[1].draw.dtd = 1;

    ClearOTagR(db[0].ot, OT_LEN);
    ClearOTagR(db[1].ot, OT_LEN);
    next_prim = db[0].packets;

    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);
    SetDispMask(1);

    InitPAD(&pad_buffer[0][0], 34, &pad_buffer[1][0], 34);
    StartPAD();
    ChangeClearPAD(0);
}

static void draw_quad(int x0, int y0, int x1, int y1,
                      int x2, int y2, int x3, int y3,
                      int r, int g, int b) {
    POLY_F4 *p = (POLY_F4 *)next_prim;
    next_prim += sizeof(POLY_F4);

    setPolyF4(p);
    p->x0 = x0; p->y0 = y0;
    p->x1 = x1; p->y1 = y1;
    p->x2 = x2; p->y2 = y2;
    p->x3 = x3; p->y3 = y3;
    setRGB0(p, r, g, b);
    addPrim(db[db_active].ot, p);
}

static void draw_rect(int x, int y, int w, int h, int r, int g, int b) {
    draw_quad(x, y, x + w, y, x, y + h, x + w, y + h, r, g, b);
}

static void draw_diamond(int cx, int cy, int rx, int ry,
                         int r, int g, int b) {
    draw_quad(cx, cy - ry, cx + rx, cy, cx, cy + ry, cx - rx, cy,
              r, g, b);
}

static void draw_ground(void) {
    int x, y;
    for (y = 42; y < 220; y += 24) {
        for (x = -8; x < 336; x += 32) {
            int shade = (((x / 32) + (y / 24)) & 1) ? 24 : 29;
            draw_diamond(x + ((y / 24) & 1) * 16, y, 16, 11,
                         shade, shade + 5, shade + 7);
        }
    }

    /* Concrete barricades and sandbag-like cover. */
    draw_rect(36, 65, 42, 8, 74, 78, 72);
    draw_rect(39, 59, 36, 7, 103, 101, 82);
    draw_rect(246, 170, 38, 8, 74, 78, 72);
    draw_rect(250, 164, 30, 7, 103, 101, 82);
}

static void draw_soldier(int x, int y, int player) {
    if (player) {
        draw_diamond(x, y + 5, 9, 5, 5, 10, 12);
        draw_rect(x - 5, y - 8, 10, 13, 40, 104, 58);
        draw_rect(x - 4, y - 12, 8, 6, 159, 143, 91);
        draw_rect(x - 2, y - 10, 4, 2, 44, 54, 39);
        draw_rect(x + facing_x * 5 - 2, y + facing_y * 5 - 2,
                  5, 3, 38, 38, 34);
    } else {
        draw_diamond(x, y + 4, 8, 4, 8, 8, 8);
        draw_rect(x - 5, y - 7, 10, 12, 128, 45, 37);
        draw_rect(x - 4, y - 11, 8, 5, 177, 112, 77);
        draw_rect(x - 1, y - 9, 3, 2, 30, 25, 22);
    }
}

static void draw_hud(void) {
    int i;
    draw_rect(8, 8, 104, 8, 35, 35, 35);
    draw_rect(9, 9, player_hp, 6, 40, 190, 70);

    draw_rect(224, 8, 88, 8, 35, 35, 35);
    for (i = 0; i < wave && i < 10; i++) {
        draw_rect(226 + i * 8, 10, 5, 4, 220, 170, 55);
    }

    /* Small kill counter made from simple blocks to avoid font assets. */
    draw_rect(8, 21, (kills % 30) * 3, 3, 80, 180, 220);
}

static void spawn_enemy(void) {
    int i;
    for (i = 0; i < ENEMY_MAX; i++) {
        if (!enemies[i].active) {
            int side = random_range(4);
            if (side == 0) { enemies[i].x = 12; enemies[i].y = 42 + random_range(150); }
            else if (side == 1) { enemies[i].x = 308; enemies[i].y = 42 + random_range(150); }
            else if (side == 2) { enemies[i].x = 24 + random_range(272); enemies[i].y = 40; }
            else { enemies[i].x = 24 + random_range(272); enemies[i].y = 214; }
            enemies[i].active = 1;
            enemies[i].hp = 2 + (wave > 4);
            enemies[i].speed = 1 + (wave > 5);
            wave_spawned++;
            return;
        }
    }
}

static void fire_bullet(void) {
    int i;
    if (fire_cooldown > 0) return;
    for (i = 0; i < BULLET_MAX; i++) {
        if (!bullets[i].active) {
            bullets[i].x = player_x + facing_x * 8;
            bullets[i].y = player_y + facing_y * 8;
            bullets[i].vx = facing_x * 5;
            bullets[i].vy = facing_y * 5;
            bullets[i].active = 1;
            fire_cooldown = 9;
            return;
        }
    }
}

static void update_game(PADTYPE *pad) {
    int i;
    if (pad && pad->stat == 0) {
        if (!(pad->btn & PAD_START)) {
            if (!start_was_pressed) paused = !paused;
            start_was_pressed = 1;
        } else {
            start_was_pressed = 0;
        }
    }
    if (paused) return;

    if (pad && pad->stat == 0) {
        if (!(pad->btn & PAD_UP)) { player_y -= 2; facing_x = 0; facing_y = -1; }
        if (!(pad->btn & PAD_DOWN)) { player_y += 2; facing_x = 0; facing_y = 1; }
        if (!(pad->btn & PAD_LEFT)) { player_x -= 2; facing_x = -1; facing_y = 0; }
        if (!(pad->btn & PAD_RIGHT)) { player_x += 2; facing_x = 1; facing_y = 0; }
        if (!(pad->btn & PAD_CROSS)) fire_bullet();
    }

    if (player_x < 12) player_x = 12;
    if (player_x > 308) player_x = 308;
    if (player_y < 36) player_y = 36;
    if (player_y > 222) player_y = 222;

    if (fire_cooldown > 0) fire_cooldown--;

    if (wave_spawned < wave_total && (wave_spawned - wave_kills) < ENEMY_MAX) {
        if ((random_range(12) == 0)) spawn_enemy();
    }

    for (i = 0; i < ENEMY_MAX; i++) {
        if (enemies[i].active) {
            int dx = player_x - enemies[i].x;
            int dy = player_y - enemies[i].y;
            if (dx > 1) enemies[i].x += enemies[i].speed;
            else if (dx < -1) enemies[i].x -= enemies[i].speed;
            if (dy > 1) enemies[i].y += enemies[i].speed;
            else if (dy < -1) enemies[i].y -= enemies[i].speed;

            if (abs(dx) < 10 && abs(dy) < 10) {
                if (player_hp > 0) player_hp--;
                enemies[i].x += (dx >= 0) ? -12 : 12;
            }
        }
    }

    for (i = 0; i < BULLET_MAX; i++) {
        int j;
        if (!bullets[i].active) continue;
        bullets[i].x += bullets[i].vx;
        bullets[i].y += bullets[i].vy;
        if (bullets[i].x < 0 || bullets[i].x > SCREEN_W ||
            bullets[i].y < 30 || bullets[i].y > SCREEN_H) {
            bullets[i].active = 0;
            continue;
        }
        for (j = 0; j < ENEMY_MAX; j++) {
            if (enemies[j].active &&
                abs(bullets[i].x - enemies[j].x) < 8 &&
                abs(bullets[i].y - enemies[j].y) < 8) {
                bullets[i].active = 0;
                enemies[j].hp--;
                if (enemies[j].hp <= 0) {
                    enemies[j].active = 0;
                    kills++;
                    wave_kills++;
                }
                break;
            }
        }
    }

    if (wave_kills >= wave_total) {
        wave++;
        wave_spawned = 0;
        wave_kills = 0;
        wave_total = 3 + wave * 2;
        if (wave_total > 30) wave_total = 30;
        if (player_hp < 100) player_hp += 10;
        if (player_hp > 100) player_hp = 100;
    }

    if (player_hp <= 0) {
        player_hp = 100;
        player_x = 160;
        player_y = 128;
        wave = 1;
        wave_spawned = 0;
        wave_kills = 0;
        wave_total = 4;
        kills = 0;
        memset(enemies, 0, sizeof(enemies));
        memset(bullets, 0, sizeof(bullets));
    }
}

static void render_frame(void) {
    int i;
    POLY_F4 *p;

    next_prim = db[db_active].packets;
    draw_ground();

    for (i = 0; i < ENEMY_MAX; i++) {
        if (enemies[i].active) draw_soldier(enemies[i].x, enemies[i].y, 0);
    }

    draw_soldier(player_x, player_y, 1);

    for (i = 0; i < BULLET_MAX; i++) {
        if (bullets[i].active) {
            draw_rect(bullets[i].x - 2, bullets[i].y - 2, 4, 4, 255, 220, 90);
        }
    }

    draw_hud();

    if (paused) {
        draw_rect(95, 105, 130, 28, 15, 20, 25);
        draw_rect(110, 116, 10, 8, 230, 230, 230);
        draw_rect(130, 116, 10, 8, 230, 230, 230);
    }

    (void)p;
    DrawSync(0);
    VSync(0);

    db_active ^= 1;
    ClearOTagR(db[db_active].ot, OT_LEN);
    PutDrawEnv(&db[db_active].draw);
    PutDispEnv(&db[db_active].disp);
    SetDispMask(1);
    DrawOTag(db[1 - db_active].ot + (OT_LEN - 1));
}

int main(void) {
    PADTYPE *pad;

    memset(enemies, 0, sizeof(enemies));
    memset(bullets, 0, sizeof(bullets));
    graphics_init();

    while (1) {
        pad = (PADTYPE *)&pad_buffer[0][0];
        update_game(pad);
        render_frame();
    }

    return 0;
}
