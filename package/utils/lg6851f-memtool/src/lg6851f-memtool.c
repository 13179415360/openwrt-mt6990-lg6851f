#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

static long page_size;

static void die(const char *what)
{
	fprintf(stderr, "ERROR: %s: %s\n", what, strerror(errno));
	exit(EXIT_FAILURE);
}

static uint64_t parse_u64(const char *s)
{
	char *end = NULL;
	errno = 0;
	unsigned long long v = strtoull(s, &end, 0);

	if (errno || !end || *end != '\0') {
		fprintf(stderr, "ERROR: invalid number: %s\n", s);
		exit(EXIT_FAILURE);
	}

	return (uint64_t)v;
}

static uint32_t read32_fd(int fd, uint64_t addr)
{
	uint64_t base = addr & ~((uint64_t)page_size - 1);
	size_t off = (size_t)(addr - base);
	void *map = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
			 MAP_SHARED, fd, (off_t)base);

	if (map == MAP_FAILED)
		die("mmap");

	volatile uint32_t *p = (volatile uint32_t *)((uint8_t *)map + off);
	uint32_t val = *p;

	if (munmap(map, (size_t)page_size) != 0)
		die("munmap");

	return val;
}

static void write32_fd(int fd, uint64_t addr, uint32_t val)
{
	uint64_t base = addr & ~((uint64_t)page_size - 1);
	size_t off = (size_t)(addr - base);
	void *map = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
			 MAP_SHARED, fd, (off_t)base);

	if (map == MAP_FAILED)
		die("mmap");

	volatile uint32_t *p = (volatile uint32_t *)((uint8_t *)map + off);
	*p = val;
	__sync_synchronize();

	if (munmap(map, (size_t)page_size) != 0)
		die("munmap");
}

static void dump_fd(int fd, uint64_t addr, size_t len)
{
	for (size_t i = 0; i < len; i += 16) {
		printf("%08" PRIx64 "  ", addr + i);

		for (size_t j = 0; j < 16; j++) {
			if (i + j < len) {
				uint64_t a = addr + i + j;
				uint64_t base = a & ~((uint64_t)page_size - 1);
				size_t off = (size_t)(a - base);
				void *map = mmap(NULL, (size_t)page_size, PROT_READ,
						 MAP_SHARED, fd, (off_t)base);
				if (map == MAP_FAILED)
					die("mmap");
				uint8_t v = *((volatile uint8_t *)map + off);
				if (munmap(map, (size_t)page_size) != 0)
					die("munmap");
				printf("%02x ", v);
			} else {
				printf("   ");
			}
		}
		putchar('\n');
	}
}

struct reg_item {
	const char *name;
	uint64_t addr;
};

static const struct reg_item regs[] = {
	{"MAC0_MCR",       0x15110100},
	{"MAC1_MCR",       0x15110200},
	{"PCS0_QPHY_PWR",  0x113040ec},
	{"PCS1_QPHY_PWR",  0x113050ec},
	{"GDM0_FWD_CFG",   0x15100500},
	{"GDM1_FWD_CFG",   0x15101500},
	{"GDM0_RX_GBCNT",  0x15101c00},
	{"GDM0_RX_GPCNT",  0x15101c08},
	{"GDM0_RX_OERCNT", 0x15101c0c},
	{"FE_INT_GRP",     0x15100020},
	{"PDMA_RX_BASE",   0x15104100},
	{"PDMA_RX_CNT",    0x15104104},
	{"PDMA_CRX",       0x15104108},
	{"PDMA_DRX",       0x1510410c},
	{"PDMA_GLO_CFG",   0x15104204},
	{"PDMA_RST_IDX",   0x15104208},
	{"PDMA_DELAY_INT", 0x1510420c},
	{"PDMA_INT_STS",   0x15104220},
	{"PDMA_INT_MASK",  0x15104228},
	{"QDMA_QTX_CFG0",  0x15104400},
	{"QDMA_QTX_SCH0",  0x15104404},
	{"QDMA_SEL",       0x151045f0},
	{"QDMA_GLO_CFG",   0x15104604},
	{"QDMA_INT_STS",   0x15104618},
	{"QDMA_INT_MASK",  0x1510461c},
	{"QDMA_INT_GRP1",  0x15104620},
	{"QDMA_INT_GRP2",  0x15104624},
	{"QDMA_CTX",       0x15104700},
	{"QDMA_DTX",       0x15104704},
	{"QDMA_CRX",       0x15104710},
	{"QDMA_DRX",       0x15104714},
};

