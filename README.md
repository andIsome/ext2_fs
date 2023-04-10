# Standalone ext2 fs implementation

### Important !<br>
#### This is still in dev phase with bugs, lots of dbg messages and missing features

This filesystem is heap allocated and does not interact with the disk.
Upon running the executable it will create a .bin file containing the fs.
You can mount it, to check/navigate the result.

To create the filesystem: `make all`<br>

To mount the filesystem: `make mount`. To navigate the fs open a terminal and type `cd /mnt` <br>

To unmount the filesystem: `make unmount` <br>

To verify the integrity you can run `sudo e2fsck -n -F /dev/loopX`
where **X** is the loop device. (See _**make mount**_ output)