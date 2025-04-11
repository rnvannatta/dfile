#ifndef D_FAST_FLOAT_H
#define D_FAST_FLOAT_H
#ifdef __cplusplus
extern "C" {
#endif
char const * ff_from_chars(char const * begin, char const * end, double * out);
char const * ff_from_charsf(char const * begin, char const * end, float * out);
#ifdef __cplusplus
}
#endif
#endif
