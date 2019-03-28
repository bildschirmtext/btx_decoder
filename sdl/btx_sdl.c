#include <SDL2/SDL.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "../xfont.h"
#include "../layer2.h"
#include "../layer6.h"

static int max(int a, int b)
{
	if (a<b) return a;
	return b;
}


void update_pixels(uint32_t pixels[], const int w, const int h)
{
	int sy=h/240; //Scale factor y
	int sx=w/480; //scale factor x
	int y; int x;
	for (y=0; y<h; y++) 
	for (x=0; x<w; x++) {
		int mi_x=max(x/sx,479);
		int mi_y=max(y/sy,239);
		int mi_p=(mi_y*480+mi_x)*3;
		int p=y*w+x;
		int r=0;
		int g=0;
		int b=0;
		if ( ((x/sx)%12==0) || ((y/sy)%10==0)) {
			r=255;
			g=255;
			b=255;
		}
		if (memimage!=NULL) {
			r=memimage[mi_p+0];
			g=memimage[mi_p+1];
			b=memimage[mi_p+2];
		}
		pixels[p]=(0xff<<24) | (r<<16) | (g<<8) | (b);
	}
}

void init_btx(char *conn)
{
	fprintf(stderr,"init_xfont\n");
	init_xfont();
	fprintf(stderr,"init_layer2_connect2\n");
	layer2_connect2("localhost", 20000);
	fprintf(stderr,"init_layer6\n");
	init_layer6();	
}


int main(int argc, char ** argv)
{
	int quit=1;
	SDL_Event event;

	init_btx(NULL);

	int width=480;
	int height=480;

	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window * window = SDL_CreateWindow("BTX-Decoder", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, 0);

	SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, 0);
	SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);

	uint32_t pixels[width*height];

	memset(pixels, 0xaa, width*height * sizeof(Uint32));

	while (quit!=0)
	{
		int btx_res=process_BTX_data();
		printf("process_BTX_data=%d\n",btx_res);
		update_pixels(pixels, width, height);
		SDL_UpdateTexture(texture, NULL, pixels, width * sizeof(Uint32));
		int eventtimeout=40;
		if (btx_res==0) eventtimeout=0;
		SDL_WaitEventTimeout(&event,40);
		switch (event.type)
		{
			case SDL_QUIT:
				quit = 0;
			break;
			case SDL_MOUSEBUTTONUP:
			break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEMOTION:
			break;
		}

		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);
	}

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
