#include <mntent.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* TODO: create a data structure to hold fields, a 2d array obtaining, rows are
 * all the possible data (Check df --output). This data could be compromised of
 * strings that are created with something like
 * `sprintf(trgtstr, "%lu%c", number, prefix);`
 *
 * TODO2: Create a function calculates maximum widths for every single columns,
 * then prints the table properly aligned. We can do something like this
 * `printf(%*s, width, str);`
 *
 * title1  title4  title5
 *    123      8k    100g
 *  10000     30G     50%
 *
 * Some good reads and references:
 * https://stackoverflow.com/q/4965355
 * https://searchcode.com/codesearch/view/17947198/
 */

int ARGOPTS  = 0x00;
enum {
	PATHLEN  = (int) 128,
	ARGHUMAN = (int) 0x01
};

typedef struct {
	char fsname[PATHLEN]; /* disk's name given by system */
	char mntdir[PATHLEN]; /* disk's mounted directory */
	unsigned long frsize; /* fragment(block) size */
	fsblkcnt_t blocks;    /* number of blocks */
	fsblkcnt_t bfree;     /* number of all free blocks */
	fsblkcnt_t bavail;    /* number of free blocks for unprivileged */
} diskinfo;


void
die(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputc('\n', stderr);
	exit(1);
}

int
isnum(char ch2chk)
{
	if (ch2chk >= 48 && ch2chk <= 57)
		return 1;
	else
		return 0;
}

/* Naming of nvme devices is nvmeAnB.
 * A is the controller number, B is the number of the drive connected
 * to the controller A.
 * Nvme disks are in the form of /dev/nvme0n1p1 and similar.
 *
 * For sata devices, naming convention is sdA.
 * A is the alphabetical number starting from a ending at z. If z is
 * exceeded it turns to aa, then ab and so on.
 * Disks are in the form of /dev/sda1 and similar.
 */
int
isdisk(char *str)
{
	int len = strlen(str);
	int i = 0;

	for (; i < 5; ++i) /* is first five /dev/ */
		if (str[i] != "/dev/"[i])
			return 0;
	for (; i < 7; ++i) /* is the rest either sd or nv */
		if (str[i] != "sd"[i-5] && str[i] != "nv"[i-5])
			return 0;
	/* last is number (sata) or last second is p (nvme) */
	if (!isnum(str[len - 1]) && str[len - 2] != 'p')
		return 0;
	return 1;
}

int
count_mnted_disks(void)
{
	FILE *mtabfile;
	struct mntent *ent;
	int n_mnted = 0;

	mtabfile = setmntent("/etc/mtab", "r");
	if (mtabfile == NULL)
		die("%s", "Failed to open /etc/mtab.");
	while ((ent = getmntent(mtabfile)) != NULL) {
		if (!isdisk(ent->mnt_fsname)) /* Skip non-disk */
			continue;
		n_mnted++;
	}
	endmntent(mtabfile);
	return n_mnted;
}

/* This function takes a statically sized array `disks` with its length `ndisk`;
 * by knowing its length, calculation can be stopped when there is no memory
 * left to store more information.
 * This function will update the `ndisk` variable to be sure somehow available
 * disks did not get smaller than initial value during the computation.
 */
void
get_mnted_disks(diskinfo *disk, int *ndisk)
{
	FILE *mtabfile;
	struct mntent *ent;
	int i = 0;
	void *errptr;

	if ((mtabfile = setmntent("/etc/mtab", "r")) == NULL)
		die("Failed to open /etc/mtab.");
	/* assign sata and nvme disk data */
	while ((i < *ndisk) && ((ent = getmntent(mtabfile)) != NULL)) {
		if (!isdisk(ent->mnt_fsname)) /* Skip if not a disk */
			continue;
		errptr = memccpy(disk[i].fsname, ent->mnt_fsname, '\0', 128);
		if (errptr == NULL)
			die("ERROR: Length of <%s> is too long. Limit is %d.",
			    disk[i].fsname, PATHLEN);
		errptr = memccpy(disk[i].mntdir, ent->mnt_dir, '\0', 128);
		if (errptr == NULL)
			die("ERROR: Length of <%s> is too long. Limit is %d.",
			    disk[i].mntdir, PATHLEN);
		++i;
	}
	endmntent(mtabfile);
	*ndisk = i;
}

