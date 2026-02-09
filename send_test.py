import socket
import struct
import time

# RSU 설정 (config.c 참고)
RSU_IP = "127.0.0.1"  # 로컬 테스트
RSU_PORT = 30000      # WL-1 수신 포트

def create_wl1_packet():
    # 1. Header (4 Bytes): ver=1, type=0(Car), ttl=1, res=0
    header = struct.pack("<BBBB", 1, 0, 1, 0)

    # 2. Sender Info (28 Bytes)
    sender_id = 1234
    send_time = int(time.time() * 1000)
    lat, lon, alt = 37000000, 127000000, 0
    reserved = b'\x00' * 4
    sender = struct.pack("<IQiii4s", sender_id, send_time, lat, lon, alt, reserved)

    # 3. Accident Info (32 Bytes)
    # severity=3 (필터 통과 기준: >=2)
    direction = 90
    lane = 1
    severity = 3 
    acc_time = send_time
    acc_id = 9999
    acc_info = struct.pack("<HBBQQiii", direction, lane, severity, acc_time, acc_id, lat, lon, alt)

    # Payload 합치기 (64 Bytes)
    payload = header + sender + acc_info

    # 4. Security Dummy (192 Bytes)
    security = b'\xEE' * 192

    # 전체 패킷 (256 Bytes)
    return payload + security

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    packet = create_wl1_packet()

    print(f"[TEST] Sending WL-1 packet ({len(packet)} bytes) to {RSU_IP}:{RSU_PORT}...")
    sock.sendto(packet, (RSU_IP, RSU_PORT))
    print("[TEST] Sent! Check RSU logs.")

if __name__ == "__main__":
    main()
