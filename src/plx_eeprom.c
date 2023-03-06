/*-
 * Copyright (c) 2017 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __FreeBSD__
#include <sys/endian.h>
#endif

#ifdef __linux__
#include <stdint.h>
#include <bsd/sys/endian.h>
#endif

#define	roundup2(x, y)	(((x)+((y)-1))&(~((y)-1)))

uint32_t
read4(void *mem, u_int addr)
{

	return (*(volatile uint32_t *)((uint8_t *)mem + addr));
}

void
write4(void *mem, u_int addr, uint32_t val)
{

	*(volatile uint32_t *)((uint8_t *)mem + addr) = val;
}

static uint32_t
plx_eeprom_read(void *mem, u_int addr)
{
	uint32_t val;

	if ((addr > 0x00ffffff) || (addr & 0x3) != 0)
		errx(EX_OSERR, "Incorrect EEPROM address %x", addr);

	/* Write address byte 3. */
	val = read4(mem, 0x26c);
	write4(mem, 0x26c, (val & ~0xff) | (addr >> 24));

	/* Initiate read. */
	addr >>= 2;
	val = read4(mem, 0x260);
	val &= 0xffe00000;
	val |= ((addr & 0x2000) << 7) | (3 << 13) | (addr & 0x1fff);
	write4(mem, 0x260, val);

	/* Wait for completion. */
	val = 100;
	while ((read4(mem, 0x260) & 0x40000) != 0 && val > 0) {
		usleep(100);
		val--;
	}
	if (val == 0)
		errx(EX_OSERR, "EEPROM write timeout");

	/* Read result. */
	val = read4(mem, 0x264);
	return (val);
}

static int
plx_eeprom_write(void *mem, u_int addr, uint32_t data)
{
	int i;
	uint32_t val;

	if ((addr > 0x00ffffff) || (addr & 0x3) != 0)
		errx(EX_OSERR, "Incorrect EEPROM address %x", addr);
	addr >>= 2;

	/* Write address byte 3. */
	val = read4(mem, 0x26c);
	write4(mem, 0x26c, (val & ~0xff) | (addr >> 14));

	/* Enable write. */
	val = read4(mem, 0x260);
	val &= 0xffe00000;
	val |= (6 << 13);
	write4(mem, 0x260, val);

	/* Wait for completion. */
	i = 100;
	while (((val = read4(mem, 0x260)) & 0x40000) != 0 && val > 0) {
		usleep(100);
		i--;
	}
	if (i == 0)
		errx(EX_OSERR, "EEPROM write enable timeout");

	/* Prepare data. */
	write4(mem, 0x264, data);

	/* Initiate write. */
	val = read4(mem, 0x260);
	val &= 0xffe00000;
	val |= ((addr & 0x2000) << 7) | (2 << 13) | (addr & 0x1fff);
	write4(mem, 0x260, val);

	/* Wait for completion. */
	i = 100;
	while (((val = read4(mem, 0x260)) & 0x40000) != 0 && val > 0) {
		usleep(100);
		i--;
	}
	if (i == 0)
		errx(EX_OSERR, "EEPROM write timeout");
	return (0);
}

static void
plx_eeprom_read_raw(void *mem, int dfd)
{
	int i, n, error;
	uint32_t val;

	val = plx_eeprom_read(mem, 0);
	write(dfd, &val, sizeof(val));
	n = 4;
	if ((val & 0xff) == 0x5a)
		n += (val >> 16) + ((n & 0x8000) ? 4 : 0);
	n = roundup2(n, 4);
	for (i = 4; i < n; i += 4) {
		val = plx_eeprom_read(mem, i);
		write(dfd, &val, sizeof(val));
	}
}

static void
plx_eeprom_write_raw(void *mem, int dfd)
{
	int i;
	uint32_t val, val2;

	/* Magic specific to One Stop Systems */
	write4(mem, 0x260, 0x0000A000);
	usleep(1000);
	write4(mem, 0x260, 0x00002000);
	usleep(1000);

	i = 0;
	val = 0;
	while (read(dfd, &val, 4) > 0) {
		plx_eeprom_write(mem, i, val);
		val2 = plx_eeprom_read(mem, i);
		if (val2 != val)
			errx(EX_OSERR, "EEPROM write error at %x %08x != %08x",
			    i, val2, val);
		i += 4;
		val = 0;
	}
	printf("%d dwords written\n", i / 4);
}

