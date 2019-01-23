extern void init_xfont(void);
extern void default_colors(void);
extern void xputc(int c, int set, int x, int y, int xdouble, int ydouble, int underline, int diacrit, int fg, int bg);
extern void xcursor(int x, int y);
extern void define_fullrow_bg(int row, int index);
extern void free_DRCS(void);
extern void define_raw_DRC(int c, char *data, int bits);
extern void define_color(unsigned int index, unsigned int r, unsigned int g, unsigned int b);
extern void define_DCLUT(int entry, int index);
