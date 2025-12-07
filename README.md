# MyISO - Advanced Bootable USB Creator

A powerful, production-grade command-line tool for creating bootable USB drives from ISO files. Unlike other tools, MyISO implements native filesystem creation and partition management without relying on external utilities.

## Author
**Jeck Christopher Anog**

## Version
0.5.7

## Advanced Features

### Core Capabilities
- Native Filesystem Creation - Direct FAT32, EXT4, and NTFS implementation without mkfs
- MBR/GPT Partition Tables - Complete partition table management from scratch
- Interactive Partition Table Selection - User-friendly prompts with detailed information
- Automatic Storage Validation - Pre-flight checks with helpful error messages
- Dry Run Mode - Simulate operations and show detailed information without making changes
- ISO Type Detection - Automatically identifies Pure ISO, El Torito, or Hybrid ISO formats
- Device Validation - Ensures whole disk devices, rejects partition paths
- Zero-Copy I/O - Uses sendfile() for maximum performance
- True Persistence - Creates actual partitions with proper bootloader integration
- Bootloader Installation - Automatic SYSLINUX/GRUB detection and installation
- Optimized Buffer Management - Direct I/O with aligned memory buffers
- Professional Output - Beautiful terminal interface with detailed progress
- Production-Ready - Enterprise-grade error handling and safety checks
- Force Mode - Bypass confirmations for automated operations

### Technical Innovations
- **Native Filesystem Formatting**: Creates FAT32, EXT4, and NTFS filesystems by directly writing boot sectors, superblocks, and file allocation tables
- **Direct Partition Management**: Implements MBR and GPT partition table creation with proper CHS/LBA addressing
- **Bootloader Integration**: Automatically detects and installs appropriate bootloader (SYSLINUX or GRUB)
- **Advanced I/O Operations**: Uses O_DIRECT, posix_memalign(), and sendfile() for optimal performance
- **Proper Boot Structure**: Creates bootable disks with correct partition flags and boot code
- **Smart Space Management**: Automatically validates available storage and provides actionable suggestions
- **Optimized CRC32**: Lookup table implementation for 8x faster GPT checksums
- **ISO Format Detection**: Identifies Hybrid, El Torito, and Pure ISO formats automatically

## Requirements

- Linux operating system (kernel 2.6+)
- Root privileges (sudo)
- C++17 compiler (g++ 7.0+)
- No external dependencies for filesystem creation

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

Creates a bootable USB with:
- Interactive partition table selection (MBR or GPT)
- FAT32 filesystem (native creation)
- SYSLINUX bootloader
- Boot menu with kernel parameters

### With Native Persistence

```bash
sudo MI -i your-file.iso -p 4096 -f ext4 -o /dev/sdX
```

Creates:
- Partition 1: FAT32 with ISO contents
- Partition 2: EXT4 with persistence label
- Bootloader with persistence menu entry
- All filesystems created natively (no mkfs)
- Automatic space validation before operation

### Fast Mode with Zero-Copy I/O

```bash
sudo MI -i your-file.iso -o /dev/sdX -m
```

Uses sendfile() for zero-copy data transfer achieving 60-100+ MB/s

### Dry Run Mode (Simulation)

```bash
sudo MI -i your-file.iso -p 4096 -f ext4 -o /dev/sdX --dry-run
```

Shows comprehensive information without making any changes:
- ISO type and size
- Device capacity
- Planned operations
- Space analysis
- All checks and validations

### Quick System Info

```bash
sudo MI -i your-file.iso -o /dev/sdX -asi
```

Displays aggressive system info (quick, non-comprehensive format)

### Force Mode (Skip Confirmations)

```bash
sudo MI -i your-file.iso -o /dev/sdX --force
```

Bypasses confirmation prompts for automated operations

### Specify Partition Table Type

```bash
sudo MI -i your-file.iso -o /dev/sdX -t mbr
sudo MI -i your-file.iso -o /dev/sdX -t gpt
```

## Command Options

| Option | Description |
|--------|-------------|
| `-i <file>` | Input ISO file (required) |
| `-o <device>` | Output device like /dev/sdX (required) |
| `-p <size>` | Enable persistence with size in MB |
| `-f <fs>` | Filesystem type for persistence (native creation) |
| `-t <type>` | Partition table type (mbr or gpt), prompts if not specified |
| `-m` | Use fast mode (zero-copy I/O) |
| `--dry-run` | Show all information without performing operations |
| `-asi` | Show aggressive system info (quick, non-comprehensive) |
| `--force` | Force operation, bypass warnings |
| `-v` | Show version information |
| `-h` | Show help message |

## Interactive Partition Table Selection

When partition table type is not specified with `-t`, MyISO presents an interactive prompt:

