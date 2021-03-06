/* Copyright (C) 2015-2017 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved. */

#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/io.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/utsname.h>
#include <linux/random.h>
#include <linux/version.h>

__attribute__((noreturn)) static void poweroff(void)
{
	fflush(stdout);
	fflush(stderr);
	reboot(RB_AUTOBOOT);
	sleep(30);
	fprintf(stderr, "\x1b[37m\x1b[41m\x1b[1mFailed to power off!!!\x1b[0m\n");
	exit(1);
}

static void panic(const char *what)
{
	fprintf(stderr, "\n\n\x1b[37m\x1b[41m\x1b[1mSOMETHING WENT HORRIBLY WRONG\x1b[0m\n\n    \x1b[31m\x1b[1m%s: %s\x1b[0m\n\n\x1b[37m\x1b[44m\x1b[1mPower off...\x1b[0m\n\n", what, strerror(errno));
	poweroff();
}

#define pretty_message(msg) puts("\x1b[32m\x1b[1m" msg "\x1b[0m")

static void print_banner(const struct utsname *utsname)
{
	int len = strlen("    WireGuard Test Suite on      ") + strlen(utsname->sysname) + strlen(utsname->release);
	printf("\x1b[45m\x1b[33m\x1b[1m%*.s\x1b[0m\n\x1b[45m\x1b[33m\x1b[1m    WireGuard Test Suite on %s %s    \x1b[0m\n\x1b[45m\x1b[33m\x1b[1m%*.s\x1b[0m\n\n", len, "", utsname->sysname, utsname->release, len, "");
}

static void seed_rng(void)
{
	int fd1, fd2, i;
	struct {
		int entropy_count;
		int buffer_size;
		unsigned char buffer[128];
	} entropy = {
		.entropy_count = 128,
		.buffer_size = 128
	};
	pretty_message("[+] Ensuring RNG entropy...");
	fd1 = open("/dev/hwrng", O_RDONLY);
	fd2 = open("/dev/urandom", O_WRONLY);
	if (fd1 < 0 || fd2 < 0)
		panic("open(hwrng,urandom)");
	for (i = 0; i < 4096; ++i) {
		if (read(fd1, entropy.buffer, 128) != 128)
			panic("read(hwrng)");
		if (ioctl(fd2, RNDADDENTROPY, &entropy) < 0)
			panic("ioctl(urandom)");
	}
	close(fd1);
	close(fd2);
}

static void mount_filesystems(void)
{
	pretty_message("[+] Mounting filesystems...");
	mkdir("/dev", 0755);
	mkdir("/proc", 0755);
	mkdir("/sys", 0755);
	mkdir("/tmp", 0755);
	mkdir("/run", 0755);
	mkdir("/var", 0755);
	if (mount("none", "/dev", "devtmpfs", 0, NULL))
		panic("devtmpfs mount");
	if (mount("none", "/proc", "proc", 0, NULL))
		panic("procfs mount");
	if (mount("none", "/sys", "sysfs", 0, NULL))
		panic("sysfs mount");
	if (mount("none", "/tmp", "tmpfs", 0, NULL))
		panic("tmpfs mount");
	if (mount("none", "/run", "tmpfs", 0, NULL))
		panic("tmpfs mount");
	if (symlink("/run", "/var/run"))
		panic("run symlink");
	if (symlink("/proc/self/fd", "/dev/fd"))
		panic("fd symlink");
}

static void enable_logging(void)
{
	int fd;
	pretty_message("[+] Enabling logging...");
	fd = open("/proc/sys/kernel/printk", O_WRONLY);
	if (fd < 0)
		panic("open(printk)");
	if (write(fd, "9\n", 2) != 2)
		panic("write(printk)");
	close(fd);
}

static void kmod_selftests(void)
{
	FILE *file;
	char line[2048], *start, *pass;
	bool success = true;
	pretty_message("[+] Module self-tests:");
	file = fopen("/proc/kmsg", "r");
	if (!file)
		panic("fopen(kmsg)");
	if (fcntl(fileno(file), F_SETFL, O_NONBLOCK) < 0)
		panic("fcntl(kmsg, nonblock)");
	while (fgets(line, sizeof(line), file)) {
		start = strstr(line, "wireguard: ");
		if (!start)
			continue;
		start += 11;
		*strchrnul(start, '\n') = '\0';
		if (strstr(start, "www.wireguard.com"))
			break;
		pass = strstr(start, ": pass");
		if (!pass || pass[6] != '\0') {
			success = false;
			printf(" \x1b[31m*  %s\x1b[0m\n", start);
		} else
			printf(" \x1b[32m*  %s\x1b[0m\n", start);
	}
	fclose(file);
	if (!success) {
		puts("\x1b[31m\x1b[1m[-] Tests failed! :-(\x1b[0m");
		poweroff();
	}
}

static void launch_tests(void)
{
	int status, fd;
	pid_t pid;
	pretty_message("[+] Launching tests...");
	pid = fork();
	if (pid == -1)
		panic("fork");
	else if (pid == 0) {
		execl("/init.sh", "init", NULL);
		panic("exec");
	}
	if (waitpid(pid, &status, 0) < 0)
		panic("waitpid");
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		pretty_message("[+] Tests successful! :-)");
		fd = open("/dev/vport1p1", O_WRONLY);
		if (fd < 0)
			panic("open(vport1p1)");
		if (write(fd, "success\n", 8) != 8)
			panic("write(success)");
		close(fd);
	} else
		puts("\x1b[31m\x1b[1m[-] Tests failed! :-(\x1b[0m");
}

static bool linux_4_8_or_higher(const struct utsname *utsname)
{
	unsigned int maj, min, rel;
	if (strcmp(utsname->sysname, "Linux"))
		return false;
	if (sscanf(utsname->release, "%u.%u.%u", &maj, &min, &rel) != 3)
		return false;
	return KERNEL_VERSION(maj, min, rel) >= KERNEL_VERSION(4, 8, 0);
}

static void ensure_console(void)
{
	for (unsigned int i = 0; i < 1000; ++i) {
		int fd = open("/dev/console", O_RDWR);
		if (fd < 0) {
			usleep(50000);
			continue;
		}
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
		if (write(1, "\0\0\0\0\n", 5) == 5)
			return;
	}
	panic("Unable to open console device");
}

int main(int argc, char *argv[])
{
	struct utsname utsname;

	ensure_console();
	if (uname(&utsname) < 0)
		panic("uname");
	print_banner(&utsname);
	mount_filesystems();
	kmod_selftests();
	if (!linux_4_8_or_higher(&utsname))
		seed_rng();
	enable_logging();
	launch_tests();
	poweroff();
	return 1;
}
