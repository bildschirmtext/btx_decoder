extern unsigned char *memimage;


extern void init_xfont(void);
extern void default_colors(void);
extern void xputc(int c, int set, int x, int y, int xdouble, int ydouble, int underline, int diacrit, int fg, int bg, int fontheight, int rows);
extern void xcursor(int x, int y, int fontheight);
extern void define_fullrow_bg(int row, int index);
extern void free_DRCS(void);
extern void define_raw_DRC(int c, char *data, int bits);
extern void define_color(unsigned int index, unsigned int r, unsigned int g, unsigned int b);
extern void define_DCLUT(int entry, int index);
extern void get_column_colour(int column, int *r, int *g, int *b);
