#ifndef TOOLS_INT_H
#define TOOLS_INT_H
#include "kidpix.h"

typedef struct {
    bool down;
    int sx, sy;          /* press position (canvas coords) */
    int px, py;          /* previous position */
    int step;            /* move counter */
    float dist;          /* distance accumulator for spaced stamps */
    int lx, ly;          /* last special point (connect-the-dots etc.) */
    int cx, cy;          /* current position */
    float t;             /* free-running parameter */
} Stroke;

extern Stroke S;
extern bool shift_mod;                       /* "enlarge" modifier (B / Shift) */
extern bool alt_mod;                         /* secondary modifier (2 / X) */
#define SCALE() (shift_mod ? 2 : 1)

u32 *undo_peek(void);
void undo_restore_base(void);
void anim_start(void (*fn)(int f), int frames);
bool anim_running(void);
Paint cur_paint(int tex);

/* brushes.c */
#define BRUSH_COUNT 28
void   brush_select(int b);
void   brush_down(int x, int y);
void   brush_move(int x, int y);
void   brush_up(int x, int y);
void   brush_frame(void);
const char *brush_icon(int b);
void   brush_preview(int x, int y);

/* mixer.c */
#define MIXER_COUNT 15
void   mixer_down(int x, int y);
void   mixer_move(int x, int y);
void   mixer_up(int x, int y);
void   mixer_frame(void);
const char *mixer_icon(int m);

/* eraser.c */
#define ERASER_COUNT 13
void   eraser_down(int x, int y);
void   eraser_move(int x, int y);
void   eraser_up(int x, int y);
void   eraser_frame(void);
void   eraser_preview(int x, int y);
const char *eraser_icon(int e);
void   eraser_select(int e);

#endif
