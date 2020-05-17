extern int tia;    /* 1: white-on-black mode, no attributes */
extern int reveal; /* 1: show hidden content */
extern int dirty;  /* screen change that requires a redraw */

extern void init_layer6(void);
extern int process_BTX_data(void);
extern void redraw_screen_rect(int x1, int y1, int x2, int y2);

extern int get_screen_character(int x, int y);
