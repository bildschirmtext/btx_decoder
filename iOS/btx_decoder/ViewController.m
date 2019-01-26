//
//  ViewController.m
//  btx_decoder
//

#import "ViewController.h"
#import "xfont.h"
#import "layer6.h"
#import "control.h"
#import "layer2.h"

@interface ViewController ()<UITextFieldDelegate>
@property (nonatomic) NSTimeInterval nextFrameTime;
@end

@implementation ViewController
{
	UIView *_btxView;
	UITextField *_textField;
}

- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
	NSLog(@"%lu/%lu '%@' (%lu)", (unsigned long)range.location, (unsigned long)range.length, string, (unsigned long)string.length);
	if (range.location == 0 && range.length == 1 && string.length == 0) {
		// backspace
		layer2_write((unsigned char *)"\b \b", 3);
	} else {
		const char *s = string.UTF8String;
		layer2_write((unsigned char *)s, (unsigned int)string.length);
	}
	return NO;
}

- (void)buttonPressed:(UIButton *)button
{
	NSUInteger i = button.tag;
	unsigned char c;
	if (i) {
		c = TER;
	} else {
		c = INI;
	}
	layer2_write(&c, 1);
}

- (void)viewDidLayoutSubviews
{
	CGRect frame = self.view.bounds;
	frame.origin.y = 50;
	frame.size.height = frame.size.width * 3.0 / 4.0;
	_btxView.frame = frame;

	_textField.frame = CGRectMake(0, CGRectGetMaxY(frame), frame.size.width, 30);
}

- (void)viewDidLoad
{
	[super viewDidLoad];

	self.view.backgroundColor = [UIColor blackColor];

	_btxView = [[UIView alloc] init];
	[self.view addSubview:_btxView];

	const CGFloat kButtonSize = 50;

	const CGFloat kPlaceholderWidth = 100;
	UIView *_inputAccessoryView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, kPlaceholderWidth, kButtonSize)];
	for (NSUInteger i = 0; i < 2; i++) {
		UIButton *button = [[UIButton alloc] init];
		CGRect f;
		f.origin = CGPointMake(0, 0);
		f.size = CGSizeMake(kButtonSize, kButtonSize);
		button.frame = CGRectMake(i * kPlaceholderWidth / 2, 0, kPlaceholderWidth / 2, kButtonSize);
		if (i == 0) {
			button.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleRightMargin;
		} else {
			button.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleLeftMargin;
		}
		[button setTitle:i ? @"#" : @"*" forState:UIControlStateNormal];
		button.backgroundColor = [UIColor grayColor];
		[button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
		button.layer.borderColor = [UIColor lightGrayColor].CGColor;
		button.layer.borderWidth = .5;
		button.tag = i;
		[button addTarget:self action:@selector(buttonPressed:) forControlEvents:UIControlEventTouchUpInside];
		[_inputAccessoryView addSubview:button];
	}

	_textField = [[UITextField alloc] init];
	[self.view addSubview:_textField];
	_textField.text = @" ";
	_textField.delegate = self;
	_textField.autocapitalizationType = UITextAutocapitalizationTypeNone;
	_textField.autocorrectionType = UITextAutocorrectionTypeNo;
	_textField.inputAccessoryView = _inputAccessoryView;
	_textField.alpha = 0;
	[_textField becomeFirstResponder];

    // initialize decoder
    init_xfont();
    init_layer6();

    layer2_connect();

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

					CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, memimage, 480*240*3, NULL);
					CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();

					CGImageRef image = CGImageCreate(480, 240, 8, 24, 480*3, colorspace, kCGBitmapByteOrderDefault | kCGImageAlphaNone, provider, NULL, NO, kCGRenderingIntentDefault);
					CFRelease(colorspace);
					CFRelease(provider);

					dispatch_async(dispatch_get_main_queue(), ^{
						self->_btxView.layer.contents = CFBridgingRelease(image);
					});
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
