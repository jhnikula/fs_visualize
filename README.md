fs_visualize
============

Simple tool for visualizing filesystem or block device content.

fs_visualize calculates average byte values in file blocks and outputs them
to png file. This can be used to visualize for instance how filesystems fill
the file or block device or does trim/discard request go to device.

File can be ordinary file or block device.

EXAMPLES

./fs_visualize file
sudo ./fs_visualize /dev/sda1