void
get_disk_usg(diskinfo *disk, int ndisk)
{
	struct statvfs stat;
	for (int i = 0; i < ndisk; ++i)
		if (!statvfs(disk[i].mntdir, &stat)) {
			disk[i].frsize = stat.f_frsize;
			disk[i].blocks = stat.f_blocks;
			disk[i].bfree = stat.f_bfree;
			disk[i].bavail = stat.f_bavail;
		}
}

unsigned long
conv2human(unsigned long number, char *prefix)
{
	unsigned long retnum = number;
	char prefixes[7] = "kMGTPE"; /* kilo, Mega, Giga, ... */
	*prefix = 'B'; /* default prefix to byte */

	for (int i = 0; retnum >= 1000 && prefixes[i] != '\0'; ++i) {
		retnum /= 1024;
		*prefix = prefixes[i];
	}
	return retnum;
}

void
printhuman(diskinfo disk)
{
	unsigned long total, avail, used, free, useperc;
	char *name, *mntdir, p_total, p_avail, p_used;

	name    = disk.fsname;
	mntdir  = disk.mntdir;
	total   = disk.blocks*disk.frsize;
	avail   = disk.bavail*disk.frsize;
	free    = disk.bfree*disk.frsize;
	used    = total - free;            /* df reports this */
	useperc = used*100/(used+avail);   /* df reports this */

	total = conv2human(total, &p_total);
	avail = conv2human(avail, &p_avail);
	used  = conv2human(used,  &p_used);

	printf("%-16s%3lu%-3c%3lu%-3c%3lu%-2c%3lu%%  %s\n", name, total,
	       p_total, used, p_used, avail, p_avail, useperc, mntdir);
}

void
printdefault(diskinfo disk)
{
	unsigned long total, free, avail, used, useperc;
	char *name, *mntdir;

	name    = disk.fsname;
	mntdir  = disk.mntdir;
	total   = disk.blocks*disk.frsize;
	avail   = disk.bavail*disk.frsize;
	free    = disk.bfree *disk.frsize;
	used    = total - free;              /* df reports this */
	useperc = used*100/(used + avail);   /* df reports this */

	/* Translate to kilobytes */
	total = total/1024;
	used  = used /1024;
	avail = avail/1024;

	printf("%-20s%-13lu%-13lu%-13lu%3lu%%  %s\n", name, total, used, avail,
	       useperc, mntdir);
}

void
printheader(void)
{
	if (ARGOPTS & ARGHUMAN)
		printf("%-16s%-6s%-6s%-6s%-5s%s\n", "Filesystem", "Size",
		       "Used", "Avai", "Use", "Mounted on");
	else
		printf("%-20s%-13s%-13s%-13s%4s  %s\n", "Filesystem",
		       "1K-blocks", "Used", "Available", "Use", "Mounted on");
}

int
main(int argc, char* argv[])
{
	/* get arguments */
	for (int i = 1; i < argc; ++i)
		if (!strcmp(argv[i], "-B") && argv[i + 1] &&
		    *argv[i + 1] != '\0') {
			fprintf(stderr, "%s is not implemented.\n", argv[i]);
			/* TODO */
			++i;
			continue;
		} else if (!strcmp(argv[i], "-h") ||
			   !strcmp(argv[i], "--human-readable")) {
			ARGOPTS |= ARGHUMAN;
			++i;
			continue;
		} else
			fprintf(stderr, "%s is not implemented.\n", argv[i]);

	/* get data */
	int n_mnted = count_mnted_disks();
        diskinfo disk[n_mnted];
	get_mnted_disks(disk, &n_mnted);
	get_disk_usg(disk, n_mnted);

	/* print results */
	printheader();
	for (int i = 0; i < n_mnted; ++i)
		if (ARGOPTS & ARGHUMAN)
			printhuman(disk[i]);
		else
			printdefault(disk[i]);
	return 0;
}
