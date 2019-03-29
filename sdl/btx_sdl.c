#include <SDL2/SDL.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include "../xfont.h"
#include "../layer2.h"
#include "../layer6.h"



int quit=1;

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

void decoder_thread(void *x_void_ptr)
{
	char conn[256];
	memset(conn, 0, sizeof(conn));
	strncpy(conn,(char *) x_void_ptr, sizeof(conn)-1);
	char *collon=strrchr(conn, ':');
	printf("decoder_thread %s --- %s\n", conn, collon);
	unsigned int port=20000;
	if (collon!=NULL) {
		port=atoi(collon+1);
		collon[0]='\0';
		printf("Connecting to %s port %d\n", conn, port);
	}
	init_xfont();
	layer2_connect2(conn, port);
	init_layer6();	
	dirty=0;
	while (quit) {
		process_BTX_data();
	}
}

void handle_textinput(char *s)
{
	int len=strlen(s);	
	printf("handle_textinput len=%d %s\n", len, s);
	layer2_write(s, len);	
}

void handle_keydown(SDL_KeyboardEvent *key)
{
	SDL_Keycode k=key->keysym.sym;
	if (k==SDLK_F1) layer2_write("\x13",1); //Initiator *
	if (k==SDLK_F2) layer2_write("\x1c",1); //Terminator #
	if (k==SDLK_LEFT) layer2_write("\x08",1); //left
	if (k==SDLK_RIGHT) layer2_write("\x09",1); //right
	if (k==SDLK_UP) layer2_write("\x0b",1); //Up
	if (k==SDLK_DOWN) layer2_write("\x0a",1); //Down
	if (k==SDLK_RETURN) layer2_write("\x0d",1); //Return
	if (k==SDLK_HOME) layer2_write("\x1e",1); //Active Position home
}


int main(int argc, char ** argv)
{	
	if (argc!=2) {
		printf("Usage: %s <host>:<port>\n", argv[0]);
		printf("Example: %s 195.201.94.166:20000\n", argv[0]);
		return 1;
	}
	pthread_t dec_thread;
	if(pthread_create(&dec_thread, NULL, decoder_thread, (void *) argv[1])) {
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}

	int width=480;
	int height=480;

	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window * window = SDL_CreateWindow("BTX-Decoder", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, 0);

	SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, 0);
	SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);

	uint32_t pixels[width*height];

	memset(pixels, 0xaa, width*height * sizeof(Uint32));
	SDL_StartTextInput();
	while (quit!=0)
	{
		int draw=0;
		if (dirty!=0) {
			dirty=0;
			draw=1;
		}
		int eventtimeout=40;
		if (draw!=0) eventtimeout=10;
		SDL_Event event;
		SDL_WaitEventTimeout(&event,250);
		switch (event.type)
		{
			case SDL_TEXTINPUT:
				handle_textinput(event.text.text);
			break;
			case SDL_KEYDOWN:
				handle_keydown(&event.key);
			break;
			case SDL_QUIT:
				quit = 0;
			break;
			case SDL_MOUSEBUTTONUP:
			break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEMOTION:
			break;
		}
		if (draw!=0) {
			update_pixels(pixels, width, height);
			SDL_UpdateTexture(texture, NULL, pixels, width * sizeof(Uint32));
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);
		}
	}

	SDL_StopTextInput();
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