static void snapshot(int fd)
{
	puts("===== MT6990 Ethernet snapshot =====");

	for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
		uint32_t v = read32_fd(fd, regs[i].addr);
		printf("%-20s 0x%08" PRIx64 " = 0x%08" PRIx32 "\n",
		       regs[i].name, regs[i].addr, v);
	}

	uint32_t rx_base = read32_fd(fd, 0x15104100);

	if (rx_base) {
		printf("\n===== RX ring first 128 bytes @ 0x%08" PRIx32 " =====\n",
		       rx_base);
		dump_fd(fd, rx_base, 128);
	}
}

#define MTK_PHY_IAC             0x15110004ULL
#define PHY_IAC_ACCESS          (1U << 31)
#define PHY_IAC_REG_SHIFT       25
#define PHY_IAC_ADDR_SHIFT      20
#define PHY_IAC_CMD_SHIFT       18
#define PHY_IAC_CMD_WRITE       1U
#define PHY_IAC_CMD_C22_READ    2U
#define PHY_IAC_ST_C22          (1U << 16)

static int mdio_wait_idle(int fd)
{
	for (unsigned int i = 0; i < 100000; i++) {
		if (!(read32_fd(fd, MTK_PHY_IAC) & PHY_IAC_ACCESS))
			return 0;
		usleep(10);
	}

	fprintf(stderr, "ERROR: MDIO controller timeout\n");
	return -1;
}

static uint16_t mdio_read_c22(int fd, unsigned int phy, unsigned int reg)
{
	uint32_t cmd;

	if (mdio_wait_idle(fd) != 0)
		exit(EXIT_FAILURE);

	cmd = PHY_IAC_ACCESS | (reg << PHY_IAC_REG_SHIFT) |
	      (phy << PHY_IAC_ADDR_SHIFT) |
	      (PHY_IAC_CMD_C22_READ << PHY_IAC_CMD_SHIFT) | PHY_IAC_ST_C22;
	write32_fd(fd, MTK_PHY_IAC, cmd);

	if (mdio_wait_idle(fd) != 0)
		exit(EXIT_FAILURE);

	return (uint16_t)read32_fd(fd, MTK_PHY_IAC);
}

static void mdio_write_c22(int fd, unsigned int phy, unsigned int reg,
			   uint16_t val)
{
	uint32_t cmd;

	if (mdio_wait_idle(fd) != 0)
		exit(EXIT_FAILURE);

	cmd = PHY_IAC_ACCESS | (reg << PHY_IAC_REG_SHIFT) |
	      (phy << PHY_IAC_ADDR_SHIFT) |
	      (PHY_IAC_CMD_WRITE << PHY_IAC_CMD_SHIFT) | PHY_IAC_ST_C22 | val;
	write32_fd(fd, MTK_PHY_IAC, cmd);

	if (mdio_wait_idle(fd) != 0)
		exit(EXIT_FAILURE);
}

static uint16_t yt8821_read_ext(int fd, unsigned int phy, uint16_t reg)
{
	mdio_write_c22(fd, phy, 30, reg);
	return mdio_read_c22(fd, phy, 31);
}

static void yt8821_write_ext(int fd, unsigned int phy, uint16_t reg,
			     uint16_t val)
{
	mdio_write_c22(fd, phy, 30, reg);
	mdio_write_c22(fd, phy, 31, val);
}

