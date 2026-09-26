# FMT_Tool
A simple python script which signs th SHA256 hash of a given binary file, appends a header and type-length values (TLVs).

## Usage
```bash
# In this directory
uv run fmt-tool --ecu-id 0x123 [--key-path ./path/to/key] ./path/to/binary
```
If a key path is not supplied, it uses an environment variable called PRIVATE_SIGNING_KEY. Both the file at the key path and the environment variable should hold the 32 byte private ed25519 key used to sign the binary in base64 encoding.

ecu-id is a 12 bit (3 hex digit) value which corresponds to the type of ECU this firmware is going to be flashed on.

## Header and TLV Structure

```C
// HEADER structures =================================================================
#define IMAGE_MAGIC 0x96f3b83d
#define IMAGE_HEADER_SIZE 32

// 4 bytes
typedef struct __attribute__((packed)) {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint16_t iv_revision;
} image_version_t;

// 16 bytes
typedef struct __attribute__((packed)) {
    uint32_t ih_magic;
    uint16_t ih_hdr_size;
    uint16_t ih_ecu_id;
    uint32_t ih_img_size;
    image_version_t ih_img_ver;
} image_header_t;

// TLV structures =================================================================
#define IMAGE_TLV_INFO_MAGIC 0x6907
#define IMAGE_TLV_SHA256 0x01
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
1. Header
2. Binary payload
3. TLV header
4. Type-length struct
5. Value
6. Type-length struct
7. Value
8. ...