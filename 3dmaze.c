#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_LEN(arr) (sizeof(arr)/sizeof(arr[0]))
#define RGB555(r, g, b) ((r) | ((g)<<5) | ((b)<<10))

#ifdef __DEVKITARM__
#define BLOCKY_MODE
#endif

// from backend
extern bool running;
extern int windowWidth;
extern int windowHeight;
extern uint16_t *screenPixels;

#ifdef __DEVKITARM__

#define ARM_CODE __attribute__((section(".iwram"), target("arm"), long_call))
#define EWRAM __attribute__((section(".ewram")))

// runs much, much faster when these are just constants!
#define windowWidth 240
#define windowHeight 160
#define screenPixels ((uint16_t *)0x06000000)

#define REG_BG2PA (*(volatile uint16_t *)0x04000020)
#define REG_BG2PB (*(volatile uint16_t *)0x04000022)
#define REG_BG2PC (*(volatile uint16_t *)0x04000024)
#define REG_BG2PD (*(volatile uint16_t *)0x04000026)

#else

#define ARM_CODE
#define EWRAM

uint16_t dummyReg;

#define REG_BG2PA dummyReg
#define REG_BG2PB dummyReg
#define REG_BG2PC dummyReg
#define REG_BG2PD dummyReg

#endif

#include "fixedpoint.h"

//----------------------------------------------------------------------
// Textures
//----------------------------------------------------------------------

#define TEXTURE_WIDTH 128
#define TEXTURE_HEIGHT 128
extern const uint16_t tex_brick[];
extern const uint16_t tex_castle[];
extern const uint16_t tex_cover8[];
extern const uint16_t tex_wood24[];

ARM_CODE
static inline uint16_t sample_texture(const uint16_t *tex, fixed_t u, fixed_t v)
{
	int x = fx_fract(u) * TEXTURE_WIDTH / (1 << FRACT_BITS);
	int y = fx_fract(v) * TEXTURE_HEIGHT / (1 << FRACT_BITS);
	return tex[y * TEXTURE_WIDTH + x];
}

//----------------------------------------------------------------------
// Maze definition
//----------------------------------------------------------------------

#define WALL_X (1 << 0)
#define WALL_Y (1 << 1)
#define GLOBE  (1 << 2)

#define I WALL_Y
#define _ WALL_X
#define L (WALL_X|WALL_Y)

#define MAZE_WIDTH 8
#define MAZE_HEIGHT 8
static uint8_t maze[MAZE_HEIGHT][MAZE_WIDTH] =
{
	{ I, _, _, _, I, 0, L, 0 },
	{ L, _, _, 0, 0, L, 0, _ },
	{ I, 0, _, _, I, I, 0, I|GLOBE },
	{ I, I|GLOBE, L, _, _, I, L, _ },
	{ I, L, 0, L, _, 0, I, _ },
	{ I, I, L, 0, I, _, I, 0 },
	{ I, L, 0, I, L, _, _, I },
	{ L, _, _, L, _, L, _, _ },
};

#undef I
#undef _
#undef L

static inline uint8_t maze_lookup(int x, int y)
{
	if (x < MAZE_WIDTH && y < MAZE_HEIGHT)
		return maze[y][x];
	return 0;
}

//----------------------------------------------------------------------
// Math functions
//----------------------------------------------------------------------

typedef struct Vec2 { fixed_t x, y; } Vec2;

extern const fixed_t sineTable[65536/4+1];

ARM_CODE
static fixed_t fixed_sin(uint32_t angle)
{
	if (angle >= UINT_MAX/2+1)
		return -fixed_sin(angle - (UINT_MAX/2+1));
	if (angle > UINT_MAX/4+1)
	{
		angle -= UINT_MAX/4+1;
		angle = UINT_MAX/4+1 - angle;
	}
	return sineTable[angle >> 16];
}

ARM_CODE static inline fixed_t fixed_cos(uint32_t angle) { return fixed_sin(angle + UINT_MAX/4); }

