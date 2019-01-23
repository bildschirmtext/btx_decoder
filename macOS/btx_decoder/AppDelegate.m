//
//  AppDelegate.m
//  btx_decoder
//

#import "AppDelegate.h"
#import <QuartzCore/QuartzCore.h>
#import "xfont.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MEMIMAGE_SIZE (480*240*3)
extern unsigned char *memimage;
extern void init_layer6(void);
extern int process_BTX_data(void);
FILE *textlog;
int visible=1;

FILE *f;
int sockfd;
unsigned char last_char;
bool is_last_char_buffered = false;

int layer2getc()
{
    if (is_last_char_buffered) {
        is_last_char_buffered = false;
    } else {
        ssize_t numbytes = recv(sockfd, &last_char, 1, 0);
        if (numbytes == -1) {
            perror("recv");
            exit(1);
        }
    }
    return last_char;
}

void layer2ungetc()
{
    is_last_char_buffered = true;
}


@interface View : NSView

@property (atomic, strong) id nextLayerContents;
- (void)swapToNextFrame;
@end

static CVReturn renderCallback(CVDisplayLinkRef displayLink,
                               const CVTimeStamp *inNow,
                               const CVTimeStamp *inOutputTime,
                               CVOptionFlags flagsIn,
                               CVOptionFlags *flagsOut,
                               void *displayLinkContext) {
    [(__bridge View *)displayLinkContext swapToNextFrame];
    return kCVReturnSuccess;
}


@implementation View

- (CGImageRef)createGameBoyScreenCGImageRef {
    uint8_t *pictureCopy = memimage;

    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, pictureCopy, MEMIMAGE_SIZE, NULL);
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();

    CGImageRef image = CGImageCreate(480, 240, 8, 24, 480*3, colorspace, kCGBitmapByteOrderDefault | kCGImageAlphaNone, provider, NULL, NO, kCGRenderingIntentDefault);
    CFRelease(colorspace);
    CFRelease(provider);
    return image;
}

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.layer = [CALayer layer];
        self.wantsLayer = YES;
        self.layer.magnificationFilter = kCAFilterNearest;

        [self setupDisplayLink];
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect {
    // intentionally nothing
}

- (void)setupDisplayLink {
    CVDisplayLinkRef displayLink;
    CGDirectDisplayID   displayID = CGMainDisplayID();
    CVReturn            error = kCVReturnSuccess;
    error = CVDisplayLinkCreateWithCGDisplay(displayID, &displayLink);
    if (error) {
        NSLog(@"DisplayLink created with error:%d", error);
        displayLink = NULL;
    }
    CVDisplayLinkSetOutputCallback(displayLink, renderCallback, (__bridge void *)self);
    CVDisplayLinkStart(displayLink);
}

- (void)updateLayerContents {
    CGImageRef fullScreenImage = [self createGameBoyScreenCGImageRef];
    self.nextLayerContents = CFBridgingRelease(fullScreenImage);
}

- (void)swapToNextFrame {
    id nextContents = self.nextLayerContents;
    if (nextContents) {
        self.nextLayerContents = nil;
        dispatch_async(dispatch_get_main_queue(), ^{
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            self.layer.contents = nextContents;
            [CATransaction commit];
        });
    }
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)keyDown:(NSEvent *)event
{
    int c = [event.characters characterAtIndex:0];
    char c2;
    NSLog(@"c: 0x%x", c);
    switch (c) {
        case 0xf704:
            c2 = 0x13; // INI
            break;
        case 0xf705:
            c2 = 0x1c; // TER
            break;
        case 0xf706:
            c2 = 0x1a; // DCT
            break;
        case 0x7f:
            c2 = 0x08; // backspace
            send(sockfd, &c2, 1, 0);
            c2 = 0x20; // space
            send(sockfd, &c2, 1, 0);
            c2 = 0x08; // backspace
            break;
        default:
            c2 = c;
    }
    if (send(sockfd, &c2, 1, 0) == -1){
        perror("send");
        exit (1);
    }
}


@end

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;
@property (nonatomic) NSTimeInterval nextFrameTime;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    int scale = 2;
    [self.window setFrame:CGRectMake(0, 1000, 480 * scale, 240 * scale + 22) display:YES];

    CGRect bounds = self.window.contentView.frame;
    bounds.origin = CGPointZero;
    View *view = [[View alloc] initWithFrame:bounds];
    [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self.window.contentView setAutoresizesSubviews:YES];
    [self.window.contentView addSubview:view];
    [self.window makeFirstResponder:view];

    memimage = malloc(MEMIMAGE_SIZE);
    textlog = stderr;
    
    init_fonts();
    default_colors();
    init_layer6();


    struct hostent *he;
    struct sockaddr_in their_addr;

    if ((he=gethostbyname("belgradstr.dyndns.org")) == NULL) {
        herror("gethostbyname");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(20000);
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    bzero(&(their_addr.sin_zero), 8);

    if (connect(sockfd, (struct sockaddr *)&their_addr, \
                sizeof(struct sockaddr)) == -1) {
        perror("connect");
        exit(1);
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSTimeInterval timePerFrame = 1.0 / (1024.0 * 1024.0 / 114.0 / 154.0);

        self.nextFrameTime = [NSDate timeIntervalSinceReferenceDate] + timePerFrame;

        for (;;) {
            if (process_BTX_data()) {
//                break;
            }
            if (true /*gb->is_ppu_dirty()*/) {
                //gb->clear_ppu_dirty();
                [view updateLayerContents];

                // wait until the next 60 hz tick
                NSTimeInterval interval = [NSDate timeIntervalSinceReferenceDate];
                if (interval < self.nextFrameTime) {
//                     [NSThread sleepForTimeInterval:self.nextFrameTime - interval];
                } else if (interval - 20 * timePerFrame > self.nextFrameTime) {
                    self.nextFrameTime = interval;
                }
                self.nextFrameTime += timePerFrame;
            }
        }
    });
}

@end
