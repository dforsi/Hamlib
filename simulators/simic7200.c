// simicom will show the pts port to use for rigctl on Unix
// using virtual serial ports on Windows is to be developed yet
// Needs a lot of improvement to work on all Icoms
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#if 0
struct ip_mreq
{
    int dummy;
};
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include "hamlib/rig.h"
#include "../src/misc.h"
#include <termios.h>
#include <unistd.h>


#define BUFSIZE 256
#define X25

int civ_731_mode = 0;
vfo_t current_vfo = RIG_VFO_A;
int split = 0;

// we make B different from A to ensure we see a difference at startup
float freqA = 14074000;
float freqB = 14074500;
mode_t modeA = RIG_MODE_PKTUSB;
mode_t modeB = RIG_MODE_PKTUSB;
int datamodeA = 0;
int datamodeB = 0;
pbwidth_t widthA = 0;
pbwidth_t widthB = 1;
ant_t ant_curr = 0;
int ant_option = 0;
int ptt = 0;
int satmode = 0;
int agc_time = 1;
int ovf_status = 0;
int powerstat = 1;
int keyspd = 85;
int filter_width = 38;
int preamp = 0;
int attenuator = 0;
int autonotch = 0;
int aflevel = 128;
int noiseblanker = 0;
int manualnotch = 0;
int mnf = 0;
int noisereduction = 0;
int speechcompressor = 0;
int agc = 0;
int vox = 0;

void dumphex(const unsigned char *buf, int n)
{
    for (int i = 0; i < n; ++i) { printf("%02x ", buf[i]); }

    printf("\n");
}

int
frameGet(int fd, unsigned char *buf)
{
    int i = 0;
    memset(buf, 0, BUFSIZE);
    unsigned char c;

again:

    while (read(fd, &c, 1) > 0)
    {
        buf[i++] = c;
        //printf("i=%d, c=0x%02x\n",i,c);

        if (c == 0xfd)
        {
            char mytime[256];
            date_strget(mytime, sizeof(mytime), 1);
            printf("%s: %s ", mytime, __func__); dumphex(buf, i);
#if 0
            // echo
            n = write(fd, buf, i);

            if (n != i) { printf("%s: error on write: %s\n", __func__, strerror(errno)); }

#endif
            return i;
        }

        if (i > 2 && c == 0xfe)
        {
            printf("Turning power on due to 0xfe string\n");
            powerstat = 1;
            int j;

            for (j = i; j < 175; ++j)
            {
                if (read(fd, &c, 1) < 0) { break; }
            }

            i = 0;
            goto again;
        }
    }

    printf("Error %s\n", strerror(errno));

    return 0;
}

