#include <stdbool.h>
#include <stdint.h>

#include <gba_video.h>

extern void init(void);
extern void update(void);
extern void render(void);

bool running = true;
int windowWidth = 240;
int windowHeight = 160;
uint16_t *screenPixels;

#define RGB555(r, g, b) ((r) | ((g)<<5) | ((b)<<10))

int main(void)
{
	screenPixels = (uint16_t *)VRAM;

	REG_DISPCNT = MODE_3 | BG2_ON;

	init();
	while (running)
	{
		update();
		render();
	}

	return 0;
}
