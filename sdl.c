#include <SDL.h>
#include <stdbool.h>

extern void init(void);
extern void update(void);
extern void render(void);

bool running = true;
int windowWidth = 240;
int windowHeight = 160;
uint16_t *screenPixels;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

static unsigned long frameCount;
static unsigned long frameTime;

int main(void)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	window = SDL_CreateWindow("3D Maze",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		windowWidth, windowHeight,
		SDL_WINDOW_SHOWN);
	if (window == NULL)
	{
		fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
		return 1;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL)
	{
		fprintf(stderr, "Failed to get create renderer: %s\n", SDL_GetError());
		return 1;
	}

	texture = SDL_CreateTexture(renderer,
		SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING,
		windowWidth, windowHeight);
	if (texture == NULL)
	{
		fprintf(stderr, "Failed to get create texture: %s\n", SDL_GetError());
		return 1;
	}
	screenPixels = malloc(windowWidth * windowHeight * 2);

bool shouldUpdate = false;

	init();
	while (running)
	{
		SDL_Event event;
		Uint32 start, end;

		//if (shouldUpdate)
			update();
		start = SDL_GetTicks();
		render();
		end = SDL_GetTicks();
		frameTime += end - start;
		frameCount++;

		SDL_UpdateTexture(texture, NULL, screenPixels, windowWidth * 2);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);

		while (SDL_PollEvent(&event))  // process all pending events
		{
			switch (event.type)
			{
			case SDL_QUIT:
				running = false;
				break;
			case SDL_KEYDOWN:
				shouldUpdate = true;
				break;
			case SDL_KEYUP:
				shouldUpdate = false;
				break;
			}
		}
	}
	SDL_Quit();
	printf("avg frame time: %.3f ms\n", (double)frameTime / (double)frameCount);
	return 0;
}