void frameParse(int fd, unsigned char *frame, int len)
{
    double freq;
    int n = 0;

    if (len == 0)
    {
        printf("%s: len==0\n", __func__);
        return;
    }

    dumphex(frame, len);

    if (frame[0] != 0xfe && frame[1] != 0xfe)
    {
        printf("expected fe fe, got ");
        dumphex(frame, len);
        return;
    }

    // reverse the addressing
    frame[3] = frame[2];
    frame[2] = 0xe0;

    switch (frame[4])
    {
    case 0x03:

        //from_bcd(frameackbuf[2], (civ_731_mode ? 4 : 5) * 2);
        if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN)
        {
            printf("get_freqA\n");
            to_bcd(&frame[5], (long long)freqA, (civ_731_mode ? 4 : 5) * 2);
        }
        else
        {
            printf("get_freqB\n");
            to_bcd(&frame[5], (long long)freqB, (civ_731_mode ? 4 : 5) * 2);
        }

        frame[10] = 0xfd;

        if (powerstat)
        {
            n = write(fd, frame, 11);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
        }

        break;

    case 0x04:
        if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN)
        {
            printf("get_modeA\n");
            frame[5] = modeA;
            frame[6] = widthA;
        }
        else
        {
            printf("get_modeB\n");
            frame[5] = modeB;
            frame[6] = widthB;
        }

        frame[7] = 0xfd;
        n = write(fd, frame, 8);

        if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

        break;

    case 0x05:
        freq = from_bcd(&frame[5], (civ_731_mode ? 4 : 5) * 2);
        printf("set_freq to %.0f\n", freq);

        if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN) { freqA = freq; }
        else { freqB = freq; }

        frame[4] = 0xfb;
        frame[5] = 0xfd;
        n = write(fd, frame, 6);

        if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

        break;

    case 0x06:
        if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN) { modeA = frame[6]; }
        else { modeB = frame[6]; }

        frame[4] = 0xfb;
        frame[5] = 0xfd;
        n = write(fd, frame, 6);

        if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

        break;

    case 0x07:

        switch (frame[5])
        {
        case 0x00: current_vfo = RIG_VFO_A; break;

        case 0x01: current_vfo = RIG_VFO_B; break;

        case 0xa0: current_vfo = freq = freqA; freqA = freqB; freqB = freq; break;

        case 0xb0: current_vfo = RIG_VFO_MAIN; break;

        case 0xd0: current_vfo = RIG_VFO_MAIN; break;

        case 0xd1: current_vfo = RIG_VFO_SUB; break;
        }

        printf("set_vfo to %s\n", rig_strvfo(current_vfo));

        frame[4] = 0xfb;
        frame[5] = 0xfd;
        n = write(fd, frame, 6);

        if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

        break;

    case 0x0f:
        if (frame[5] == 0) { split = 0; }
        else if (frame[5] == 1) { split = 1; }
        else { frame[6] = split; }

        if (frame[5] == 0xfd)
        {
            printf("get split %d\n", 1);
            frame[7] = 0xfd;
            n = write(fd, frame, 8);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
        }
        else
        {
            printf("set split %d\n", 1);
            frame[4] = 0xfb;
            frame[5] = 0xfd;
            n = write(fd, frame, 6);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
        }

        break;

    case 0x11:
        if (frame[6] == 0xfd)
        {
            frame[6] = attenuator;
            frame[7] = 0xfd;
            n = write(fd, frame, 8);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
        }
        else
        {
            attenuator = frame[6];
            frame[4] = 0xfb;
            frame[5] = 0xfd;
            n = write(fd, frame, 6);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
        }

        break;

    case 0x12: // we're simulating the 3-byte version -- not the 2-byte
        if (frame[5] != 0xfd)
        {
            printf("Set ant %d\n", -1);
            ant_curr = frame[5];
            ant_option = frame[6];
            dump_hex(frame, 8);
        }
        else
        {
            printf("Get ant\n");
        }

        frame[5] = ant_curr;
        frame[6] = ant_option;
        frame[7] = 0xfd;
        printf("write 8 bytes\n");
        dump_hex(frame, 8);
        n = write(fd, frame, 8);

        if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

        break;

    case 0x14:
        switch (frame[5])
        {
            static int power_level = 0;

        case 0x01:
            if (frame[6] == 0xfd)
            {
                frame[6] = aflevel;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                aflevel = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x07:
        case 0x08:
            if (frame[6] != 0xfd)
            {
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                dumphex(frame, 6);
                hl_usleep(10 * 1000);
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

                printf("ACK x14 x08\n");
            }
            else
            {
                to_bcd(&frame[6], (long long)128, 2);
                frame[8] = 0xfd;
                dumphex(frame, 9);
                n = write(fd, frame, 9);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

                printf("SEND x14 x08\n");
            }

            break;

        case 0x0c:
            dumphex(frame, 10);
            printf("subcmd=0x0c #1\n");

            if (frame[6] != 0xfd) // then we have data
            {
                printf("subcmd=0x0c #1\n");
                keyspd = from_bcd(&frame[6], 2);
                frame[6] = 0xfb;
                n = write(fd, frame, 7);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                printf("subcmd=0x0c #1\n");
                to_bcd(&frame[6], keyspd, 2);
                frame[8] = 0xfd;
                n = write(fd, frame, 9);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x0a:
            printf("Using power level %d\n", power_level);
            power_level += 10;

            if (power_level > 250) { power_level = 0; }

            to_bcd(&frame[6], (long long)power_level, 2);
            frame[8] = 0xfd;
            n = write(fd, frame, 9);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

            break;
        }

    case 0x0d:
        if (frame[6] == 0xfd)
        {
            frame[6] = mnf;
            frame[7] = 0xfd;
            n = write(fd, frame, 8);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
        }
        else
        {
            mnf = frame[6];
            frame[4] = 0xfb;
            frame[5] = 0xfd;
            n = write(fd, frame, 6);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
        }

        break;

        break;

    case 0x15:
        switch (frame[5])
        {
            static int meter_level = 0;

        case 0x07:
            frame[6] = ovf_status;
            frame[7] = 0xfd;
            n = write(fd, frame, 8);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

            ovf_status = ovf_status == 0 ? 1 : 0;
            break;

        case 0x11:
            printf("Using meter level %d\n", meter_level);
            meter_level += 10;

            if (meter_level > 250) { meter_level = 0; }

            to_bcd(&frame[6], (long long)meter_level, 2);
            frame[8] = 0xfd;
            n = write(fd, frame, 9);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

            break;
        }

    case 0x16:
        switch (frame[5])
        {
        case 0x02:
            if (frame[6] == 0xfd)
            {
                frame[6] = preamp;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                preamp = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x12:
            if (frame[6] == 0xfd)
            {
                frame[6] = agc;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                agc = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x22:
            if (frame[6] == 0xfd)
            {
                frame[6] = noiseblanker;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                noiseblanker = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x40:
            if (frame[6] == 0xfd)
            {
                frame[6] = noisereduction;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                noisereduction = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x41:
            if (frame[6] == 0xfd)
            {
                frame[6] = autonotch;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                autonotch = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x44:
            if (frame[6] == 0xfd)
            {
                frame[6] = speechcompressor;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                speechcompressor = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x46:
            if (frame[6] == 0xfd)
            {
                frame[6] = vox;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                vox = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x48:
            if (frame[6] == 0xfd)
            {
                frame[6] = manualnotch;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                manualnotch = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x5a:
            if (frame[6] == 0xfe)
            {
                satmode = frame[6];
            }
            else
            {
                frame[6] = satmode;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;
        }

        break;

    case 0x19: // miscellaneous things
        frame[5] = 0x94;
        frame[6] = 0xfd;
        n = write(fd, frame, 7);

        if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

        break;

    case 0x1a: // miscellaneous things
        switch (frame[5])
        {
        case 0x03:  // width
            if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN) { frame[6] = widthA; }
            else { frame[6] = widthB; }

            frame[7] = 0xfd;
            n = write(fd, frame, 8);

            if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

            break;

        case 0x02: // filter width
            printf("frame[6]==x%02x, frame[7]=0%02x\n", frame[6], frame[7]);

            if (frame[6] == 0xfd)   // the we are reading
            {
                frame[6] = filter_width;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                printf("FILTER_WIDTH RESPONSE******************************");
                filter_width = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x04: // AGC TIME
            printf("frame[6]==x%02x, frame[7]=0%02x\n", frame[6], frame[7]);

            if (frame[6] == 0xfd)   // the we are reading
            {
                frame[6] = agc_time;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                printf("AGC_TIME RESPONSE******************************");
                agc_time = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        case 0x06: // Data mode
            if (frame[6] == 0xfd) // then we're replying with mode
            {
                frame[6] = datamodeA;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                datamodeA = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        }

        break;

    case 0x1c:
        switch (frame[5])
        {
        case 0:
            if (frame[6] == 0xfd)
            {
                frame[6] = ptt;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }
            else
            {
                ptt = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);

                if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }
            }

            break;

        }

        break;


    case 0x25:
        printf("x25 send nak\n");
        frame[4] = 0xfa;
        frame[5] = 0xfd;
        n = write(fd, frame, 6);

        if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

        break;

    case 0x26:
        printf("x26 send nak\n");
        frame[4] = 0xfa;
        frame[5] = 0xfd;
        n = write(fd, frame, 6);

        if (n <= 0) { fprintf(stderr, "%s(%d) write error %s\n", __func__, __LINE__, strerror(errno)); }

        break;

    default: printf("cmd 0x%02x unknown\n", frame[4]);
    }

    if (n == 0) { printf("Write failed=%s\n", strerror(errno)); }

// don't care about the rig type yet

}

#if defined(WIN32) || defined(_WIN32)
int openPort(char *comport) // doesn't matter for using pts devices
{
    int fd;
    fd = open(comport, O_RDWR);

    if (fd < 0)
    {
        perror(comport);
    }

    return fd;
}

#else
int openPort(char *comport) // doesn't matter for using pts devices
{
    int fd = posix_openpt(O_RDWR);
    char *name = ptsname(fd);

    if (name == NULL)
    {
        perror("ptsname");
        return -1;
    }

    printf("name=%s\n", name);

    if (fd == -1 || grantpt(fd) == -1 || unlockpt(fd) == -1)
    {
        perror("posix_openpt");
        return -1;
    }

    return fd;
}
#endif

void rigStatus()
{
    char vfoa = current_vfo == RIG_VFO_A ? '*' : ' ';
    char vfob = current_vfo == RIG_VFO_B ? '*' : ' ';
    printf("%cVFOA: mode=%d datamode=%d width=%ld freq=%.0f\n", vfoa, modeA,
           datamodeA,
           widthA,
           freqA);
    printf("%cVFOB: mode=%d datamode=%d width=%ld freq=%.0f\n", vfob, modeB,
           datamodeB,
           widthB,
           freqB);
}

int main(int argc, char **argv)
{
    unsigned char buf[256];
    int fd = openPort(argv[1]);

    printf("%s: %s\n", argv[0], rig_version());
    printf("x25/x26 command rejected\n");
#if defined(WIN32) || defined(_WIN32)

    if (argc != 2)
    {
        printf("Missing comport argument\n");
        printf("%s [comport]\n", argv[0]);
        exit(1);
    }

#endif

    while (1)
    {
        int len = frameGet(fd, buf);

        if (len <= 0)
        {
            close(fd);
            fd = openPort(argv[1]);
        }

        if (powerstat)
        {
            frameParse(fd, buf, len);
        }
        else
        {
            hl_usleep(1000 * 1000);
        }

        rigStatus();
    }

    return 0;
}
