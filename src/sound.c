/* WAV loading (lazy) and playback through ASND. */
#include <stdio.h>
#include <dirent.h>
#include <malloc.h>
#include <asndlib.h>
#include "kidpix.h"

#define MAX_SND 160
#define NUM_VOICES 14

typedef struct {
    char path[300];
    char name[96];
    s16 *buf;
    int bytes, rate;
    int voice;
    bool tried;
} Snd;

static Snd snds[MAX_SND];
static int nsnds;
static int next_voice;
static bool snd_ok;

static void scan_dir(const char *sub)
{
    char dir[300];
    snprintf(dir, sizeof dir, "%s/snd%s%s", asset_root, sub[0] ? "/" : "", sub);
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && nsnds < MAX_SND) {
        const size_t n = strlen(e->d_name);
        if (n < 5 || strcasecmp(e->d_name + n - 4, ".wav")) continue;
        snprintf(snds[nsnds].path, sizeof snds[0].path, "%s/%s", dir, e->d_name);
        strncpy(snds[nsnds].name, e->d_name, sizeof snds[0].name - 1);
        snds[nsnds].voice = -1;
        nsnds++;
    }
    closedir(d);
}

void snd_init(void)
{
    ASND_Init();
    ASND_Pause(0);
    snd_ok = true;
    scan_dir("");
    scan_dir("english");
}

static u32 rd32(const u8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24); }
static u16 rd16(const u8 *p) { return p[0] | (p[1] << 8); }

static bool load_wav(Snd *s)
{
    s->tried = true;
    FILE *f = fopen(s->path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *raw = (u8 *)malloc(sz);
    if (!raw || fread(raw, 1, sz, f) != (size_t)sz) { free(raw); fclose(f); return false; }
    fclose(f);

    int fmt_ch = 1, fmt_bits = 8, rate = 11025;
    const u8 *data = NULL;
    u32 dlen = 0;
    long pos = 12;
    while (pos + 8 <= sz) {
        const u32 clen = rd32(raw + pos + 4);
        if (!memcmp(raw + pos, "fmt ", 4)) {
            fmt_ch = rd16(raw + pos + 10);
            rate = rd32(raw + pos + 12);
            fmt_bits = rd16(raw + pos + 22);
        } else if (!memcmp(raw + pos, "data", 4)) {
            data = raw + pos + 8;
            dlen = clen;
            if (pos + 8 + dlen > (u32)sz) dlen = sz - pos - 8;
            break;
        }
        pos += 8 + clen + (clen & 1);
    }
    if (!data || !dlen || fmt_ch < 1) { free(raw); return false; }

    const int bps = fmt_bits / 8;
    const int frames = dlen / (bps * fmt_ch);
    const int bytes = (frames * 2 + 31) & ~31;
    s16 *out = (s16 *)memalign(32, bytes);
    if (!out) { free(raw); return false; }
    memset(out, 0, bytes);
    for (int i = 0; i < frames; i++) {
        const u8 *p = data + i * bps * fmt_ch;
        out[i] = bps == 1 ? (s16)(((int)p[0] - 128) << 8) : (s16)rd16(p);
    }
    free(raw);
    DCFlushRange(out, bytes);
    s->buf = out;
    s->bytes = bytes;
    s->rate = rate;
    return true;
}

static Snd *find(const char *prefix)
{
    const size_t n = strlen(prefix);
    for (int i = 0; i < nsnds; i++)
        if (!strncmp(snds[i].name, prefix, n)) return &snds[i];
    return NULL;
}

static void play(Snd *s)
{
    if (!s->buf && (s->tried || !load_wav(s))) return;
    const int v = 1 + (next_voice++ % NUM_VOICES);
    ASND_StopVoice(v);
    ASND_SetVoice(v, VOICE_MONO_16BIT, s->rate, 0, s->buf, s->bytes, 255, 255, NULL);
    s->voice = v;
}

void snd_play(const char *prefix)
{
    if (!snd_ok) return;
    Snd *s = find(prefix);
    if (s) play(s);
}

void snd_play_idle(const char *prefix)
{
    if (!snd_ok) return;
    Snd *s = find(prefix);
    if (!s) return;
    if (s->voice >= 0 && ASND_StatusVoice(s->voice) == SND_WORKING) return;
    play(s);
}

void snd_oops(void)
{
    char n[16];
    snprintf(n, sizeof n, "oops%d", rnd(4));
    snd_play(n);
}
