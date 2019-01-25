#include <stdio.h>
#include "../layer2.h"
#include "../layer6.h"
#include "../xfont.h"

#include <SDL/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

int main(int argc, char **argv)
{    
    // initialize decoder
    init_xfont();
    init_layer6();
    printf("init ok!\n");
    connect_to_service();

    printf("hello, world!\n");
    
    int count = 0;
    for (;;) {
        if (process_BTX_data())
            break;
        count++;
//        printf("%d\n", count);
//        if (count==1000)
//            break;
    }
    printf("done\n");
//    return 0;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface *screen = SDL_SetVideoMode(480, 240, 32, SDL_SWSURFACE);
    
#ifdef TEST_SDL_LOCK_OPTS
    EM_ASM("SDL.defaults.copyOnLock = false; SDL.defaults.discardOnLock = true; SDL.defaults.opaqueFrontBuffer = false;");
#endif
    
    if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 480; x++) {
            int alpha = 255;
            *((Uint32*)screen->pixels + y * 480 + x) =
            SDL_MapRGBA(screen->format, 
                        memimage[(y * 480 + x) * 3],
                        memimage[(y * 480 + x) * 3 + 1],
                        memimage[(y * 480 + x) * 3 + 2],
                        alpha);
        }
    }
    if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
    SDL_Flip(screen); 
    
    
    SDL_Quit();

    return 0;
}

