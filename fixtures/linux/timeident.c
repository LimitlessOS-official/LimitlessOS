/*
 * Linux persona fixture for the identity and time syscalls added in M201:
 * uname, getuid/getgid/getegid, gettimeofday, time, clock_gettime
 * (CLOCK_REALTIME), and getrandom. Static musl, linked at 0x52000000 like the
 * BusyBox gate binary:
 *   x86_64-linux-musl-gcc -static -O2 -Wl,-Ttext-segment=0x52000000 timeident.c -o TIMEID
 */
#include <stdio.h>
#include <sys/random.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

int main(void)
{
    struct utsname uts;
    struct timeval tv;
    struct timespec ts;
    unsigned char bytes[16];
    unsigned int zeros = 0u;
    unsigned int index;
    time_t now;

    if (uname(&uts) != 0) {
        puts("timeident: uname failed");
        return 1;
    }
    printf("timeident uname %s %s %s %s\n", uts.sysname, uts.nodename, uts.release, uts.machine);
    printf("timeident ids uid %u gid %u egid %u\n", (unsigned)getuid(), (unsigned)getgid(), (unsigned)getegid());

    if (gettimeofday(&tv, NULL) != 0 || clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        puts("timeident: clock failed");
        return 1;
    }
    now = time(NULL);
    printf("timeident time gettimeofday %lld time %lld realtime %lld\n",
        (long long)tv.tv_sec, (long long)now, (long long)ts.tv_sec);

    if (getrandom(bytes, sizeof(bytes), 0) != (ssize_t)sizeof(bytes)) {
        puts("timeident: getrandom failed");
        return 1;
    }
    printf("timeident random ");
    for (index = 0u; index < sizeof(bytes); ++index) {
        printf("%02x", bytes[index]);
        zeros += (bytes[index] == 0u) ? 1u : 0u;
    }
    printf("\n");
    /* 2020-01-01 as a floor: the wall clock must be real time, not seconds since boot. */
    puts((now > 1577836800 && tv.tv_sec > 1577836800 && ts.tv_sec > 1577836800) ? "timeident ok" : "timeident clock-not-wall");
    (void)zeros;
    return 0;
}