ARM_CODE static inline Vec2 vec2_add(Vec2 a, Vec2 b) { return (Vec2){ a.x+b.x, a.y+b.y }; }
ARM_CODE static inline Vec2 vec2_sub(Vec2 a, Vec2 b) { return (Vec2){ a.x-b.x, a.y-b.y }; }

ARM_CODE
static inline Vec2 vec2_rotate(Vec2 v, int32_t angle)
{
	fixed_t s = fixed_sin(angle);
	fixed_t c = fixed_cos(angle);

	return (Vec2){ fx_mul(v.x,c) - fx_mul(v.y,s), fx_mul(v.x,s) + fx_mul(v.y,c) };
}

static Vec2 camPos;
static uint32_t camYaw;
static uint32_t camRoll;

ARM_CODE
static inline void plot_pixel(int x, int y, uint16_t color)
{
	screenPixels[y * windowWidth + x] = color;
}

ARM_CODE
static inline void plot_pixel_checked(int x, int y, uint16_t color)
{
	if (x >= 0 && x < windowWidth && y >= 0 && y < windowHeight)
		screenPixels[y * windowWidth + x] = color;
}

ARM_CODE
static Vec2 cast_ray_2d(Vec2 pos, Vec2 dir)
{
	fixed_t sx;  // ray distance (hypotenuse) it takes to move one unit in x direction
	fixed_t sy;  // ray distance (hypotenuse) it takes to move one unit in y direction

	// This division is an expensive performance killer, and I hate it!
	fixed_t dydx = dir.x == 0 ? 0 : fx_div(dir.y,dir.x);
	fixed_t dxdy = dir.y == 0 ? 0 : fx_div(dir.x,dir.y);
	sx = dir.x == 0 ? FIXED_MAX : fx_sqrt(TO_FIXED(1) + fx_mul(dydx,dydx));
	sy = dir.y == 0 ? FIXED_MAX : fx_sqrt(TO_FIXED(1) + fx_mul(dxdy,dxdy));

	fixed_t lx = FIXED_MAX;  // ray length in accumulated x direction
	fixed_t ly = FIXED_MAX;  // ray length in accumulated y direction

	struct { int x, y; } step = { 0, 0 };

	// round down to cell
	int cellx = fx_int(pos.x);  // cell to check walls in
	int celly = fx_int(pos.y);

	if (dir.x > 0)
	{
		step.x = 1;
		lx = fx_mul(sx, TO_FIXED(1) - fx_fract(pos.x));
	}
	else if (dir.x < 0)
	{
		step.x = -1;
		lx = fx_mul(sx, fx_fract(pos.x));
	}

	if (dir.y > 0)
	{
		step.y = 1;
		ly = fx_mul(sy, TO_FIXED(1) - fx_fract(pos.y));
	}
	else if (dir.y < 0)
	{
		step.y = -1;
		ly = fx_mul(sy, fx_fract(pos.y));
	}

	// since cells define left and bottom walls, we need to
	// advance the check to the next adjacent cell when looking
	// forward or right
	int celloffsx = (dir.x > 0) ? 1 : 0;
	int celloffsy = (dir.y > 0) ? 1 : 0;

	Vec2 hit;

	//for (int i = 0; i < 100; i++)
	while (1)
	{
		// check the current cell
		if (lx < ly)
		{
			cellx += celloffsx;
			// check left/right map bounds
			if (cellx <= 0 || cellx >= MAZE_WIDTH
			  // check cell vertical wall
			 || (maze[celly][cellx] & WALL_Y))
			{
				hit.x = TO_FIXED(cellx);
				hit.y = pos.y + fx_mul(dydx, hit.x-pos.x);
				return hit;
			}
			cellx -= celloffsx;

			cellx += step.x;
			lx += sx;
		}
		else
		{
			celly += celloffsy;
			// check top/bottom map bounds
			if (celly <= 0 || celly >= MAZE_HEIGHT
			  // check cell horizontal wall
			 || (maze[celly][cellx] & WALL_X))
			{
				hit.y = TO_FIXED(celly);
				hit.x = pos.x + fx_mul(dxdy, hit.y-pos.y);
				return hit;
			}
			celly -= celloffsy;

			celly += step.y;
			ly += sy;
		}
	}
}

