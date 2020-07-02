#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <system/core/toolbox/getevent.h>

#include "main.h"
#include "input.h"

int fd_input, udp_input;
struct input_event *event_key, *event_sync;
uint8_t event_size;
pthread_t input_thread_id;

void *input_thread(void *)
{
    uint8_t buf[8];
    struct input_event e;
    ssize_t received;
    memset(&e, 0, event_size);
    printf("Input thread started\n");
    while (1)
    {
        // TODO: Lock in target ip
        received = recv(fd, &buf, 8, 0);
        if (received > 0)
        {
            e.type = buf[0] << 8;
            e.type |= buf[1];

            e.code = buf[2] << 8;
            e.code |= buf[3];

            e.value = buf[4] << 24;
            e.value |= buf[5] << 16;
            e.value |= buf[6] << 8;
            e.value |= buf[7];

            printf("E: %04X %04X %08X\n", e.type, e.code, e.value);
            // for (int i = 0; i < received; i++)
            // {
            //     printf("%02x ", buf[i]);
            // }
            // printf("\n");
            write(fd_input, &e, event_size);
        }
        else
        {
            printf("IGNORE %d bytes\n", received);
        }
    }
}

void input_setup(uint16_t port)
{
    int version;
    sockaddr_in *listen_addr;
    listen_addr = (sockaddr_in *)calloc(__SOCK_SIZE__, 1);

    event_size = sizeof(input_event);
    event_key = (input_event *)calloc(event_size, 1);
    event_sync = (input_event *)calloc(event_size, 1);
    event_key->type = EV_KEY;

    fd_input = open("/dev/input/event4", O_RDWR);
    if (ioctl(fd_input, EVIOCGVERSION, &version))
        DIE("open input");

    listen_addr->sin_family = AF_INET;
    listen_addr->sin_addr.s_addr = INADDR_ANY;
    listen_addr->sin_port = htons(port);

    if (pthread_create(&input_thread_id, NULL, &input_thread, NULL) != 0)
        DIE("Fail starting input thread");
}

/* Simulating keypress */
int input_press(int key)
{
    // UP
    event_key->code = key;
    event_key->value = 1;

    if (write(fd_input, event_key, event_size) != event_size)
        return 0;

    // Sync
    if (write(fd_input, event_sync, event_size) != event_size)
        return 0;

    // Down
    event_key->value = 0;
    if (write(fd_input, event_key, event_size) != event_size)
        return 0;

    // Sync
    if (write(fd_input, event_sync, event_size) != event_size)
        return 0;

    return 1;
}
