#include <sys/mount.h>

int np_mount(char *mntpt, int mntflags, char *opts)
{
	return mount("none", mntpt, "9P", mntflags, opts);
}
