#pragma once
#include <stdint.h>
#include <stdbool.h>

#pragma pack(push, 1)

// [공통] 사고 정보 (32 Bytes)
typedef struct {
    uint16_t direction;
    uint8_t  lane;
    uint8_t  severity;
    uint64_t accident_time;
    uint64_t accident_id;
    int32_t  lat;
    int32_t  lon;
    int32_t  alt;
} acc_info_t;

// [1. WL-1 / RSU-1] 무선 패킷
typedef struct {
    uint8_t version;
    uint8_t msg_type;     // 0x00:Car, 0x01:RSU
    uint8_t ttl;          // 1
    uint8_t reserved;
} wl1_header_t;

typedef struct {
    uint32_t sender_id;
    uint64_t send_time;
    int32_t  lat;
    int32_t  lon;
    int32_t  alt;
    uint8_t  reserved[4]; // res[0]: 0xFF 해제
} wl1_sender_info_t;

// WL-1 Payload (보안 제외 순수 데이터: 64 Bytes)
typedef struct {
    wl1_header_t      header;
    wl1_sender_info_t sender;
    acc_info_t        accident;
} wl1_payload_t;

// WL-1 Full Packet (Payload + Security 192B: 256 Bytes)
#define WL_SEC_SIZE 192
typedef struct {
    wl1_payload_t payload;
    uint8_t       security[WL_SEC_SIZE];
} wl1_packet_t;


// [2. RSU-2] RSU -> Server
typedef struct {
    uint16_t distance;
    uint16_t acc_flag;    // On:0xFFFF, Off:0x0000
    uint64_t rsu_rx_time;
} rsu2_info_t;

// RSU-2 Payload (48 Bytes)
typedef struct {
    uint32_t    rsu_id;
    acc_info_t  accident;
    rsu2_info_t rsu_info;
} rsu2_payload_t;

// RSU-2 Full Packet (Payload + Token 16B: 64 Bytes)
#define WIRED_TOKEN_SIZE 16
typedef struct {
    rsu2_payload_t payload;
    uint8_t        token[WIRED_TOKEN_SIZE];
} rsu2_packet_t;


// [3. RSU-3] Server -> RSU
typedef struct {
    uint64_t server_tx_time;
    uint16_t period_sec;
    uint16_t acc_flag;    // On/Off
} rsu3_info_t;

// RSU-3 Payload (48 Bytes)
typedef struct {
    uint32_t    rsu_id;
    acc_info_t  accident;
    rsu3_info_t server_info;
} rsu3_payload_t;

// RSU-3 Full Packet (Payload + Token 16B: 64 Bytes)
typedef struct {
    rsu3_payload_t payload;
    uint8_t        token[WIRED_TOKEN_SIZE];
} rsu3_packet_t;

#pragma pack(pop)

// [내부 이벤트 타입]
typedef enum {
    EV_WL1_RX,       // 무선 수신 -> 필터/보안 거쳐 RSU-2'로 변환됨
    EV_RSU3_RX,      // 서버 수신 -> 보안 거쳐 RSU-3'로 변환됨
    EV_TIMER_TICK    // 2초 타이머
} sm_event_type_t;

typedef struct {
    sm_event_type_t type;
    union {
        rsu2_payload_t *rsu2p;
        rsu3_payload_t *rsu3p;
    } u;
} sm_event_t;

// [TX Command: StateManager -> WiredClient]
typedef struct {
    rsu2_payload_t *rsu2p; // 아직 Token 안 붙은 것
} tx_cmd_wired_t;

#endif // types.h 중복 방지 (pragma once 사용 시 생략 가능)