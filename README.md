# MyISO - Bootable USB Creator

A powerful command-line tool for creating bootable USB drives from ISO files, similar to Rufus but for Linux.

## Author
**Jeck Christopher Anog**

## Version
0.1.3 prerelease

## Features

- **Raw ISO Burning** - Direct bit-by-bit ISO writing
- **Fast Mode** - Optimized burning with larger block sizes
- **Persistence Support** - Keep your files across reboots
- **Multiple Filesystems** - Support for ext4, NTFS, exFAT, FAT32, FAT64
- **Progress Bar** - Real-time progress with ETA and speed
- **Colorized Output** - Beautiful terminal interface
- **Safe Operations** - Confirmation prompts and error handling

## Requirements

- Linux operating system
- Root privileges (sudo)
- Standard utilities: `dd`, `parted`, `mkfs.*` tools
- C++17 compiler (g++)

## Installation

```bash
# Clone or extract the source code
cd myiso

# Compile
make

# Install system-wide (optional)
sudo make install
```

## Usage

### Basic ISO Burning

```bash
sudo MI -i your-file.iso -o /dev/sdX
```

### With Persistence

```bash
sudo MI -i your-file.iso -p 4096 -f ext4 -o /dev/sdX
```

### Fast Mode Burning

```bash
sudo MI -i your-file.iso -o /dev/sdX -m
```

## Command Options

| Option | Description |
|--------|-------------|
| `-i <file>` | Input ISO file (required) |
| `-o <device>` | Output device like /dev/sdX (required) |
| `-p <size>` | Enable persistence with size in MB |
| `-f <fs>` | Filesystem type for persistence partition |
| `-m` | Use fast mode for ISO burning |
| `-v` | Show version information |
| `-h` | Show help message |

## Supported Filesystems

- **ext4** - Linux native filesystem (default)
- **ntfs** - Windows NTFS
- **exfat** - Cross-platform exFAT
- **FAT32** - Legacy FAT32
- **FAT64** - Extended FAT

**Note:** The `-f` option only works with `-p` (persistence mode).

## Examples

### Create Ubuntu Live USB
```bash
sudo MI -i ubuntu-22.04.iso -o /dev/sdb
```

### Create Persistent Kali Linux USB
```bash
sudo MI -i kali-linux.iso -p 8192 -f ext4 -o /dev/sdc
```

### Fast Mode with Persistence
```bash
sudo MI -i debian.iso -p 4096 -o /dev/sdd -m
```

## Building from Source

```bash
# Clean previous builds
make clean

# Compile with optimizations
make

# Install to /usr/local/bin
sudo make install

# Uninstall
sudo make uninstall
```

## Safety Features

- Root privilege verification
- Device validation before operations
- User confirmation before destructive operations
- Proper error handling and reporting
- Device unmounting before operations
- Filesystem sync after operations

## Error Handling

The tool provides detailed error messages:

```
Fatal Error: Fail writing at /dev/sdX, cause: [specific error]
```

Common errors:
- **Permission Error**: Not running with sudo
- **Device Error**: Invalid or inaccessible device
- **File Error**: Invalid or missing ISO file
- **Filesystem Error**: Unsupported or failed filesystem operation

## Performance

- **Raw Mode**: ~30-50 MB/s (depends on USB speed)
- **Fast Mode**: ~60-100 MB/s (optimized with 4MB blocks)
- **Progress Updates**: Real-time with ETA calculation

## License

Open Source Project - Not Distributed

## Contributing

This is an open-source project. Feel free to contribute improvements, bug fixes, or new features.

## Warnings

⚠️ **WARNING**: This tool will destroy all data on the target device!

- Always double-check the device path
- Backup important data before proceeding
- Verify device is not your system drive
- Use at your own risk

## Support

For issues, questions, or contributions, please contact the author or submit an issue in the project repository.

## Changelog

### v0.1.3 prerelease
- Initial release
- Raw ISO burning
- Fast mode support
- Persistence feature
- Multiple filesystem support
- Progress bar with ETA
- Colorized output
- Comprehensive error handling

---

**Made with ❤️ by Jeck Christopher Anog**