static fixed_t slopeLUT[160];

ARM_CODE
void draw_scene(void)
{
	// This needs a lot of optimization!

	//const fixed_t h = TO_FIXED(1);  // height of screen, in world space
	const fixed_t w = TO_FIXED(1);  //fx_div(fx_mul(h, TO_FIXED(windowWidth)), TO_FIXED(windowHeight));  // width of screen, in world space
	const fixed_t d = TO_FIXED(.5);  // distance from camera to screen, in world space

	//fixed_t dy = -fx_div(h, TO_FIXED(windowHeight));
	fixed_t dx = fx_div(w, TO_FIXED(windowWidth));

	// x and y components of ray
	fixed_t x;
	//fixed_t y;

	x = -TO_FIXED(.5);
#ifdef BLOCKY_MODE
	for (int px = 0; px < windowWidth/2; px++, x += dx*2)
#else
	for (int px = 0; px < windowWidth; px++, x += dx)
#endif
	{
		Vec2 dir = (Vec2){ fx_mul(x, w), d };
		dir = (vec2_rotate((dir), camYaw));  // rotate by camera angle
		Vec2 rayHit = cast_ray_2d(camPos, dir);

		// in camera space
		Vec2 rayHit2 = vec2_rotate(vec2_sub(rayHit, camPos), -camYaw);

		const uint16_t *texture = (maze_lookup(fx_int(rayHit.x), fx_int(rayHit.y)) & GLOBE) ? tex_cover8 : tex_brick;

		//y = TO_FIXED(.3);
#ifdef BLOCKY_MODE
		for (int py = 0; py < windowHeight; py += 2/*, y += dy*2*/)
#else
		for (int py = 0; py < windowHeight; py++/*, y += dy*/)
#endif
		{
			fixed_t depth = rayHit2.y;
			fixed_t slope = slopeLUT[py];//fx_div(y, d);
			// get z coordinate of ray hit
			fixed_t z = TO_FIXED(.5) + fx_mul(depth, slope);

			uint16_t color;
			if (z < 0)
				color = RGB555(30, 15, 0);
			else if (z >= TO_FIXED(1))
				color = RGB555(20, 20, 20);
			else
			{
				color = sample_texture(texture, rayHit.x+rayHit.y, z);
			}
#ifdef BLOCKY_MODE
			uint16_t *ptr = screenPixels + py/2 * windowWidth + px;
			*ptr = *(ptr + windowWidth) = color;
#else
			plot_pixel(px, py, color);
#endif
		}
	}
}

static const int CELLW = 8;

ARM_CODE
static void plot_map_pixel(Vec2 point, uint16_t color)
{
	int x = fx_int(point.x*CELLW);
	int y = MAZE_HEIGHT*CELLW-1 - fx_int(point.y*CELLW);
	plot_pixel_checked(x, y, color);
}

ARM_CODE
static void draw_map(void)
{
	for (int y = 0; y < MAZE_HEIGHT*CELLW; y++)
		for (int x = 0; x < MAZE_WIDTH*CELLW; x++)
			plot_pixel(x, y, RGB555(0, 0, 0));

	for (int y = 0; y < MAZE_HEIGHT; y++)
	{
		for (int x = 0; x < MAZE_WIDTH; x++)
		{
			if (maze[y][x] & WALL_Y)
				for (int i = 0; i < CELLW; i++)
					plot_pixel(x*CELLW, MAZE_HEIGHT*CELLW-1 - (y*CELLW + i), RGB555(0, 0, 31));
			if (maze[y][x] & WALL_X)
				for (int i = 0; i < CELLW; i++)
					plot_pixel(x*CELLW + i, MAZE_HEIGHT*CELLW-1 -  y*CELLW, RGB555(0, 0, 31));
		}
	}

	const Vec2 dir = vec2_rotate((Vec2){ TO_FIXED(0), TO_FIXED(1) }, camYaw);

	// white = player position
	plot_map_pixel(camPos, RGB555(31, 31, 31));

	// green = look direction
	Vec2 target = vec2_add(camPos, dir);
	plot_map_pixel(target, RGB555(0, 31, 0));

	// red = ray hit
	Vec2 hit = cast_ray_2d(camPos, dir);
	plot_map_pixel(hit, RGB555(31, 0, 0));
}

