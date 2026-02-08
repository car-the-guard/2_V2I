#pragma once
#include <stdint.h>
#include <stdbool.h>

// 구조체 패킹 (컴파일러가 바이트 정렬을 위해 빈 공간을 넣지 않도록 함)
#pragma pack(push, 1)

// ---- 공통: 사고 정보 (Accident Info, 32 Bytes) ----
// 이미지: Dir(2)+Lane(1)+Sev(1)+Time(8)+ID(8)+Lat(4)+Lon(4)+Alt(4) = 32 Bytes
typedef struct {
    uint16_t direction;   // 사고 차량 지자기 Heading
    uint8_t  lane;
    uint8_t  severity;    // 2 이상인 값만 서버 등록
    uint64_t accident_time;
    uint64_t accident_id;
    int32_t  lat;         // Micro Deg
    int32_t  lon;         // Micro Deg
    int32_t  alt;         // Micro Deg
} acc_info_t;

// ---- 1. WL-1 / RSU-1 (무선 구간) ----

// 1-1. Header (4 Bytes)
typedef struct {
    uint8_t version;
    uint8_t msg_type;     // 0x00: 차량->RSU, 0x01: RSU->차량
    uint8_t ttl;          // 1 고정
    uint8_t reserved;
} wl1_header_t;

// 1-2. Sender Info (28 Bytes)
// 이미지: ID(4)+Time(8)+Lat(4)+Lon(4)+Alt(4)+Res(4) = 28 Bytes
typedef struct {
    uint32_t sender_id;   // 송신자 ID or RSU ID
    uint64_t send_time;   // 재송신 시각
    int32_t  lat;
    int32_t  lon;
    int32_t  alt;
    uint8_t  reserved[4]; // res[0]: 0xFF 해제 신호
} wl1_sender_info_t;

// 1-3. WL-1 순수 payload (Header + Sender + Accident) = 4 + 28 + 32 = 64 Bytes
typedef struct {
    wl1_header_t      header;
    wl1_sender_info_t sender;
    acc_info_t        accident;
} wl1_payload_t;

// 1-4. WL-1 전체 패킷 (Payload + Wireless Security)
// 무선 보안 서명 크기는 이전 대화의 192B를 가정 (이미지엔 별도 표기 없으나 모듈 존재함)
#define WL_SEC_SIZE 192
typedef struct {
    wl1_payload_t payload;          // 64 Bytes
    uint8_t       security[WL_SEC_SIZE]; // 192 Bytes
} wl1_packet_t; // Total 256 Bytes


// ---- 2. RSU-2 (RSU -> Server) ----

// 2-1. RSU Info (12 Bytes)
typedef struct {
    uint16_t distance;    // RSU와 사고 상황 간 거리
    uint16_t acc_flag;    // On: 0xFFFF, Off: 0x0000
    uint64_t rsu_rx_time; // RSU가 사고 메시지 받은 시각
} rsu2_info_t;

// 2-2. RSU-2 순수 Payload (ID + Acc + RSU Info) = 4 + 32 + 12 = 48 Bytes
typedef struct {
    uint32_t    rsu_id;
    acc_info_t  accident;
    rsu2_info_t rsu_info;
} rsu2_payload_t;

// 2-3. RSU-2 전체 패킷 (Payload + Token)
#define WIRED_TOKEN_SIZE 16
typedef struct {
    rsu2_payload_t payload;           // 48 Bytes
    uint8_t        token[WIRED_TOKEN_SIZE]; // 16 Bytes (RSU ID)
} rsu2_packet_t; // Total 64 Bytes


// ---- 3. RSU-3 (Server -> RSU) ----

// 3-1. Server Info (12 Bytes) - 이미지 상 RSU Info 영역
typedef struct {
    uint64_t server_tx_time;
    uint16_t period_sec;  // 명령 주기 (2초)
    uint16_t acc_flag;    // On/Off
} rsu3_info_t;

// 3-2. RSU-3 순수 Payload (ID + Acc + Srv Info) = 4 + 32 + 12 = 48 Bytes
typedef struct {
    uint32_t    rsu_id;
    acc_info_t  accident;
    rsu3_info_t server_info;
} rsu3_payload_t;

// 3-3. RSU-3 전체 패킷 (Payload + Token)
typedef struct {
    rsu3_payload_t payload;            // 48 Bytes
    uint8_t        token[WIRED_TOKEN_SIZE]; // 16 Bytes
} rsu3_packet_t; // Total 64 Bytes

#pragma pack(pop)

// ---- 이벤트 및 큐용 데이터 타입 정의 ----

// State Manager 등 내부 로직에서 사용할 이벤트 타입
typedef enum {
    EV_WL1_RX,       // 무선 수신 (RSU-2' 형태)
    EV_RSU3_RX,      // 유선 수신 (RSU-3' 형태)
    EV_TIMER_TICK,   // 2초 타이머
    EV_ACK           // ACK 수신 (필요 시)
} sm_event_type_t;

typedef struct {
    sm_event_type_t type;
    union {
        rsu2_payload_t *rsu2p; // WL-1 수신 -> RSU-2' 변환된 것
        rsu3_payload_t *rsu3p; // Server 수신 -> RSU-3' 변환된 것
    } u;
} sm_event_t;

// TX Command (State Manager -> Wired Client)
typedef struct {
    rsu2_payload_t *data; // RSU-2' (보안 붙이기 전)
} tx_cmd_wired_t;

// TX Command (State Manager -> Wireless TX)
typedef struct {
    wl1_payload_t *data; // RSU-1' (보안 붙이기 전)
} tx_cmd_wireless_t;

// WL-1'(Payload) -> RSU-2'(Payload)
bool packet_wl1_to_rsu2(const wl1_payload_t *wl1, uint32_t rsu_id, 
                        uint32_t dist_m, rsu2_payload_t *out);

// RSU-3'(Payload) -> WL-1'(Payload, 즉 RSU-1')
bool packet_rsu3_to_wl1(const rsu3_payload_t *rsu3, wl1_payload_t *out);