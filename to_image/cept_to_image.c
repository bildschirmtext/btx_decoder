#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include "../xfont.h"
#include "../layer2.h"
#include "../attrib.h"
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
	int wx=sx*480;
	int wy=sy*240;
	int ox=(w-wx)/2;
	int oy=(h-wy)/2;
	int y; int x;
	for (y=0; y<h; y++) 
	for (x=0; x<w; x++) {
		int p=y*w+x;
		int r=0;
		int g=0;
		int b=0;
		if ( (y<oy) || (y>=wy+oy) ) {
			int c=(y-oy)/sy/10;
			get_column_colour(c, &r, &g, &b);
		} else 
		if ( (x<ox) || (x>=wx+ox) ) {
			int c=(y-oy)/sy/10;
			get_column_colour(c, &r, &g, &b);
		} else {
			int mi_x=max((x-ox)/sx,479);
			int mi_y=max((y-oy)/sy,239);
			int mi_p=(mi_y*480+mi_x)*3;

			if (memimage!=NULL) {
				r=memimage[mi_p+0];
				g=memimage[mi_p+1];
				b=memimage[mi_p+2];
			}
		}
		pixels[p]=(0xff<<24) | (r<<16) | (g<<8) | (b);
	}
}

int imagenum=0;

void write_image(const uint32_t pixels[], const int w, const int h, const char *fn)
{
	uint8_t opix[w*h*3];
	int n;
	for (n=0; n<h*w; n++) {
		int r=(pixels[n])&0xff;
		int g=(pixels[n]>>8)&0xff;
		int b=(pixels[n]>>16)&0xff;
		opix[n*3+2]=r;
		opix[n*3+1]=g;
		opix[n*3+0]=b;
	}
	FILE *f=fopen(fn,"w");
	sprintf(fn, "P6 %d %d 255\n", w, h);
	fwrite(fn, strlen(fn), 1, f);
	fwrite(opix, w*h*3, 1, f);
	fclose(f);
}




int main(int argc, char ** argv)
{	
	if (argc!=3) {
		printf("Usage: %s <cept-file> <pnm-file>\n", argv[0]);
		return 1;
	}

	FILE *f=fopen(argv[1],"r");
	uint8_t buf[1024*1024];
	size_t s=fread(buf, 1, sizeof(buf), f);
	fclose(f);
	int n;
	for (n=0; n<s; n++) {
		layer2_write_readbuffer(buf[n]);
	}

	init_xfont();
	init_layer6();

	int cnt=0;

	while (layer2_eof()!=0) {
		process_BTX_data();
		cnt=cnt+1;
		if (cnt>s*2) break;
	}
	
	int width=480+120;//480*2+240;
	int height=240+40;//240*3+120;

	uint32_t *image=malloc(width*height*sizeof(uint32_t));

	update_pixels(image, width, height);
	write_image(image, width, height, argv[2]);


	free(image);

	for (n=25; n<38; n++) printf("%c", get_screen_character(n,0));
	printf("\n");


	return 0;
}
