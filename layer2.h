extern void layer2_connect(void);
extern void layer2_connect2(const char *, const int);
extern int layer2_getc(void);
extern void layer2_ungetc(void);
extern void layer2_write(const unsigned char *s, unsigned int len);
extern int layer2_write_readbuffer(uint8_t c);
extern int layer2_eof();
