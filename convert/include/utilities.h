#ifndef __CALI_UTILITIES__
#define __CALI_UTILITIES__

#include <sys/stat.h>

// color output
#define RESET     "\033[0m"
#define BOLD      "\033[1m"
#define DIM       "\033[2m"
#define UNDERLINE "\033[4m"
#define BLINK     "\033[5m"
#define INVERSE   "\033[7m"
#define BLACK     "\033[30;20m"
#define RED       "\033[31;20m"
#define GREEN     "\033[32;20m"
#define YELLOW    "\033[33;20m"
#define BLUE      "\033[34;20m"
#define MAGENTA   "\033[35;20m"
#define CYAN      "\033[36;20m"
#define WHITE     "\033[37;20m"
#define BOLDBLACK     "\033[1m\033[30m"
#define BOLDRED       "\033[1m\033[31m"
#define BOLDGREEN     "\033[1m\033[32m"
#define BOLDYELLOW    "\033[1m\033[33m"
#define BOLDBLUE      "\033[1m\033[34m"
#define BOLDMAGENTA   "\033[1m\033[35m"
#define BOLDCYAN      "\033[1m\033[36m"
#define BOLDWHITE     "\033[1m\033[37m"

#define DEBUG   "(" << __FILE__ << ":" << __LINE__ << ") " << BLUE	 << "DEBUG"   << RESET << " - "
#define INFO    "(" << __FILE__ << ":" << __LINE__ << ") " << GREEN 	 << "INFO"    << RESET << " - "
#define WARNING "(" << __FILE__ << ":" << __LINE__ << ") " << YELLOW	 << "WARNING" << RESET << " - "
#define ALERT   "(" << __FILE__ << ":" << __LINE__ << ") " << BOLDYELLOW << "ALERT"   << RESET << " - "
#define ERROR   "(" << __FILE__ << ":" << __LINE__ << ") " << RED	 << "ERROR"   << RESET << " - "
#define FATAL   "(" << __FILE__ << ":" << __LINE__ << ") " << BOLDRED	 << "FATAL"   << RESET << " - "

/*
extern void CERR(const char *format, ...) __attribute__((format(printf, 1, 2)));
extern void CERR(const char *format, ...) {
    printf(format, ...);
}
*/

bool fileExists(const char* fname)
{
    struct stat buffer;
    return stat(fname, &buffer) == 0 ? true : false;
}
bool dirExists(const char* dname)
{
    struct stat buffer;
    return stat(dname, &buffer) == 0 ? ((buffer.st_mode & S_IFDIR) != 0) : false;
}
#endif