```
╔════════════════════════════════════════════════════════════════╗
║        PARTITION TABLE SELECTION                              ║
╚════════════════════════════════════════════════════════════════╝

Please choose the installation partition table type.
This is needed by the BIOS to properly boot your system.

If you don't know what this is, read the manual at:
https://wiki.archlinux.org/title/Partitioning#Partition_table

[1]. MBR (Master Boot Record)
     • Compatible with older systems (BIOS)
     • Maximum 4 primary partitions
     • Supports disks up to 2TB
     • Recommended for maximum compatibility

[2]. GPT (GUID Partition Table)
     • Required for UEFI systems
     • Supports 128 partitions
     • Supports disks larger than 2TB
     • Recommended for modern systems

Choose [1/2]:
```

## ISO Type Detection

MyISO automatically detects and reports the ISO format:

### Detected Types
- **Pure ISO 9660**: Standard ISO without boot capabilities
- **El Torito Bootable ISO**: ISO with El Torito boot specification
- **Hybrid ISO**: Contains both MBR partition table and ISO 9660 filesystem
- **Unknown/Non-standard ISO**: Unrecognized format

### Detection Process
1. Checks sector 16 for ISO 9660 signature (CD001)
2. Checks sector 17 for El Torito boot record
3. Checks sector 0 for MBR signature and partition table
4. Combines findings to determine ISO type

## Device Validation

MyISO requires whole disk devices, not partitions.

### Valid Devices
```bash
/dev/sdb     # Correct - whole disk
/dev/sdc     # Correct - whole disk
/dev/nvme0n1 # Correct - whole disk
```

### Invalid Devices
```bash
/dev/sdb1    # Wrong - partition 1
/dev/sdb3    # Wrong - partition 3
/dev/nvme0n1p1 # Wrong - partition 1
```

### Error Message
```
[FATAL] Fatal Error: The target device is incomplete.
  You specified: /dev/sdb3
  Try instead: /dev/sdb
  Just remove the number at the end.
```

## Automatic Storage Validation

MyISO automatically validates available storage before any operations. If insufficient space is detected, detailed error messages are provided:

### Example: Insufficient Storage Error

```
[FATAL] Insufficient storage for requested persistence
  Device: 2048 MB
  ISO: 1536 MB
  Requested persistence: 4096 MB
  Required: 5632 MB
  Shortage: 3584 MB

  Maximum persistence available: 312 MB

Try: MI -i ubuntu.iso -p 312 -f ext4 -o /dev/sdb
```

### Storage Check Features

- Pre-flight validation before destructive operations
- Detailed breakdown of space requirements
- Overhead calculation (100MB for partition tables, alignment, filesystem structures)
- Minimum size enforcement (512 MB for persistence)
- Automatic maximum space calculation
- Actionable command suggestions

## Dry Run Mode Details

The `--dry-run` option provides comprehensive simulation without making changes:

### Information Displayed
- Input information (ISO file, size, type)
- Device information (size, capacity in MB and GB)
- Operation details (partition table type, burn mode, persistence settings)
- Step-by-step planned operations
- Space analysis (ISO size, persistence size, overhead, total used, remaining space, usage percentage)

### Example Output
```
=== DRY RUN MODE - NO CHANGES WILL BE MADE ===

Input Information:
  ISO File: ubuntu-22.04.iso
  ISO Size: 3500 MB
  ISO Type: Hybrid ISO (MBR + ISO 9660)
  Target Device: /dev/sdb
  Device Size: 8000 MB (7 GB)

Operation Details:
  Partition Table: MBR
  Burn Mode: Raw (Standard)
  Persistence: Enabled
  Persistence Size: 4096 MB
  Persistence Filesystem: ext4

Planned Operations:
  1. Unmount all partitions on /dev/sdb
  2. Create MBR partition table
  3. Create partition 1: FAT32 (3500 MB)
  4. Burn ISO to partition 1
  5. Create partition 2: ext4 (4096 MB)
  6. Install bootloader (SYSLINUX/GRUB)
  7. Sync and finalize

Space Analysis:
  ISO: 3500 MB
  Persistence: 4096 MB
  Overhead: ~100 MB
  Total Used: 7696 MB
  Remaining: 304 MB
  Usage: 96%

All checks passed. Ready to proceed with actual operation.
Remove --dry-run flag to perform the actual operation.
```

## Native Filesystem Support

All filesystems are created directly by MyISO without external tools:

### FAT32 (Native Implementation)
- Boot sector with BPB (BIOS Parameter Block)
- FSInfo sector for free cluster tracking
- Dual FAT tables with proper initialization
- Root directory cluster allocation
- Volume label and serial number
- Post-creation boot signature verification

