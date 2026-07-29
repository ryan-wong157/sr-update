#ifndef UDS_CODES_H
#define UDS_CODES_H

// =================================================================================================
// UDS Service IDs
// =================================================================================================
#define SID_SESS_CTRL_RQ 0x10
#define SID_SESS_CTRL_RES 0x50

#define SID_ECU_RST_RQ 0x11
#define SID_ECU_RST_RES 0x51

#define SID_SEC_ACCESS_RQ 0x27
#define SID_SEC_ACCESS_RES 0x67

#define SID_TESTER_HBT_RQ 0x3E
#define SID_TESTER_HBT_RES 0x7E

#define SID_READ_DATA_RQ 0x22
#define SID_READ_DATA_RES 0x62

#define SID_DOWNLOAD_START_RQ 0x34
#define SID_DOWNLOAD_START_RES 0x74

#define SID_TRNSFR_DATA_RQ 0x36
#define SID_TRNSFR_DATA_RES 0x76

#define SID_DOWNLOAD_EXIT_RQ 0x37
#define SID_DOWNLOAD_EXIT_RES 0x77

// =================================================================================================
// Negative Response Codes
// =================================================================================================
#define SID_NEG_RES 0x7F
#define NRC_GENERAL_REJECT 0x10
#define NRC_SERVICE_NOT_SUPPORTED 0x11
#define NRC_SUB_FUNCTION_NOT_SUPPORTED 0x12
#define NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT 0x13
#define NRC_RESPONSE_TOO_LONG 0x14
#define NRC_BUSY_REPEAT_REQUEST 0x21
#define NRC_CONDITIONS_NOT_CORRECT 0x22
#define NRC_REQUEST_SEQUENCE_ERROR 0x24
#define NRC_EXEC_FAILURE 0x26
#define NRC_REQUEST_OUT_OF_RANGE 0x31
#define NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED 0x70
#define NRC_TRANSFER_DATA_SUSPENDED 0x71
#define NRC_PROGRAMMING_FAILURE 0x72
#define NRC_WRONG_BLOCK_SEQUENCE_COUNTER 0x73
#define NRC_REQUEST_RECEIVED_RESPONSE_PENDING 0x78
#define NRC_SERVICE_NOT_SUPPORTED_IN_CURR_SESS 0x7F

#endif