static enum { UP, LEFT, DOWN, RIGHT } direction = LEFT;
static uint32_t targetYaw;
static Vec2 targetPos;

void init(void)
{
	// Initial position
	camPos.x = TO_FIXED(4.5);
	camPos.y = TO_FIXED(.5);
	targetPos = camPos;
	camYaw = 0x40000000;
	targetYaw = camYaw;

	// flip maze
	uint8_t temp[MAZE_HEIGHT][MAZE_WIDTH];

	for (int r = 0; r < MAZE_HEIGHT; r++)
		memcpy(temp + MAZE_HEIGHT - r - 1, maze + r, sizeof(temp[0]));
	memcpy(maze, temp, sizeof(maze));

	// init LUT
	const fixed_t h = TO_FIXED(1);  // height of screen, in world space
	const fixed_t d = TO_FIXED(.5);  // distance from camera to screen, in world space
	fixed_t dy = -fx_div(h, TO_FIXED(windowHeight));
	fixed_t y = TO_FIXED(.3);
	for (int py = 0; py < windowHeight; py++, y += dy)
		slopeLUT[py] = fx_div(y, d);

	// Stretch the screen 2x in blocky mode
#ifdef BLOCKY_MODE
	REG_BG2PA = 1 << 7;
	REG_BG2PD = 1 << 7;
#endif
}

ARM_CODE
static int find_direction(int x, int y, int direction)
{
#define TRY_UP    if (y+1 < MAZE_HEIGHT && !(maze[y+1][x] & WALL_X)) return UP
#define TRY_LEFT  if (!(maze[y][x] & WALL_Y)) return LEFT
#define TRY_DOWN  if (!(maze[y][x] & WALL_X)) return DOWN
#define TRY_RIGHT if (x+1 < MAZE_WIDTH && !(maze[y][x+1] & WALL_Y)) return RIGHT
	// Solve the maze by sticking to the right wall throughout
	switch (direction)
	{
	case LEFT:  TRY_UP;    TRY_LEFT;  TRY_DOWN;  return RIGHT;
	case UP:    TRY_RIGHT; TRY_UP;    TRY_LEFT;  return DOWN;
	case RIGHT: TRY_DOWN;  TRY_RIGHT; TRY_UP;    return LEFT;
	case DOWN:  TRY_LEFT;  TRY_DOWN;  TRY_RIGHT; return UP;
	}
#undef TRY_UP
#undef TRY_LEFT
#undef TRY_DOWN
#undef TRY_RIGHT
	// unreachable
	assert(0); return 0;
}

void update(void)
{
	const fixed_t delta = TO_FIXED(1./16.);

	// update direction
	if (camYaw != targetYaw)
	{
		camYaw -= 0x4000000;
		return;
	}

	// update position
	if      (camPos.x < targetPos.x) camPos.x += delta;
	else if (camPos.x > targetPos.x) camPos.x -= delta;
	else if (camPos.y < targetPos.y) camPos.y += delta;
	else if (camPos.y > targetPos.y) camPos.y -= delta;
	else  // reached target
	{
		int x = fx_int(camPos.x);
		int y = fx_int(camPos.y);

		// choose new direction
		direction = find_direction(x, y, direction);
		// set target to one square in new direction
		switch (direction)
		{
		case LEFT:  targetPos.x -= TO_FIXED(1); break;
		case RIGHT: targetPos.x += TO_FIXED(1); break;
		case DOWN:  targetPos.y -= TO_FIXED(1); break;
		case UP:    targetPos.y += TO_FIXED(1); break;
		}
		targetYaw = direction * 0x40000000;
	}
}

void render(void)
{
	draw_scene();
	//draw_map();
}
