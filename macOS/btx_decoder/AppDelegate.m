//
//  AppDelegate.m
//  btx_decoder
//

#import "AppDelegate.h"
#import <QuartzCore/QuartzCore.h>
#import "xfont.h"
#import "layer6.h"
#import "control.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

FILE *textlog = NULL;
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

- (CGImageRef)createCGImageRef {
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, memimage, 480*240*3, NULL);
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
    CGImageRef fullScreenImage = [self createCGImageRef];
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
    if (!event.characters.length) {
        return;
    }
    int ch = [event.characters characterAtIndex:0];
    char c[3];
    int len = 1;
    NSLog(@"c: 0x%x", ch);
    switch (ch) {
        case NSUpArrowFunctionKey:
            c[0] = APU;
            break;
        case NSDownArrowFunctionKey:
            c[0] = APD;
            break;
        case NSLeftArrowFunctionKey:
            c[0] = APB;
            break;
        case NSRightArrowFunctionKey:
            c[0] = APF;
            break;
        case NSF1FunctionKey:
            c[0] = INI;
            break;
        case NSF2FunctionKey:
            c[0] = TER;
            break;
        case NSF3FunctionKey:
            c[0] = DCT;
            break;
        case 0x7f: // backspace
            c[0] = APB;
            c[1] = ' ';
            c[2] = APB;
            len = 3;
            break;
        case 0xc4: // 'Ä'
            c[0] = SS2;
            c[1] = 'H';
            c[2] = 'A';
            len = 3;
            break;
        case 0xd6: // 'Ö'
            c[0] = SS2;
            c[1] = 'H';
            c[2] = 'O';
            len = 3;
            break;
        case 0xdc: // 'Ü'
            c[0] = SS2;
            c[1] = 'H';
            c[2] = 'U';
            len = 3;
            break;
        case 0xdf: // 'ß'
            c[0] = SS2;
            c[1] = '{';
            len = 2;
            break;
        case 0xe4: // 'ä'
            c[0] = SS2;
            c[1] = 'H';
            c[2] = 'a';
            len = 3;
            break;
        case 0xf6: // 'ö'
            c[0] = SS2;
            c[1] = 'H';
            c[2] = 'o';
            len = 3;
            break;
        case 0xfc: // 'ü'
            c[0] = SS2;
            c[1] = 'H';
            c[2] = 'u';
            len = 3;
            break;
        default:
            c[0] = ch;
    }
    if (send(sockfd, c, len, 0) == -1){
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

//    textlog = stderr;
    
    init_xfont();
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

    // Thread 1: Decoder    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        for (;;) {
            process_BTX_data();
        }
    });

    // Thread 2: Display
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSTimeInterval TIME_PER_FRAME = 1.0 / 60;

        self.nextFrameTime = [NSDate timeIntervalSinceReferenceDate] + TIME_PER_FRAME;

        for (;;) {
            NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
            if (now < self.nextFrameTime) {
                [NSThread sleepForTimeInterval:self.nextFrameTime - now];
            } else {
                if (dirty) {
                    dirty = 0;
                    [view updateLayerContents];
                }
                if (now - 2 * TIME_PER_FRAME > self.nextFrameTime) {
                    self.nextFrameTime = now;
                } else {
                    self.nextFrameTime += TIME_PER_FRAME;
                }
            }
        }
    });
}

@end
