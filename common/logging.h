#ifndef LOGGING_H_INCLUDED
#define LOGGING_H_INCLUDED

void info(const char *fmt, ...);
void warn(const char *fmt, ...);
void error(const char *fmt, ...);
void fatal(const char *fmt, ...);

#endif /* ndef LOGGING_H_INCLUDED */