static void
plx_config_print(char *buf, int n)
{
	int i, r, port, error;
	uint32_t val;

	printf("   #\tPort\tReg\tValue\n");
	printf("--------------------------------\n");
	n /= 6;
	for (i = 0; i < n; i++) {
		printf(" %3d\t", i);
		val = le16dec(&buf[i * 6]);
		port = val >> 10;
		if ((port & 0x38) == 0x20)
			printf("A-LUT %d", port & 0x3);
		else if (port == 0x2c)
			printf("DMA RAM");
		else if ((port & 0x38) == 0x28)
			printf("DMA %d", port & 0x7);
		else if ((port & 0x38) == 0x30)
			printf("S%dP0", port & 0x7);
		else if ((port & 0x38) == 0x38)
			printf("NT%d %c", (port >> 1) & 0x1,
			    (port & 0x1) ? 'V' : 'L');
		else
			printf("%d", port);
		printf("\t%03x\t%08x\n",
		    (val & 0x3ff) * 4, le32dec(&buf[i * 6] + 2));
	}
}

static void
plx_eeprom_print(void *mem)
{
	int i, n, r;
	uint32_t *bufdw;
	uint32_t val;

	val = plx_eeprom_read(mem, 0);
	if ((val & 0xff) != 0x5a)
		return;
	n = (val >> 16);
	r = roundup2(n, 4);
	bufdw = malloc(r);
	for (i = 0; i < r; i += 4)
		bufdw[i / 4] = plx_eeprom_read(mem, 4 + i);
	plx_config_print((char *)bufdw, n);
	free(bufdw);
}

static void
plx_file_print(int fd)
{
	int i, n;
	uint32_t *bufdw;
	uint32_t val;
	struct stat st;

	if (fstat(fd, &st) < 0)
		err(EX_OSERR, "Can't fstat() file");
	n = st.st_size;
	bufdw = malloc(n);
	if (read(fd, bufdw, n) < n)
		err(EX_OSERR, "Can't read file");
	val = bufdw[0];
	if ((val & 0xff) != 0x5a) {
		err(EX_OSERR, "Wrong signature");
		return;
	}
	if ((val >> 16) + 4 > n) {
		err(EX_OSERR, "Truncated image");
		return;
	}
	n = (val >> 16);
	plx_config_print((char *)(bufdw + 1), n);
	free(bufdw);
}

static void
usage(void)
{
	fprintf(stderr, "usage: plx_eeprom -b <base> [-rw] [-f <file>]\n");
	exit (1);
}

int
main(int argc, char **argv)
{
	int fd = -1, dfd = -1, c, r = 0, w = 0;
	uint64_t base = 0;
	uint8_t	*mem = NULL;
	char *fn = NULL;

	while ((c = getopt(argc, argv, "b:f:hrw")) != -1) {
		switch (c) {
		case 'b':
			base = strtoll(optarg, NULL, 0);
			break;
		case 'f':
			fn = optarg;
			break;
		case 'r':
			r = 1;
			break;
		case 'w':
			w = 1;
			break;
		case 'h':
		default:
			usage();
		}
	}

	if (base != 0) {
		fd = open("/dev/mem", O_RDWR);
		if (fd < 0)
			err(EX_OSERR, "Can't open /dev/mem");
		mem = mmap(NULL, 256*1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
		if (mem == MAP_FAILED)
			err(EX_OSERR, "Can't mmap /dev/mem");
	}

	if (fn != NULL) {
		if (strcmp(fn, "-") != 0) {
			dfd = open(fn, r ? (O_WRONLY | O_TRUNC | O_CREAT) : O_RDONLY);
			if (dfd < 0)
				err(EX_OSERR, "Can't open %s", fn);
		} else
			dfd = w ? STDIN_FILENO : STDOUT_FILENO;
	}

	if (r || w) {
		if (mem == NULL) {
			warnx("Base address not specified");
			usage();
		}
		if (dfd < 0) {
			warnx("File name not specified");
			usage();
		}
		if (r)
			plx_eeprom_read_raw(mem, dfd);
		else
			plx_eeprom_write_raw(mem, dfd);
	} else if (mem != NULL) {
		plx_eeprom_print(mem);
	} else if (dfd >= 0) {
		plx_file_print(dfd);
	} else {
		warnx("Neither base address nor file name specified");
		usage();
	}

	if (dfd >= 0)
		close(dfd);
	if (fd >= 0) {
		munmap(mem, 256*1024);
		close(fd);
	}
}
