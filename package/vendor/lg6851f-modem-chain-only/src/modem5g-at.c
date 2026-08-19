// SPDX-License-Identifier: GPL-2.0-only
/* Bounded client for the stock atcid loopback AT port. */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ATCID_PORT 17171
#define AT_TIMEOUT_MS 8000
#define SMS_TIMEOUT_MS 18000
#define LOCK_WAIT_MS 1500
#define CONTROL_LOCK "/var/lock/modem5g-at.lock"
#define FAILURE_MARKER "/tmp/modem5g-at.failed"
#define CIRCUIT_BREAK_SECONDS 30

static bool circuit_is_open(void)
{
	struct stat st;
	time_t now = time(NULL);

	return !stat(FAILURE_MARKER, &st) && now >= st.st_mtime &&
	       now - st.st_mtime < CIRCUIT_BREAK_SECONDS;
}

static void mark_channel_failed(void)
{
	int fd = open(FAILURE_MARKER, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC,
		      0600);

	if (fd >= 0)
		close(fd);
}

static int lock_control_channel(void)
{
	struct timespec delay = { .tv_sec = 0, .tv_nsec = 100000000 };
	int waited = 0;
	int fd;

	fd = open(CONTROL_LOCK, O_CREAT | O_CLOEXEC | O_RDWR, 0600);
	if (fd < 0)
		return -1;

	while (flock(fd, LOCK_EX | LOCK_NB)) {
		if (errno != EWOULDBLOCK && errno != EAGAIN) {
			close(fd);
			return -1;
		}
		if (waited >= LOCK_WAIT_MS) {
			fprintf(stderr, "Modem command channel is busy; retry shortly\n");
			close(fd);
			errno = EBUSY;
			return -1;
		}
		nanosleep(&delay, NULL);
		waited += 100;
	}

	return fd;
}

static int write_all(int fd, const void *data, size_t len)
{
	const char *p = data;

	while (len) {
		ssize_t n = write(fd, p, len);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		p += n;
		len -= n;
	}
	return 0;
}

static bool terminal_response(const char *s)
{
	return strstr(s, "\r\nOK") || strstr(s, "\r\nERROR") ||
		strstr(s, "+CME ERROR:") || strstr(s, "+CMS ERROR:");
}

int main(int argc, char **argv)
{
	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_port = htons(ATCID_PORT),
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	struct pollfd pfd = { .events = POLLIN };
	char output[16384] = { 0 };
	size_t used = 0;
	bool body_sent = argc == 2;
	int fd, lockfd, ret;
	int io_timeout_ms;

	if (argc < 2 || argc > 3 || strlen(argv[1]) > 512 ||
	    (argc == 3 && strlen(argv[2]) > 160))
		return 2;
	io_timeout_ms = argc == 3 ? SMS_TIMEOUT_MS : AT_TIMEOUT_MS;

	if (circuit_is_open()) {
		fprintf(stderr, "Modem AT channel is temporarily unavailable; retry shortly\n");
		return 75;
	}

	/* atcid is a single modem-control channel.  LuCI navigation can leave an
	 * earlier RPC running while the next page starts another one.  Never let
	 * those requests overlap: a short busy response is preferable to piling
	 * up synchronous rpcd workers until the whole web interface appears hung. */
	lockfd = lock_control_channel();
	if (lockfd < 0)
		return errno == EBUSY ? 75 : 3;

	fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0 || connect(fd, (struct sockaddr *)&address, sizeof(address))) {
		perror("atcid connect");
		mark_channel_failed();
		close(lockfd);
		return 3;
	}
	pfd.fd = fd;

	/* Stock atcid emits a connection prompt.  Drain it so it cannot be
	 * mistaken for the CMGS body prompt. */
	if (poll(&pfd, 1, 1000) > 0)
		(void)read(fd, output, sizeof(output));
	memset(output, 0, sizeof(output));

	if (write_all(fd, argv[1], strlen(argv[1])) || write_all(fd, "\r", 1)) {
		perror("atcid write");
		close(fd);
		return 4;
	}

	for (;;) {
		ret = poll(&pfd, 1, io_timeout_ms);
		if (ret <= 0) {
			fprintf(stderr, ret ? "atcid poll error\n" : "AT command timed out\n");
			/* Leave atcid's message-input state clean.  The stock TCP queue
			 * only dispatches records ending in CR/LF, including ESC. */
			if (argc == 3)
				(void)write_all(fd, "\x1b\r", 2);
			mark_channel_failed();
			close(fd);
			return 5;
		}
		ret = read(fd, output + used, sizeof(output) - used - 1);
		if (ret <= 0)
			break;
		used += ret;
		output[used] = '\0';

		if (!body_sent && strchr(output, '>')) {
			/* atcid's 2024 TCP queue fix releases input only on CR/LF.  The
			 * modem payload itself still terminates at Ctrl+Z; the trailing CR
			 * is the local TCP record delimiter that makes atcid dispatch it. */
			if (write_all(fd, argv[2], strlen(argv[2])) ||
			    write_all(fd, "\x1a\r", 2)) {
				(void)write_all(fd, "\x1b\r", 2);
				close(fd);
				return 4;
			}
			body_sent = true;
		}
		if (terminal_response(output))
			break;
		if (used == sizeof(output) - 1) {
			fprintf(stderr, "AT response too large\n");
			close(fd);
			return 6;
		}
	}

	fwrite(output, 1, used, stdout);
	close(fd);
	close(lockfd);
	if (terminal_response(output))
		unlink(FAILURE_MARKER);
	return terminal_response(output) && !strstr(output, "ERROR") ? 0 : 7;
}
