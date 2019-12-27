#Problem

Sometimes using a ramdisk for disk I/O is necessary during performance tests.
However the latency of the ramdisk is so much lower than traditional disk I/O
latency and this can result inaccuracies when simulating workload behaviour
and bottlenecks. The following is a design for adding some artificial latency
to ramdisk read and writes.

#Design

We could add a mount option to tmpfs but then users would need to compile and
boot a new kernel. If the goal was to create a patch for the Linux kernel then
a tmpfs mount option would be the better approach. However ignoring the hurdle
of getting the change into the Linux kernel, it would be long before most
systems are upgraded to the latest kernel version. As a result we choose to
create a module that can be loaded to add support for a new file system which
is essentially tmpfs with support for additional mount options.

As alluded to above, we will have two mount options to control the read and
write latency of our ramdisk. Laency values will be in milliseconds.

#Issues

Unable to reuse kernel functions like `shmem_fill_super`, `ramfs_create`,
`ramfs_mkdir`, etc. since they are not exported symbols. During module compile
we get undefined symbol warnings. As a result we end up copying a lot of ramfs
and tmpfs code which we could have reused. The same is true for structures
like `ramfs_file_operations` and `ramfs_file_inode_operations`.

#Notes

```
fs/ramfs/inode.c:

ramfs_parse_options
ramfs_fill_super
ramfs_mount

struct ramfs_fs_info:

struct ramfs_mount_opts {
    umode_t mode;
};

struct ramfs_fs_info {
    struct ramfs_mount_opts mount_opts;
};

mm/shmem.c:

shmem_parse_options
```