### EXT4 (Native Implementation)
- Superblock with ext4 features
- Block group descriptors
- Inode table initialization
- UUID generation
- Journal preparation structures
- Superblock magic number verification

### NTFS (Native Implementation)
- Boot sector with NTFS BPB
- MFT (Master File Table) initialization
- Volume serial number generation
- Cluster allocation setup

## Technical Details

### Partition Table Creation

**MBR Structure:**
```
Offset   Size   Description
0x000    440    Bootstrap code
0x1B8    4      Disk signature
0x1BE    64     Partition table (4 entries)
0x1FE    2      Boot signature (0x55AA)
```

**Each Partition Entry:**
```
Offset   Size   Description
0x00     1      Boot flag (0x80 = bootable)
0x01     3      CHS address of first sector
0x04     1      Partition type
0x05     3      CHS address of last sector
0x08     4      LBA of first sector
0x0C     4      Number of sectors
```

### FAT32 Structure Created

1. **Boot Sector (Sector 0)**
   - Jump instruction (0xEB 0x58 0x90)
   - OEM name ("MSWIN4.1")
   - BPB with geometry
   - Extended BPB with FAT32 specifics
   - Boot signature (0x55AA)

2. **FSInfo Sector (Sector 1)**
   - Lead signature (0x41615252)
   - Free cluster count
   - Next free cluster hint
   - Trail signature (0xAA550000)

3. **FAT Tables (Starting Sector 32)**
   - Media descriptor
   - End-of-chain markers
   - Cluster allocation chains

4. **Data Region**
   - Root directory at cluster 2
   - File and directory data

### EXT4 Structure Created

1. **Superblock (Offset 1024)**
   - Inode/block counts
   - Block size (4096 bytes)
   - Features (extent, flex_bg, etc.)
   - Volume UUID
   - Volume label
   - Magic number (0xEF53)

2. **Block Group Descriptors**
   - Bitmap locations
   - Inode table locations
   - Free block/inode counts

