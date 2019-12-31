#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <SDL.h>
#include <SDL2/SDL_ttf.h>

#define NELEM(a) ((sizeof(a)/sizeof(a[0])))

enum {
    BLOCK_SIZE = 30, /* Side of block */
    BLOCK_PADDING = 2, /* Spacing between blocks */
    NXBLOCKS = 10,
    NYBLOCKS = 20,
    MAX_WIDTH = NXBLOCKS * BLOCK_SIZE + (NXBLOCKS-1) * BLOCK_PADDING, /* Width of game window */
    MAX_HEIGHT = NYBLOCKS * BLOCK_SIZE + (NYBLOCKS-1) * BLOCK_PADDING, /* Height of game window */
    FALL_SPEED = 2,
    FPS = 1, /* Desired frames per second */
};

enum Color {
    EMPTY,
    RED,
    ORANGE,
    YELLOW,
    GREEN,
    BLUE,
    INDIGO,
    VIOLET,
    NCOLORS
};

static SDL_Point tetras[][4] = {
    /* ---- */
    {{ -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 }},
    /* |__ */
    {{ 0, 1 }, { 0, 0 }, { 1, 0 }, { 2, 0 }},
    /* __| */
    {{ 0, 0 }, { 1, 0 }, { 2, 0 }, { 2, 1 }},
    /* 88 */
    {{ 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }},
    /* _|- */
    {{ 0, 0 }, { 1, 0 }, { 1, 1 }, { 2, 1 }},
    /* _|_ */
    {{ -1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 0 }},
    /* -|_ */
    {{ 0, 1 }, { 1, 1 }, { 1, 0 }, { 2, 0 }}
};

static SDL_Color colors[] = {
    {   0, 0, 0 }, /* EMPTY */
    { 150, 28, 0 }, /* RED */
    { 235, 61, 0 }, /* ORANGE */
    { 255, 184, 0 }, /* YELLOW */
    { 96, 139, 50 }, /* GREEN */
    { 1, 111, 222 }, /* BLUE */
    { 0, 44, 106 }, /* INDIGO */
    { 73, 45, 165 }, /* VIOLET */
};

struct Body {
    SDL_Rect r; /* size and position*/
    int vx;     /* x velocity */
    int vy;     /* y velocity */
};
typedef struct Body Body;

struct Game {
    int store[NYBLOCKS][NXBLOCKS];
    int *blocks[NYBLOCKS];
    SDL_Point tetra[4];
    SDL_Point o; /* Position of top-left corner (origin) of board plane */
    SDL_Point p; /* Position of origin of tetramino plane */
    enum Color color; /* Color of tetramino */
    int hastetra; /* Whether a movable tetramino should be draw */
    int score;
    int speed;
};
typedef struct Game Game;

static void rotate(Game *g);
static int collidesp(Game *g, int dx, int dy);
static int clear(SDL_Renderer *r);
static int draw(SDL_Renderer *r, Game *g);
static void reset(Game *g);
static void step(Game *g);
static void ensurepos(Game *g);
static int min(int a, int b);
static int max(int a, int b);
static void newtetra(Game *g);

int
main(void)
{
    Game g; /* Game state */
    SDL_Window *w; /* Window handle */
    SDL_Renderer *r; /* Renderer used for drawing */
    SDL_Texture *t; /* Ball texture */
    SDL_Event e; /* A keyboard or window event */
    unsigned start; /* Start time of frame in ticks */
    unsigned end; /* End time of frame in ticks */
    int done; /* Flag to break out of game loop */
    int rc; /* Return code of program */

    w = NULL;
    r = NULL;
    t = NULL;
    rc = EXIT_SUCCESS;

    srand(time(NULL));
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("Can't initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    if (TTF_Init() == -1) {
        SDL_Log("Can't initialize TTF: %s", TTF_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }
    w = SDL_CreateWindow("Tetris",
                         SDL_WINDOWPOS_UNDEFINED,
                         SDL_WINDOWPOS_UNDEFINED,
                         MAX_WIDTH, MAX_HEIGHT, 0);
    if (w == NULL) { goto error; }
    r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED);
    if (r == NULL) { goto error; }

    SDL_Log("Resetting state");
    reset(&g);
    SDL_Log("Drawing");
    if (draw(r, &g) != 0) { goto error; }

    SDL_Log("Entering game loop");
    start = SDL_GetTicks();
    for (done = 0; !done; ) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                done = 1;
                break;
            case SDL_KEYDOWN:
                switch (e.key.keysym.sym) {
                case SDLK_DOWN:
                    if (!collidesp(&g, 0, 1))
                        g.p.y += 1;
                    break;
                case SDLK_UP:
                    rotate(&g);
                    break;
                case SDLK_LEFT:
                    if (!collidesp(&g, -1, 0))
                        g.p.x -= 1;
                    break;
                case SDLK_RIGHT:
                    if (!collidesp(&g, 1, 0))
                        g.p.x += 1;
                    break;
                }
                break;
            case SDL_KEYUP:
                switch (e.key.keysym.sym) {
                case SDLK_DOWN:
                    /* TODO: Stop moving tetramino */
                    break;
                }
                break;
            }
        }
        /* step(&g); */
        if (draw(r, &g) != 0) { goto error; }
        end = SDL_GetTicks();
        if (end - start >= 1000/g.speed) {
            start = SDL_GetTicks();
            step(&g);
        }
        /* if (end-start < 1000/FPS) */
        /*     SDL_Delay(1000/FPS - (start-end)); */
    }
    goto shutdown;

