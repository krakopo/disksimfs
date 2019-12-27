#!/bin/sh

set -x
set -e

RED='\033[1;31m'
GREEN='\033[1;32m'
NC='\033[0m' # No Color

success=0

function cleanup() {
	set +e
	sudo umount /tmp/disksim
	sudo rmdir /tmp/disksim
	sudo rmmod -f disksimfs
	if [ $success -eq 0 ]
	then
		echo -e ${RED}FAILED${NC}
	else
		echo -e ${GREEN}PASSED${NC}
	fi
}

trap "cleanup" EXIT

# Test module loads
sudo insmod disksimfs.ko

# Test file system is now available
cat /proc/filesystems | grep disksimfs

# Test mounting
test -f /tmp/disksim && sudo umount -f /tmp/disksim
sudo rm -rf /tmp/disksim
sudo mkdir /tmp/disksim
read_delay_ms=2000
write_delay_ms=5000
sudo mount -t disksimfs -o size=10m,read_delay=${read_delay_ms},write_delay=${write_delay_ms} disksimfs /tmp/disksim
df -h /tmp/disksim
cat /proc/mounts | grep disksimfs | grep "read_delay=${read_delay_ms}" | grep "write_delay=${write_delay_ms}"

# Test write delay
write_start=$(date +%s)
time sudo sh -c "dd if=/dev/zero of=/tmp/disksim/test.out count=1 bs=1"
write_elapsed=$(( $(date +%s) - ${write_start} ))
test ${write_elapsed} -eq $(( ${write_delay_ms} / 1000 ))

# Test read delay
read_start=$(date +%s)
time sudo sh -c "dd if=/tmp/disksim/test.out count=1 bs=1"
read_elapsed=$(( $(date +%s) - ${read_start} ))
test ${read_elapsed} -eq $(( ${read_delay_ms} / 1000 ))

success=1