3. **Inode Tables**
   - Root inode (#2)
   - Reserved inodes

### Bootloader Installation Process

1. **Detection Phase**
   - Scans ISO for ISOLINUX/SYSLINUX signature
   - Checks for GRUB configuration
   - Defaults to SYSLINUX if uncertain

2. **SYSLINUX Installation**
   - Writes MBR boot code (first 440 bytes)
   - Creates /syslinux directory
   - Generates syslinux.cfg with boot menu
   - Configures persistence kernel parameters

3. **Boot Menu Configuration**
   ```
   DEFAULT menu.c32
   LABEL linux
     KERNEL /casper/vmlinuz
     APPEND initrd=/casper/initrd boot=casper quiet splash
   
   LABEL persistent
     KERNEL /casper/vmlinuz
     APPEND initrd=/casper/initrd boot=casper persistent quiet
   ```

## Performance Optimizations

### Zero-Copy I/O (Fast Mode)
- Uses `sendfile()` system call
- Eliminates user-space buffer copying
- Achieves 60-100+ MB/s on USB 3.0
- Falls back to optimized buffered I/O if unsupported

### Direct I/O (Raw Mode)
- `O_DIRECT` flag for unbuffered writes
- `posix_memalign()` for 4K-aligned buffers
- 4MB buffer size for optimal throughput
- Achieves 40-70 MB/s on USB 3.0

### CRC32 Optimization
- Pre-computed lookup table (256 entries)
- Single-pass calculation
- 8x faster than bit-by-bit method
- Used for GPT header verification

### Buffer Management
```cpp
// Aligned buffer allocation for O_DIRECT
void* buffer;
posix_memalign(&buffer, 4096, 4*1024*1024);

// Zero-copy transfer
sendfile(output_fd, input_fd, nullptr, chunk_size);
```

## Safety Features

- Comprehensive privilege verification
- Device validation (whole disk vs partition)
- User confirmation for destructive operations
- Atomic filesystem operations
- Proper error handling and rollback
- Device unmounting before operations
- Multiple fsync points for data integrity
- Partition table commit with kernel notification
- Pre-operation storage validation
- Detailed error reporting with actionable suggestions
- Post-creation integrity verification

## Building from Source

```bash
# Clean build
make clean

# Compile with optimizations
make

# Install system-wide
sudo make install

# Uninstall
sudo make uninstall
```

## Advanced Examples

### Dry Run Before Actual Operation
```bash
# First, check what will happen
sudo MI -i ubuntu-22.04.iso -p 8192 -f ext4 -o /dev/sdb --dry-run

# If everything looks good, run for real
sudo MI -i ubuntu-22.04.iso -p 8192 -f ext4 -o /dev/sdb
```

### Quick System Check
```bash
# Fast info display
sudo MI -i debian.iso -o /dev/sdc -asi
```

### Automated Operation (No Prompts)
```bash
# Force mode for scripts
sudo MI -i fedora.iso -p 4096 -o /dev/sdd -t mbr --force
```

### Maximum Performance with GPT
```bash
# Fast mode with zero-copy I/O and GPT
sudo MI -i arch.iso -o /dev/sdc -m -t gpt
```

### Custom Filesystem with Verification
```bash
# Use NTFS, dry run first, then execute
sudo MI -i windows.iso -p 4096 -f ntfs -o /dev/sdd --dry-run
sudo MI -i windows.iso -p 4096 -f ntfs -o /dev/sdd
```

## Error Handling

Detailed error messages with context and suggestions:

### Storage Error Example
```
[FATAL] Insufficient storage space on device
  Device capacity: 2048 MB
  ISO size: 1536 MB
  Requested persistence: 4096 MB
  Required total: 5736 MB
  Shortage: 3688 MB

Maximum persistence you can use: 312 MB
```

### Device Validation Error
```
[FATAL] Fatal Error: The target device is incomplete.
  You specified: /dev/sdb3
  Try instead: /dev/sdb
  Just remove the number at the end.
```

### Common Error Scenarios Handled
- Invalid devices or missing privileges
- Corrupted or invalid ISO files
- Insufficient disk space with exact calculations
- I/O errors with sector information
- Filesystem creation failures
- Bootloader installation issues
- Space constraint violations
- Partition vs whole disk errors

## Performance Benchmarks

| Mode | USB 2.0 | USB 3.0 | USB 3.1 |
|------|---------|---------|---------|
| Raw  | 15-25 MB/s | 40-70 MB/s | 60-100 MB/s |
| Fast | 20-30 MB/s | 60-100 MB/s | 80-150 MB/s |

## Troubleshooting

### "This is a privilege tool, to access this, use sudo"
Run with sudo: `sudo MI ...`

### "The target device is incomplete"
You specified a partition (e.g., /dev/sdb1) instead of whole disk.
Use the base device name (e.g., /dev/sdb) without the partition number.

### "Insufficient storage for requested persistence"
The error message will show:
- Exact space available
- Shortage amount
- Suggested command with maximum available persistence

Example:
```bash
# If you see: "Maximum persistence available: 312 MB"
# Use the suggested command:
sudo MI -i ubuntu.iso -p 312 -f ext4 -o /dev/sdb
```

### "Fatal Error: Fail writing at /dev/sdX"
- Check device is not mounted
- Verify device path is correct (whole disk, not partition)
- Ensure sufficient space
- Check for hardware issues

### Bootloader Installation Failed
- ISO may have custom boot structure
- Try different bootloader type
- Check ISO integrity
- Review detected ISO type

### Device Too Small
The tool will calculate and display:
- Total space needed
- Space shortage
- Maximum persistence possible
- Whether persistence is even possible on the device

### Want to Preview Operations?
Use `--dry-run` to see exactly what will happen without making changes:
```bash
sudo MI -i your.iso -p 4096 -o /dev/sdb --dry-run
```

## Contributing

This is an open-source project. Contributions welcome:
- Filesystem implementations (exFAT, F2FS)
- GPT partition support enhancements
- UEFI boot support
- Additional bootloader support
- Enhanced storage optimization algorithms
- Additional ISO format detection

## License

Open Source Project - Not Distributed

## Technical References

- Microsoft FAT32 Specification
- ext4 Filesystem Documentation
- NTFS Technical Specification
- SYSLINUX Project Documentation
- MBR/GPT Standards (UEFI Specification)
- ISO 9660 Standard
- El Torito Bootable CD-ROM Specification

## Changelog

### v0.2.4 prerelease
- Added interactive partition table selection (MBR/GPT) with detailed information
- Implemented automatic free space checking before operations
- Added comprehensive storage validation with helpful error messages
- Improved error reporting with exact shortage calculations
- Added smart suggestions for maximum available persistence
- Implemented --dry-run mode for operation simulation
- Added -asi flag for quick aggressive system info display
- Added --force option to bypass confirmation prompts
- Implemented ISO type detection (Pure ISO, El Torito, Hybrid)
- Enhanced MBR/GPT implementation with optimized CRC32 (8x faster)
- Optimized filesystem creation with O_DIRECT support
- Added device validation to reject partition paths
- Added post-creation filesystem integrity verification
- Fixed compilation errors and improved code quality
- Enhanced user prompts with formatted display and manual links

### v0.1.3 prerelease
- Initial release
- Raw ISO burning
- Fast mode support
- Persistence feature
- Multiple filesystem support
- Progress bar with ETA
- Colorized output
- Comprehensive error handling

**Built with modern C++17 and low-level system programming**

**Created by Jeck Christopher Anog**