error:
    rc = EXIT_FAILURE;
    SDL_Log("%s", SDL_GetError());
    
shutdown:
    if (t != NULL) SDL_DestroyTexture(t);
    if (r != NULL) SDL_DestroyRenderer(r);
    if (w != NULL) SDL_DestroyWindow(w);
    TTF_Quit();
    SDL_Quit();
    return rc;
}

static int
collidesp(Game *g, int dx, int dy)
{
    int i;
    int x;
    int y;
    if (!g->hastetra)
        return 0;
    for (i = 0; i < 4; ++i) {
        x = g->o.x + g->p.x + g->tetra[i].x + dx;
        y = g->o.y + g->p.y + g->tetra[i].y + dy;
        if (x < 0 || x >= NXBLOCKS || y < 0 || y >= NYBLOCKS || g->blocks[y][x])
            return 1;
    }
    return 0;
}

/* Draw the background upon which the blocks and text will be rendered. */
static int
clear(SDL_Renderer *r)
{
    int rc;
    SDL_Color *c;

    c = &colors[EMPTY];
    rc = SDL_SetRenderDrawColor(r, c->r, c->g, c->b, c->a);
    if (rc != 0)
        return rc;
    rc = SDL_RenderClear(r);
    if (rc != 0)
        return rc;
    return 0;
}

/* Rotate a tetramino 90 degrees counter-clockwise. */
static void
rotate(Game *g)
{
    int i;
    int x;
    int y;
    int minx;
    int maxx;

    assert(g != NULL);

    /* Rotate blocks and record bottom-left-most block position. */
    for (i = 0; i < 4; ++i) {
        x = -g->tetra[i].y;
        y = g->tetra[i].x;
        if (i == 0) {
            minx = maxx = x;
            /* miny = g->p.y + g->p.y + y; */
        } else {
            minx = min(minx, x);
            maxx = max(maxx, x);
            /* miny = min(miny, g->o.y + g->p.y + y); */
        }
        g->tetra[i].x = x;
        g->tetra[i].y = y;
    }
    ensurepos(g);
    /* if (g->p.x + minx < 0) { */
    /*     /\* g->p.x + minx < 0 *\/ */
    /*     g->p.x = -minx; */
    /* } else if (NXBLOCKS <= g->p.x + maxx) { */
    /*     g->p.x = NXBLOCKS - maxx - 1; */
    /* } */
}

static void
ensurepos(Game *g)
{
    int i;
    int minx;
    int maxx;
    int miny;
    int maxy;

    minx = maxx = g->tetra[0].x;
    miny = maxy = g->tetra[0].y;
    for (i = 0; i < 4; ++i) {
        minx = min(minx, g->tetra[i].x);
        maxx = max(maxx, g->tetra[i].x);
        miny = min(miny, g->tetra[i].y);
        maxy = max(maxy, g->tetra[i].y);
    }
    /* Correct x position */
    if (g->p.x + minx < 0) {
        g->p.x = -minx;
    } else if (NXBLOCKS <= g->p.x + maxx) {
        g->p.x = NXBLOCKS - maxx - 1;
    }
    /* Correct y position */
    if (g->p.y + miny < 0) {
        g->p.y = -miny;
    } else if (NYBLOCKS <= g->p.y + maxy) {
        g->p.y = NYBLOCKS - maxy - 1;
    }
}    

static int
drawblk(SDL_Renderer *r, int bx, int by, enum Color c)
{
    SDL_Rect rect;
    int rc;
    
    assert(r != NULL);

    rect.w = rect.h = BLOCK_SIZE;
    rect.x = bx * (BLOCK_SIZE + BLOCK_PADDING);
    rect.y = by * (BLOCK_SIZE + BLOCK_PADDING);
    rc = SDL_SetRenderDrawColor(r, colors[c].r, colors[c].g, colors[c].b, colors[c].a);
    if (rc != 0)
        return rc;
    rc = SDL_RenderFillRect(r, &rect);
    if (rc != 0)
        return rc;
    return rc;
}

