#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/statvfs.h>
#include <signal.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <lcd.h>
#include <pcf8574.h>

#define I2C_ADDR_1 0x27   // change as needed
#define BASE 64
#define RS   (BASE + 0)
#define RW   (BASE + 1)
#define EN   (BASE + 2)
#define LED  (BASE + 3)
#define D4   (BASE + 4)
#define D5   (BASE + 5)
#define D6   (BASE + 6)
#define D7   (BASE + 7)

static int lcdHandle = -1;

// Buffers to hold last-displayed lines
static char prev_line1[17] = {0};
static char prev_line2[17] = {0};

// Cache for drive temps and timing
static time_t last_drive_read = 0;
static volatile sig_atomic_t stop_requested = 0;
static int lcd_is_on = 0;

static void lcd_power_on(void) {
    if (lcdHandle < 0) return;
    digitalWrite(LED, HIGH);
    lcdDisplay(lcdHandle, 1);
    lcd_is_on = 1;
}

static void lcd_power_off(void) {
    if (lcdHandle < 0 || !lcd_is_on) return;
    lcdDisplay(lcdHandle, 0);
    digitalWrite(LED, LOW);
    lcd_is_on = 0;
}

static void handle_signal(int sig) {
    (void)sig;
    stop_requested = 1;
}

int read_cpu_temp_celsius(void) {
    const char *path = "/sys/class/thermal/thermal_zone0/temp";
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int milli = 0;
    if (fscanf(f, "%d", &milli) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return milli / 1000;
}

int get_fs_usage(const char *mnt, unsigned long long *total, unsigned long long *used) {
    struct statvfs st;
    if (statvfs(mnt, &st) != 0) return -1;
    *total = st.f_blocks * st.f_frsize;
    *used  = (st.f_blocks - st.f_bfree) * st.f_frsize;
    return 0;
}

void format_iec(char *out, size_t outlen, unsigned long long bytes) {
    const char *units[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
    int u = 0;
    double d = (double)bytes;
    while (d >= 1024.0 && u < (int)(sizeof(units)/sizeof(units[0])) - 1) {
        d /= 1024.0;
        u++;
    }
    snprintf(out, outlen, "%.1f%s", d, units[u]);
}

int get_drive_temp_smartctl(const char *devpath) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "smartctl -A --json %s | jq .ata_smart_attributes.table[17].value", devpath);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    int temp = -1;
    if (fscanf(fp, "%d", &temp) != 1) {
        pclose(fp);
        return -1;
    }
    pclose(fp);
    return temp;
}

int lcd_init_freenove(void) {
    int fd = wiringPiI2CSetup(I2C_ADDR_1);
    if (fd < 0) {
        fprintf(stderr, "wiringPiI2CSetup failed\n");
        return -1;
    }
    pcf8574Setup(BASE, I2C_ADDR_1);
    for (int i = 0; i < 8; i++) {
        pinMode(BASE + i, OUTPUT);
    }
    digitalWrite(LED, HIGH);
    digitalWrite(RW, LOW);
    int handle = lcdInit(2, 16, 4, RS, EN, D4, D5, D6, D7, 0, 0, 0, 0);
    if (handle < 0) {
        fprintf(stderr, "lcdInit failed\n");
        return -1;
    }
    return handle;
}

void lcd_putline_if_changed(int row, const char *line, char *prev_buf) {
    // line is 16 chars (or less, null-terminated)
    for (int i = 0; i < 16; i++) {
        char c = line[i];
        if (c == '\0') c = ' ';  // fill blank
        if (prev_buf[i] != c) {
            lcdPosition(lcdHandle, i, row);
            lcdPutchar(lcdHandle, c);
            prev_buf[i] = c;
        }
    }
}

int main(void) {
    const char *mnt = "/srv/dev-disk-by-uuid-9d471d15-da94-46d4-8bf8-dc94d589f651";  // adjust to your RAID mount path

    if (wiringPiSetup() == -1) {
        fprintf(stderr, "wiringPi setup failed\n");
        return 1;
    }
    lcdHandle = lcd_init_freenove();
    if (lcdHandle < 0) {
        return 1;
    }

    lcd_power_on();
    atexit(lcd_power_off);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Optionally initialize prev buffers to spaces
    memset(prev_line1, ' ', sizeof(prev_line1));
    memset(prev_line2, ' ', sizeof(prev_line2));

    while (!stop_requested) {
        // Build line1: usage
        char line1[17] = {0};
        unsigned long long tot = 0, used = 0;
        if (get_fs_usage(mnt, &tot, &used) == 0) {
            char u_s[32], t_s[32];
            format_iec(u_s, sizeof(u_s), used);
            format_iec(t_s, sizeof(t_s), tot);
            snprintf(line1, sizeof(line1), "%s/%s", u_s, t_s);
        } else {
            snprintf(line1, sizeof(line1), "FS err");
        }

        // Drive temperature polling logic
        int d0 = get_drive_temp_smartctl("/dev/sda");
        int d1 = get_drive_temp_smartctl("/dev/sdb");

        // Build line2: CPU + drive temps
        int ctemp = read_cpu_temp_celsius();
        char line2[17] = {0};
        if (ctemp < 0 && d0 < 0 && d1 < 0) {
            snprintf(line2, sizeof(line2), "No temp data");
        } else {
            // Abbreviated format
            if (d0 < 0) d0 = 0;
            if (d1 < 0) d1 = 0;
            if (ctemp < 0) ctemp = 0;
            // e.g. "C:35c D:33c,34c"
            snprintf(line2, sizeof(line2), "c:%dC d:%dC,%dC",
                     ctemp, d0, d1);
        }

        // Output only changes
        lcd_putline_if_changed(0, line1, prev_line1);
        lcd_putline_if_changed(1, line2, prev_line2);

        usleep(5 * 1000 * 1000);  // 2s delay (adjust as needed)
    }

    lcd_power_off();
    return 0;
}
