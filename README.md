# CSIFallGuard Library

> Thư viện mã nguồn mở hỗ trợ xây dựng các hệ thống cảm biến Wi-Fi CSI trên ESP32-S3.

CSIFallGuard Library được phát triển nhằm đơn giản hóa việc xây dựng các ứng dụng sử dụng **Wi-Fi Channel State Information (CSI)** như:

- Phát hiện hiện diện của con người
- Nhận dạng hoạt động
- Phát hiện té ngã
- Nghiên cứu Wireless Sensing
- Các dự án IoT và AI Edge

Thư viện được xây dựng trên nền **ESP-IDF**, hỗ trợ **PlatformIO** và được tối ưu cho ESP32-S3.

---

# Tính năng

- Thu thập Wi-Fi CSI
- Giao tiếp ESP-NOW
- Kiến trúc hướng đối tượng (OOP)
- Hỗ trợ ESP-IDF
- Hỗ trợ PlatformIO
- Phân tách TX và RX
- Dễ mở rộng cho nhiều bài toán CSI

---

# Yêu cầu

## Phần cứng

- 02 ESP32-S3
- Cáp USB
- Máy tính cài VSCode

## Phần mềm

- Visual Studio Code
- PlatformIO IDE
- ESP-IDF

---

# Cài đặt

## Cách 1: Clone trực tiếp

```bash
git clone https://github.com/HieuTrung0937/CSIFallGuard_Library
```

Sau đó copy thư viện vào:

```
lib/
```

Ví dụ:

```
MyProject
│
├── lib
│   └── CSIFallGuard_Library
├── src
├── include
└── platformio.ini
```

---

## Cách 2: PlatformIO

Thêm vào `platformio.ini`

```ini
lib_deps =
    https://github.com/HieuTrung0937/CSIFallGuard_Library
```

Sau đó chạy

```bash
pio pkg install
```

---

# Cấu trúc thư viện

```
CSIFallGuard_Library
│
├── CMakeLists.txt
├── LICENSE
├── README.md
├── library.json
├── platformio.ini
│
├── examples/
│
└── src/
    ├── CSIFallGuard.h
    ├── CSIFallGuard.cpp
    ├── CSIFallGuard_Globals.cpp
    ├── CMakeLists.txt
    │
    ├── common/
    │   ├── config.h
    │   └── esp_now_system.h
    │
    ├── tx/
    │   ├── EspNowTransmitter.hpp
    │   └── EspNowTransmitter.cpp
    │
    └── rx/
        ├── CsiReceiver.hpp
        └── CsiReceiver.cpp
```

---

# Ý nghĩa các thư mục

| Thư mục | Mô tả |
|----------|------|
| examples | Các ví dụ sử dụng thư viện |
| src/common | Thành phần dùng chung |
| src/tx | Bộ phát ESP-NOW |
| src/rx | Bộ thu CSI |
| CSIFallGuard.h | API chính của thư viện |

---

# Sử dụng

Khai báo thư viện

```cpp
#include <CSIFallGuard.h>
```

Khởi tạo

```cpp
CSIFallGuard system;
```

Khởi động

```cpp
void setup()
{
    system.begin();
}
```

Vòng lặp

```cpp
void loop()
{
    system.update();
}
```

---

# Ví dụ

Các ví dụ được đặt trong

```
examples/
```

Dự kiến sẽ bao gồm:

- ESP-NOW cơ bản
- Thu CSI
- Presence Detection
- Human Activity Recognition
- Fall Detection

---


# Đóng góp

Mọi đóng góp đều được hoan nghênh.

Có thể tham gia bằng cách:

1. Fork dự án
2. Tạo branch mới
3. Commit thay đổi
4. Gửi Pull Request

Hoặc tạo Issue để báo lỗi và đề xuất tính năng.

---

# Giấy phép

Dự án được phát hành theo giấy phép MIT License.

---

# Tác giả

Nguyễn Trung Hiếu

Được phát triển với mục tiêu xây dựng một thư viện Wi-Fi CSI mã nguồn mở dành cho cộng đồng Việt Nam, giúp học sinh, sinh viên và nhà phát triển dễ dàng tiếp cận công nghệ Wireless Sensing trên ESP32-S3.
