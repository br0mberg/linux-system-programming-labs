#include <stdint.h>
#include <stdbool.h>
#include "io.h"

#define VGA_MEM ((volatile uint16_t*)0xB8000)
#define VGA_W 80
#define VGA_H 25

static uint8_t cur_x = 0, cur_y = 0;

static inline uint8_t vga_attr(uint8_t fg, uint8_t bg) {
    return (bg << 4) | (fg & 0x0F);
}
static void vga_put_at(char c, uint8_t x, uint8_t y, uint8_t attr) {
    VGA_MEM[y * VGA_W + x] = ((uint16_t)attr << 8) | (uint8_t)c;
}
static void vga_clear(uint8_t attr) {
    for (int y = 0; y < VGA_H; y++)
        for (int x = 0; x < VGA_W; x++)
            vga_put_at(' ', (uint8_t)x, (uint8_t)y, attr);
    cur_x = 0; cur_y = 0;
}
static void vga_write(const char* s, uint8_t attr) {
    while (*s) {
        char c = *s++;
        if (c == '\n') { cur_x = 0; if (++cur_y >= VGA_H) cur_y = VGA_H - 1; continue; }
        vga_put_at(c, cur_x, cur_y, attr);
        if (++cur_x >= VGA_W) { cur_x = 0; if (++cur_y >= VGA_H) cur_y = VGA_H - 1; }
    }
}
static void vga_write_at(int x, int y, const char* s, uint8_t attr) {
    int cx = x;
    while (*s && cx < VGA_W) {
        vga_put_at(*s++, (uint8_t)cx++, (uint8_t)y, attr);
    }
}

// PC speaker (PIT ch2)
static void speaker_on(uint32_t freq_hz) {
    if (freq_hz == 0) freq_hz = 880;
    uint32_t div = 1193182u / freq_hz;

    outb(0x43, 0xB6); // ch2, lobyte/hibyte, mode 3
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3) outb(0x61, tmp | 3);
}
static void speaker_off(void) {
    outb(0x61, inb(0x61) & 0xFC);
}

// Keyboard (простая карта сканкодов)
static const char scancode_to_ascii[128] = {
    0, 27,'1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0,' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char kbd_getch(void) {
    while ((inb(0x64) & 1) == 0) { }  // ждём символ
    uint8_t sc = inb(0x60);
    if (sc & 0x80) return 0;          // key release
    if (sc < 128) return scancode_to_ascii[sc];
    return 0;
}

// RTC CMOS time
static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}
static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}
static void read_time_hms(uint8_t* hh, uint8_t* mm, uint8_t* ss) {
    uint8_t s = cmos_read(0x00);
    uint8_t m = cmos_read(0x02);
    uint8_t h = cmos_read(0x04);
    *ss = bcd_to_bin(s);
    *mm = bcd_to_bin(m);
    *hh = bcd_to_bin(h);
}

static void write_time_top(uint8_t attr) {
    uint8_t hh, mm, ss;
    read_time_hms(&hh, &mm, &ss);

    char buf[9];
    buf[0] = '0' + (hh / 10); buf[1] = '0' + (hh % 10);
    buf[2] = ':';            buf[3] = '0' + (mm / 10); buf[4] = '0' + (mm % 10);
    buf[5] = ':';            buf[6] = '0' + (ss / 10); buf[7] = '0' + (ss % 10);
    buf[8] = 0;

    vga_write_at(0, 0, "TIME ", attr);
    vga_write_at(5, 0, buf, attr);

    // подчистим хвост строки (чтобы не оставалось мусора)
    for (int x = 13; x < VGA_W; x++) vga_put_at(' ', (uint8_t)x, 0, attr);
}

static void echo_char(char c, uint8_t attr) {
    if (c == '\b') {
        if (cur_x > 0) {
            cur_x--;
            vga_put_at(' ', cur_x, cur_y, attr);
        }
        return;
    }
    char s[2] = {c, 0};
    vga_write(s, attr);
}

static void prompt(uint8_t attr) {
    vga_write("\n> ", attr);
}

static bool streq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; a++; b++; }
    return *a == 0 && *b == 0;
}

void kmain(void) {
    uint8_t blue  = vga_attr(15, 1); // белый на синем
    uint8_t green = vga_attr(0, 2);  // черный на зелёном

    // экран 
    vga_clear(blue);
    vga_write("FSB OF RUSSIA LOCKED U PC\n", blue);
    vga_write("To resolve this issue, please follow the instructions below.\n\n", blue);
    vga_write("You will receive a password after completing the procedure.\n", blue);
    vga_write("Input password here:\n", blue);

    // пищим до правильного пароля
    speaker_on(880);

    const char* USER_PASS = "password";
    char passbuf[64];
    int plen = 0;

    while (1) {
        char c = kbd_getch();
        if (!c) continue;

        if (c == '\n') {
            passbuf[plen] = 0;
            if (streq(passbuf, USER_PASS)) break;

            vga_write("\nPassword incorrect :) Poprobui ugadat' eche and smiris'.\nPassword: ", blue);
            plen = 0;
            continue;
        }

        if (c == '\b') {
            if (plen > 0) { plen--; echo_char('\b', blue); }
            continue;
        }

        if (plen < (int)sizeof(passbuf) - 1) {
            passbuf[plen++] = c;
            echo_char('*', blue); // скрываем ввод пароля
        }
    }

    // разблокировка
    speaker_off();
    vga_clear(green);

    // время сверху
    write_time_top(green);

    // зона ввода ниже первой строки
    cur_x = 0; cur_y = 1;
    vga_write("Unlocked. Good luck.\n", green);
    prompt(green);

    char line[128];
    int len = 0;

    while (1) {
        write_time_top(green);

        char c = kbd_getch();
        if (!c) continue;

        if (c == '\n') {
            line[len] = 0;
            prompt(green);
            len = 0;
            continue;
        }

        if (c == '\b') {
            if (len > 0) { len--; echo_char('\b', green); }
            continue;
        }

        if (len < (int)sizeof(line) - 1) {
            line[len++] = c;
            echo_char(c, green);
        }
    }
}

// make clean
// make
// env -i PATH=/usr/bin:/bin HOME="$HOME" DISPLAY="$DISPLAY" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR"   /usr/bin/qemu-system-i386 -cdrom kernel.iso