# FMT_Tool
A simple python script which signs th SHA512 hash of a given binary file, appends a header and type-length values (TLVs).

## Usage
```bash
# In this directory
uv run fmt-tool --ecu-id 0x123 --image-version 1.2.3 [--key-path ./path/to/key] ./path/to/binary

# e.g. in CI, with the version kept in a VERSION file holding just "1.2.3"
uv run fmt-tool --ecu-id 0x123 --image-version "$(cat VERSION)" ./path/to/binary

# OR, install as a tool to use anywhere
uv tool install . # use in this directory
```
If a key path is not supplied, it uses an environment variable called PRIVATE_SIGNING_KEY. Both the file at the key path and the environment variable should hold the 32 byte private ed25519 key used to sign the binary in base64 encoding.

ecu-id is a 12 bit (3 hex digit) value which corresponds to the type of ECU this firmware is going to be flashed on.

image-version is the firmware's major.minor.revision, each part 0-255. The bootloader refuses (and erases) an update whose version is older than the image it is replacing; an equal version is accepted.

## Header and TLV Structure

```C
// HEADER structures =================================================================
#define IMAGE_MAGIC 0xb0fe673d
// The header struct is padded out to this size so the binary after it starts on a vector table
// alignment boundary (VTOR needs the table aligned to its size rounded up to a power of 2).
// 1024 covers every supported MCU: the G4s need 512 (118 vectors), the H743 needs 1024
#define IMAGE_HEADER_SIZE 1024

// 4 bytes
typedef struct __attribute__((packed)) {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint8_t iv_revision;
    uint8_t iv_reserved; // keeps the header 16 bytes so the binary after it stays aligned
} image_version_t;

// 16 bytes
typedef struct __attribute__((packed)) {
    uint32_t ih_magic;
    uint16_t ih_hdr_size;
    uint16_t ih_ecu_id;
    uint32_t ih_img_size;
    image_version_t ih_img_ver;
} image_header_t;

// TLV structures ====================================================================
#define IMAGE_TLV_INFO_MAGIC 0xcd34
#define IMAGE_TLV_SHA512 0x01
#define IMAGE_TLV_ED25519_SIG 0x02

// 4 bytes
typedef struct __attribute__((packed)) {
    uint16_t th_tlv_magic;
    uint16_t th_tlv_total_size; // including header
} image_tlv_header_t;

// 4 bytes + tlv_len for each TLV
typedef struct  __attribute__((packed)) {
    uint16_t tlv_type;
    uint16_t tlv_len;
} image_tlv_t;

#endif
```

The final image is as follows:
1. Header, padded with zeros to 1024 bytes (`ih_hdr_size` is 1024) so the binary's vector table is aligned on every supported MCU
2. Binary payload, which must be linked to run at slot A start + 1024 (0x400)
3. TLV header
4. Type-length struct
5. Value
6. Type-length struct
7. Value
8. ...