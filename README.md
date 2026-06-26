# CSIFallGuard Library

> Thư viện mã nguồn mở hỗ trợ xây dựng các hệ thống cảm biến **Wi-Fi Channel State Information (CSI)** trên ESP32-S3.

CSIFallGuard Library được phát triển nhằm đơn giản hóa việc xây dựng các ứng dụng sử dụng công nghệ **Wi-Fi CSI** trên nền tảng ESP32-S3. Thư viện đóng gói toàn bộ quá trình khởi tạo Wi-Fi, ESP-NOW, thu thập CSI và các thuật toán cơ bản thành một API đơn giản, giúp người dùng tập trung vào việc phát triển ứng dụng thay vì cấu hình hệ thống từ đầu.

Một số ứng dụng tiêu biểu:

* Phát hiện hiện diện của con người (Presence Detection)
* Nhận dạng hoạt động (Human Activity Recognition)
* Phát hiện té ngã (Fall Detection)
* Wireless Sensing
* Smart Home
* Edge AI
* Nghiên cứu Wi-Fi CSI

---

# Tính năng

* Thu thập dữ liệu Wi-Fi CSI trên ESP32-S3
* Hỗ trợ ESP-NOW
* Tự động cấu hình Wi-Fi
* Kiến trúc hướng đối tượng (OOP)
* Phân tách TX và RX
* API đơn giản, dễ sử dụng
* Hỗ trợ ESP-IDF
* Hỗ trợ PlatformIO
* Dễ mở rộng cho các bài toán CSI khác

---

# Yêu cầu

## Phần cứng

* 02 ESP32-S3
* Cáp USB Type-C
* Máy tính cài VSCode

## Phần mềm

* Visual Studio Code
* PlatformIO IDE
* ESP-IDF 5.x trở lên

---

# Cài đặt

## Cách 1: Clone trực tiếp

```bash
git clone https://github.com/HieuTrung0937/CSIFallGuard_Library.git
```

Sau đó sao chép thư viện vào thư mục `lib` của dự án.

Ví dụ:

```text
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

Thêm vào file `platformio.ini`

```ini
lib_deps =
    https://github.com/HieuTrung0937/CSIFallGuard_Library.git
```

Sau đó cài đặt

```bash
pio pkg install
```

---

# Cấu trúc thư viện

```text
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

# API chính

| Hàm                               | Chức năng                                 |
| --------------------------------- | ----------------------------------------- |
| `CSIFallGuard_begin()`            | Khởi tạo thư viện                         |
| `CSIFallGuard_update()`           | Cập nhật dữ liệu CSI                      |
| `CSIFallGuard_set_tx_mac()`       | Thiết lập địa chỉ MAC của bộ phát         |
| `CSIFallGuard_set_rx_mac()`       | Thiết lập địa chỉ MAC của bộ thu          |
| `CSIFallGuard_set_channel()`      | Thiết lập kênh Wi-Fi                      |
| `CSIFallGuard_set_print_mode()`   | Chọn chế độ hiển thị log                  |
| `CSIFallGuard_set_lying_params()` | Cấu hình tham số phát hiện trạng thái nằm |
| `CSIFallGuard_is_fall()`          | Kiểm tra phát hiện té ngã                 |
| `CSIFallGuard_is_lying()`         | Kiểm tra trạng thái nằm                   |
| `CSIFallGuard_get_delta()`        | Lấy giá trị Delta hiện tại                |

---

# Ví dụ sử dụng

## Bộ phát (TX)

```cpp
#include <CSIFallGuard.h>

uint8_t tx_mac[] = { /* MAC TX */ };
uint8_t rx_mac[] = { /* MAC RX */ };

extern "C" void app_main()
{
    CSIFallGuard_set_tx_mac(tx_mac);
    CSIFallGuard_set_rx_mac(rx_mac);
    CSIFallGuard_set_channel(1);

    CSIFallGuard_begin(MODE_TX, 1);

    while (1)
    {
        CSIFallGuard_update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

---

## Bộ thu (RX)

```cpp
#include <CSIFallGuard.h>

uint8_t tx_mac[] = { /* MAC TX */ };
uint8_t rx_mac[] = { /* MAC RX */ };

extern "C" void app_main()
{
    CSIFallGuard_set_tx_mac(tx_mac);
    CSIFallGuard_set_rx_mac(rx_mac);
    CSIFallGuard_set_channel(1);

    CSIFallGuard_begin(MODE_RX, 1);

    while (1)
    {
        CSIFallGuard_update();

        if (CSIFallGuard_is_fall())
        {
            printf("Fall detected!\n");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

---

# Ví dụ

Thư mục `examples/` sẽ bao gồm:

* ESP-NOW cơ bản
* Thu thập CSI
* Presence Detection
* Human Activity Recognition
* Fall Detection
* Realtime Monitor

---

# Đóng góp

Mọi đóng góp đều được hoan nghênh.

Nếu muốn cải thiện thư viện:

1. Fork dự án.
2. Tạo branch mới.
3. Commit thay đổi.
4. Gửi Pull Request.

Hoặc tạo Issue để báo lỗi hoặc đề xuất tính năng mới.

---

# Giấy phép

Dự án được phát hành theo giấy phép **MIT License**.

---

# Tác giả

**Nguyễn Trung Hiếu**

CSIFallGuard Library được xây dựng với mục tiêu mang công nghệ **Wi-Fi Channel State Information (CSI)** đến gần hơn với cộng đồng học sinh, sinh viên, nhà nghiên cứu và lập trình viên Việt Nam thông qua một thư viện mã nguồn mở, dễ sử dụng và dễ mở rộng trên nền tảng ESP32-S3.
