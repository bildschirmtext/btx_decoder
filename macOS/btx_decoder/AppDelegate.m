//
//  AppDelegate.m
//  btx_decoder
//

#import "AppDelegate.h"
#import <QuartzCore/QuartzCore.h>

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


static size_t _getBytesCallback(void *info, void *buffer, off_t position, size_t count) {
    static const uint8_t colors[4] = { 255, 170, 85, 0 };
    uint8_t *target = (uint8_t *)buffer;
    uint8_t *source = ((uint8_t *)info) + position;
    uint8_t *sourceEnd = source + count;
    while (source < sourceEnd) {
        *(target++) = colors[*(source)];
        source++;
    }
    return count;
}

static void _releaseInfo(void *info) {
//    free(info);
}

@implementation View

- (CGImageRef)createGameBoyScreenCGImageRef {
    CGDataProviderDirectCallbacks callbacks;
    callbacks.version = 0;
    callbacks.getBytePointer = NULL;
    callbacks.releaseBytePointer = NULL;
    callbacks.getBytesAtPosition = _getBytesCallback;
    callbacks.releaseInfo = _releaseInfo;

    size_t pictureSize = MEMIMAGE_SIZE;
    uint8_t *pictureCopy = memimage;

    CGDataProviderRef provider = CGDataProviderCreateDirect(pictureCopy, pictureSize, &callbacks);
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
    int scale = 2;
    [self.window setFrame:CGRectMake(0, 1000, 160 * scale, 144 * scale + 22) display:YES];

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
    init_layer6();
    f = fopen("/tmp/1.cept", "r");

    while (!process_BTX_data()) {
//        printf(".");
    }


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
