/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 18.07.2017
 *
 * General unix includes
 *
 */

// TOP

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <alloca.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#if defined(USE_LOG)
# include <stdio.h>
#endif

// BOTTOM