static void yt8821_status(int fd, unsigned int phy)
{
	static const unsigned int utp_regs[] = { 0, 1, 4, 5, 9, 17 };
	static const unsigned int sds_regs[] = { 0, 17 };

	/* Match the selector sequence used by the official diagnostic script. */
	mdio_write_c22(fd, phy, 30, 0xa000);
	mdio_write_c22(fd, phy, 31, 0x0000);
	printf("YT8821 PHY%u UTP page\n", phy);
	for (size_t i = 0; i < sizeof(utp_regs) / sizeof(utp_regs[0]); i++)
		printf("  reg%-2u = 0x%04x\n", utp_regs[i],
		       mdio_read_c22(fd, phy, utp_regs[i]));

	mdio_write_c22(fd, phy, 30, 0xa000);
	mdio_write_c22(fd, phy, 31, 0x0002);
	printf("YT8821 PHY%u SDS page\n", phy);
	for (size_t i = 0; i < sizeof(sds_regs) / sizeof(sds_regs[0]); i++)
		printf("  reg%-2u = 0x%04x\n", sds_regs[i],
		       mdio_read_c22(fd, phy, sds_regs[i]));

	/* Leave the PHY in its normal UTP register space. */
	mdio_write_c22(fd, phy, 30, 0xa000);
	mdio_write_c22(fd, phy, 31, 0x0000);
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s snapshot\n"
		"  %s read32 <address>\n"
		"  %s dump <address> <length>\n"
		"  %s mdio-read <phy> <reg>\n"
		"  %s mdio-write <phy> <reg> <value> I_UNDERSTAND\n"
		"  %s yt8821-ext-read <phy> <reg>\n"
		"  %s yt8821-ext-write <phy> <reg> <value> I_UNDERSTAND\n"
		"  %s yt8821-status <phy>\n"
		"  %s write32 <address> <value> I_UNDERSTAND\n",
		prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

int main(int argc, char **argv)
{
	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0)
		die("sysconf");

	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0)
		die("open /dev/mem");

	if (argc == 2 && strcmp(argv[1], "snapshot") == 0) {
		snapshot(fd);
	} else if (argc == 3 && strcmp(argv[1], "read32") == 0) {
		uint64_t addr = parse_u64(argv[2]);
		printf("0x%08" PRIx32 "\n", read32_fd(fd, addr));
	} else if (argc == 4 && strcmp(argv[1], "dump") == 0) {
		uint64_t addr = parse_u64(argv[2]);
		size_t len = (size_t)parse_u64(argv[3]);
		dump_fd(fd, addr, len);
	} else if (argc == 4 && strcmp(argv[1], "mdio-read") == 0) {
		uint64_t phy = parse_u64(argv[2]);
		uint64_t reg = parse_u64(argv[3]);
		if (phy > 31 || reg > 31) {
			fprintf(stderr, "ERROR: Clause 22 PHY and register must be 0..31\n");
			close(fd);
			return EXIT_FAILURE;
		}
		printf("PHY%" PRIu64 " reg%" PRIu64 " = 0x%04x\n",
		       phy, reg, mdio_read_c22(fd, (unsigned int)phy,
					      (unsigned int)reg));
	} else if (argc == 6 && strcmp(argv[1], "mdio-write") == 0 &&
		   strcmp(argv[5], "I_UNDERSTAND") == 0) {
		uint64_t phy = parse_u64(argv[2]);
		uint64_t reg = parse_u64(argv[3]);
		uint64_t val = parse_u64(argv[4]);
		if (phy > 31 || reg > 31 || val > UINT16_MAX) {
			fprintf(stderr, "ERROR: invalid Clause 22 write argument\n");
			close(fd);
			return EXIT_FAILURE;
		}
		mdio_write_c22(fd, phy, reg, val);
		printf("PHY%" PRIu64 " reg%" PRIu64 " = 0x%04x\n", phy, reg,
		       mdio_read_c22(fd, phy, reg));
	} else if (argc == 4 && strcmp(argv[1], "yt8821-ext-read") == 0) {
		uint64_t phy = parse_u64(argv[2]);
		uint64_t reg = parse_u64(argv[3]);
		if (phy > 31 || reg > UINT16_MAX) {
			fprintf(stderr, "ERROR: invalid YT8821 extended read argument\n");
			close(fd);
			return EXIT_FAILURE;
		}
		printf("PHY%" PRIu64 " ext 0x%04" PRIx64 " = 0x%04x\n", phy,
		       reg, yt8821_read_ext(fd, phy, reg));
	} else if (argc == 6 && strcmp(argv[1], "yt8821-ext-write") == 0 &&
		   strcmp(argv[5], "I_UNDERSTAND") == 0) {
		uint64_t phy = parse_u64(argv[2]);
		uint64_t reg = parse_u64(argv[3]);
		uint64_t val = parse_u64(argv[4]);
		if (phy > 31 || reg > UINT16_MAX || val > UINT16_MAX) {
			fprintf(stderr, "ERROR: invalid YT8821 extended write argument\n");
			close(fd);
			return EXIT_FAILURE;
		}
		yt8821_write_ext(fd, phy, reg, val);
		printf("PHY%" PRIu64 " ext 0x%04" PRIx64 " = 0x%04x\n", phy,
		       reg, yt8821_read_ext(fd, phy, reg));
	} else if (argc == 3 && strcmp(argv[1], "yt8821-status") == 0) {
		uint64_t phy = parse_u64(argv[2]);
		if (phy > 31) {
			fprintf(stderr, "ERROR: PHY address must be 0..31\n");
			close(fd);
			return EXIT_FAILURE;
		}
		yt8821_status(fd, (unsigned int)phy);
	} else if (argc == 5 && strcmp(argv[1], "write32") == 0 &&
		   strcmp(argv[4], "I_UNDERSTAND") == 0) {
		uint64_t addr = parse_u64(argv[2]);
		uint32_t val = (uint32_t)parse_u64(argv[3]);
		uint32_t before = read32_fd(fd, addr);
		write32_fd(fd, addr, val);
		uint32_t after = read32_fd(fd, addr);
		printf("addr=0x%08" PRIx64 " before=0x%08" PRIx32
		       " after=0x%08" PRIx32 "\n",
		       addr, before, after);
	} else {
		usage(argv[0]);
		close(fd);
		return EXIT_FAILURE;
	}

	close(fd);
	return EXIT_SUCCESS;
}
