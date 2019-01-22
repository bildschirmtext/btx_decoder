//
//  AppDelegate.m
//  btx_decoder
//

#import "AppDelegate.h"
#import <QuartzCore/QuartzCore.h>
#import "xfont.h"

#define MEMIMAGE_SIZE (480*240*3)
extern unsigned char *memimage;
extern void init_layer6(void);
extern int process_BTX_data(void);
FILE *textlog;
int visible=1;

FILE *f;
int last_char;
bool is_last_char_buffered = false;

int layer2getc()
{
    if (is_last_char_buffered) {
        is_last_char_buffered = false;
    } else {
        last_char = fgetc(f);
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

static uint8_t keys;

- (uint8_t)keyMaskFromEvent:(NSEvent *)event
{
    switch ([event.characters characterAtIndex:0]) {
        case 0xF703: // right
            return 1;
        case 0xF702: // left
            return 2;
        case 0xF700: // up
            return 4;
        case 0xF701: // down
            return 8;
        case 'a': // a
            return 16;
        case 's': // b
            return 32;
        case '\'': // select
            return 64;
        case '\r': // start
            return 128;
        default:
            return 0;
    }
}

- (void)keyDown:(NSEvent *)event
{
    keys |= [self keyMaskFromEvent:event];
//    gb->set_buttons(keys);
}

- (void)keyUp:(NSEvent *)event
{
    keys &= ~[self keyMaskFromEvent:event];
//    gb->set_buttons(keys);
}

@end

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;
@property (nonatomic) NSTimeInterval nextFrameTime;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    int scale = 1;
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
//    char *filename = "/tmp/1.cept";
//    char *filename = "/Users/mist/Documents/git/bildschirmtext/historic_dumps/PC online 1&1/06MICROS.CPT";
//    char *filename = "/Users/mist/Documents/git/bildschirmtext/historic_dumps/PC online 1&1/03IBM_1.CPT";
    char *filename = "/Users/mist/Documents/git/bildschirmtext/historic_dumps/PC online 1&1/18BAHN.CPT";
//    char *filename = "/Users/mist/Documents/btx/rtx/pages/190912a";
    f = fopen(filename, "r");

    while (!process_BTX_data()) {
//        printf(".");
    }

    f = fopen("/tmp/memimage.bin", "wb");
    fwrite(memimage, 1, MEMIMAGE_SIZE, f);
    fclose(f);

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSTimeInterval timePerFrame = 1.0 / (1024.0 * 1024.0 / 114.0 / 154.0);

        self.nextFrameTime = [NSDate timeIntervalSinceReferenceDate] + timePerFrame;

        for (;;) {
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
