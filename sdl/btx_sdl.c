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
int keyboard_mode=0;

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

//Translation table for special characters
#define KM_NORMAL (0)
#define KM_FCOLOR (1)
#define KM_BCOLOR (2)
#define KM_SIZE (3)
#define KM_ATTR (3)
struct {
	int mode; //Mode: 0=normal
	char *input;
	char *output;
} translations[]={
	//Special printable characters
	{KM_NORMAL,	"Ä",	"\x19HA"},
	{KM_NORMAL,	"Ö",	"\x19HO"},
	{KM_NORMAL,	"Ü",	"\x19HU"},
	{KM_NORMAL,	"ä",	"\x19Ha"},
	{KM_NORMAL,	"ö",	"\x19Ho"},
	{KM_NORMAL,	"ü",	"\x19Hu"},
	{KM_NORMAL,	"ß",	"\x1d\x3b"},
	{KM_NORMAL,	"¡",	"\x19\x21"},
	{KM_NORMAL,	"¢",	"\x19\x22"},
	{KM_NORMAL,	"£",	"\x19\x23"},
	{KM_NORMAL,	"$",	"\x19\x24"},
	{KM_NORMAL,	"¥",	"\x19\x25"},
	{KM_NORMAL,	"#",	"\x19\x26"},
	{KM_NORMAL,	"§",	"\x19\x27"},
	{KM_NORMAL,	"¤",	"\x19\x28"},
	{KM_NORMAL,	"‘",	"\x19\x29"},
	{KM_NORMAL,	"“",	"\x19\x2A"},
	//Colours
	//Foreground
	{KM_FCOLOR,	"0",	"\x80"},
	{KM_FCOLOR,	"1",	"\x81"},
	{KM_FCOLOR,	"2",	"\x82"},
	{KM_FCOLOR,	"3",	"\x83"},
	{KM_FCOLOR,	"4",	"\x84"},
	{KM_FCOLOR,	"5",	"\x85"},
	{KM_FCOLOR,	"6",	"\x86"},
	{KM_FCOLOR,	"7",	"\x87"},
	//Colour tables
	{KM_FCOLOR,	"a",	"\x1B\x30\x40"},
	{KM_FCOLOR,	"b",	"\x1B\x31\x40"},
	{KM_FCOLOR,	"c",	"\x1B\x32\x40"},
	{KM_FCOLOR,	"d",	"\x1B\x33\x40"},
	//Background
	{KM_BCOLOR,	"0",	"\x90"},
	{KM_BCOLOR,	"1",	"\x91"},
	{KM_BCOLOR,	"2",	"\x92"},
	{KM_BCOLOR,	"3",	"\x93"},
	{KM_BCOLOR,	"4",	"\x94"},
	{KM_BCOLOR,	"5",	"\x95"},
	{KM_BCOLOR,	"6",	"\x96"},
	{KM_BCOLOR,	"7",	"\x97"},
	{KM_BCOLOR,	"t",	"\x9E"},
	//Attributes
	//Size
	{KM_SIZE,	"0",	"\x8C"},
	{KM_SIZE,	"1",	"\x8D"},
	{KM_SIZE,	"2",	"\x8E"},
	{KM_SIZE,	"3",	"\x8F"},
	//Underline
	{KM_ATTR,	"L",	"\x9A"}, //Start (under) Lining
	{KM_ATTR,	"l",	"\x99"}, //Start (under) Lining
	//Polarity	
	{KM_ATTR,	"I",	"\x9D"}, //Inverted Polarity
	{KM_ATTR,	"i",	"\x9C"}, //Normal Polarity
};

#define TRANSLATIONCNT (sizeof(translations)/sizeof(translations[0]))

void handle_textinput(char *s)
{
	size_t len=strlen(s);	
	if (len>31) len=31;
	char buf[32];
	memset(buf, 0, sizeof(buf));
	memcpy(buf, s, len);
	char *out=buf;
	int n;
	for (n=0; n<TRANSLATIONCNT; n++) {
		if ((strcmp(buf,translations[n].input)==0) && (translations[n].mode==keyboard_mode)) {
			out=translations[n].output;
			break;
		}
	}
	keyboard_mode=KM_NORMAL;
	layer2_write(out, strlen(out));	
}

void handle_keydown(SDL_KeyboardEvent *key)
{
	SDL_Keycode k=key->keysym.sym;
	unsigned int mod=key->keysym.mod;
	if (k==SDLK_F1)		layer2_write("\x13",1); //F1 => Initiator *
	if (k==SDLK_F2)		layer2_write("\x1c",1); //F2 => Terminator #
	if (k==SDLK_F3) {
		if (mod==KMOD_NONE)		keyboard_mode=KM_FCOLOR; //F3 => Foreground color
		if ((mod&KMOD_SHIFT)!=0)	keyboard_mode=KM_BCOLOR; //Shift+F3 => Background color
	}
	if (k==SDLK_F4) 	keyboard_mode=KM_SIZE; //F4 => Size
	if (k==SDLK_F12)	layer2_write("\x1a",3); //F12 => DCT
	if (k==SDLK_LEFT) 	layer2_write("\x08",1); //left
	if (k==SDLK_RIGHT) 	layer2_write("\x09",1); //right
	if (k==SDLK_UP) 	layer2_write("\x0b",1); //Up
	if (k==SDLK_DOWN)	layer2_write("\x0a",1); //Down
	if ((k==SDLK_RETURN) && (mod==KMOD_NONE)) layer2_write("\x0d",1); //Return
	if ((k==SDLK_RETURN) && (mod==KMOD_SHIFT)) layer2_write("\x18",1); //Return+Shift => Erase till end of line
	if (k==SDLK_HOME) {
		if (mod==KMOD_NONE) layer2_write("\x1e",1); //Active Position home
		if ((mod&KMOD_SHIFT)!=0) layer2_write("\x0c",1); //Clear Screen
	}
	if (k==SDLK_BACKSPACE)	layer2_write("\x08 \x08",3);
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

	int width=480*2;
	int height=240*3;

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
		update_pixels(pixels, width, height);
		SDL_UpdateTexture(texture, NULL, pixels, width * sizeof(Uint32));
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);
	}

	SDL_StopTextInput();
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
