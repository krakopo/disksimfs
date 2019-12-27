# Problem

Sometimes using a ramdisk for disk I/O is necessary during performance tests.
However the latency of the ramdisk is so much lower than traditional disk I/O
latency and this can result inaccuracies when simulating workload behaviour
and bottlenecks. The following is a design for adding some artificial latency
to ramdisk read and writes.

# Design

We could add a mount option to tmpfs but then users would need to compile and
boot a new kernel. If the goal was to create a patch for the Linux kernel then
a tmpfs mount option would be the better approach. However ignoring the hurdle
of getting the change into the Linux kernel, it would be long before most
systems are upgraded to the latest kernel version. As a result we choose to
create a module that can be loaded to add support for a new file system which
is essentially tmpfs with support for additional mount options.

As alluded to above, we will have two mount options to control the read and
write latency of our ramdisk. Laency values will be in milliseconds.