/* Update the screen according to the game state. */
static int
draw(SDL_Renderer *r, Game *g)
{
    int rc;
    int x;
    int y;
    int i;
    int xoff;
    int yoff;

    assert(r != NULL);
    assert(g != NULL);

    /* Clear screen */
    rc = clear(r);
    if (rc != 0)
        return rc;
    /* Draw set blocks */
    for (y = 0; y < NYBLOCKS; ++y) {
        for (x = 0; x < NXBLOCKS; ++x) {
            rc = drawblk(r, x, y, g->blocks[y][x]);
            if (rc != 0)
                return rc;
        }
    }
    /* Draw tetramino */
    if (g->hastetra) {
        xoff = g->o.x + g->p.x;
        yoff = g->o.y + g->p.y;
        for (i = 0; i < 4; ++i) {
            rc = drawblk(r, xoff + g->tetra[i].x, yoff + g->tetra[i].y, g->color);
            if (rc != 0)
                return rc;
        }
    }
    /* Update screen */
    SDL_RenderPresent(r);
    return rc;
}

/* Reset the game state. */
static void
reset(Game *g)
{
    int i;
    int j;
    assert(g != NULL);

    /* Clear the board */
    SDL_Log("Clearing the board");
    for (i = 0; i < NYBLOCKS; ++i) {
        for (j = 0; j < NXBLOCKS; ++j) {
            g->store[i][j] = EMPTY; /*rand() % NCOLORS;*/
        }
    }
    /* Reset row pointers */
    SDL_Log("Setting row pointers");
    for (i = 0; i < NYBLOCKS; ++i) {
        g->blocks[i] = &g->store[i][0];
    }
    g->o.x = g->o.y = 0;
    newtetra(g);

    /* Set score */
    SDL_Log("Resetting the score");
    g->score = 0;
    g->speed = FALL_SPEED;
}

static void
newtetra(Game *g)
{
    int i;
    int j;
    assert(g != NULL);

    j = rand() % 7;
    for (i = 0; i < 4; ++i) {
        g->tetra[i] = tetras[j][i];
    }
    g->p.x = NXBLOCKS / 2;
    g->p.y = -10;
    ensurepos(g);
    g->color = 1 + rand() % (NCOLORS-1);
    g->hastetra = 1;
}

static int
search(int *a, int n, int key)
{
    int i;
    
    assert(a != NULL);
    for (i = 0; i < n; ++i) {
        if (a[i] == key)
            return i;
    }
    return -1;
}

static int
icmp(const void *a, const void *b)
{
    return (int) b - (int) a;
}

static void
erase(Game *g, int *rows, int nrows)
{
    int i;
    int j;
    int k;
    assert(g != NULL);

    i = 0;
    j = 0; /* Index of row to remove */
    for (i = NYBLOCKS-1; i >= 0 && j < nrows; --i) {
        if (i != rows[j])
            continue;
        ++j;
        /* Erase row */
        for (k = 0; k < NXBLOCKS; ++k) {
            g->blocks[i][k] = EMPTY;
        }
        /* Move all pointers from [i+1, NYBLOCKS) down */
        int *p = g->blocks[i];
        for (k = i; k > 0; --k) {
            g->blocks[k] = g->blocks[k-1];
        }
        g->blocks[0] = p;
    }
}

static void
step(Game *g)
{
    int i;
    int j;
    int x;
    int y;
    int sum;
    int nr;
    int rows[4];
    nr = 0;
    if (g->hastetra && collidesp(g, 0, 1)) {
        /* Copy tetramino blocks to board and make new tetramino */
        for (i = 0; i < 4; ++i) {
            x = g->o.x + g->p.x + g->tetra[i].x;
            y = g->o.y + g->p.y + g->tetra[i].y;
            g->blocks[y][x] = g->color;
            for (sum = 0, j = 0; j < NXBLOCKS; ++j) {
                sum += g->blocks[y][j] ? 1 : 0;
            }
            if (sum == NXBLOCKS && search(rows, nr, y) < 0) {
                rows[nr++] = y;
            }
        }
        /* Erase full rows */
        if (nr > 0) {
            qsort(rows, nr, sizeof rows[0], icmp);
            erase(g, rows, nr);
        }
        newtetra(g);
    } else {
        g->p.y += 1;
    }
    assert(g != NULL);
}

static int
min(int a, int b)
{
    return (a < b) ? a : b;
}

static int
max(int a, int b)
{
    return (a > b) ? a : b;
}
