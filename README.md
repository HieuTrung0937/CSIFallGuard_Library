# FOSSCSI: INVISIBLE FALL DETECTION SYSTEM USING WI-FI CSI

Dự án FossCSI là hệ thống giám sát và cảnh báo té ngã thụ động dựa trên công nghệ Wi-Fi Channel State Information (CSI). Hệ thống được thiết kế chuyên biệt để bảo vệ người cao tuổi trong các không gian nhạy cảm về quyền riêng tư (như phòng tắm, phòng ngủ) mà không cần sử dụng camera ghi hình hay thiết bị đeo tay.

Dự án được xây dựng trên nền tảng vi điều khiển ESP32, ứng dụng kiến trúc hướng đối tượng (OOP), xử lý đa luồng (Dual-Core) và các thuật toán lọc tín hiệu số phức tạp để hoạt động ổn định trong môi trường thực tế.

---

## 1. TỔNG QUAN KIẾN TRÚC HỆ THỐNG

Hệ thống bao gồm hai node vi điều khiển giao tiếp với nhau thông qua giao thức ESP-NOW (không cần router Wi-Fi trung gian):

* **Transmitter (TX - Nguồn phát):** Hoạt động ở chế độ Station, liên tục phát các gói tin quảng bá qua ESP-NOW với tần số tối ưu là 15.6Hz. Công suất phát (TX Power) được giới hạn để cân bằng giữa phạm vi phủ sóng và nhiệt độ vận hành (giữ dưới 45°C).
* **Receiver (RX - Trạm thu và Xử lý):** Hoạt động ở chế độ Promiscuous để thu thập gói tin ở tầng vật lý. Phân tách phần thực (I) và phần ảo (Q) của sóng để tính toán biên độ (Amplitude) của 52 subcarriers. Lõi xử lý (Core 0) chịu trách nhiệm thu nhận tín hiệu tần số cao, trong khi luồng logic tính toán hoạt động độc lập trên Core 1 để tránh lỗi nghẽn hệ thống (Watchdog).

---

## 2. NGUYÊN LÝ HOẠT ĐỘNG VÀ THUẬT TOÁN LÕI

Dự án không chỉ sử dụng cường độ tín hiệu trung bình (RSSI) mà đi sâu vào phân tích sự biến thiên của 52 sóng con (subcarriers) trong dải băng thông 20MHz.

### Thuật toán "Shock + Still" (Chấn động + Bất động)

Để vượt qua bài toán nhiễu sóng trong phòng tắm (do vòi nước chảy), thuật toán yêu cầu hai điều kiện tuần tự:

1. **Trạng thái Shock (Chấn động):** Phát hiện sự sụt giảm biên độ đột ngột của toàn bộ 52 subcarriers (mức thay đổi trung bình > 10%) kết hợp với chênh lệch RSSI lớn (> 10). Tương ứng với khoảnh khắc cơ thể người rơi tự do cắt ngang trường sóng.
2. **Trạng thái Still (Bất động - Lying):** Nước chảy tạo ra độ nhiễu liên tục (Stability cao), nhưng cơ thể người nằm bất tỉnh tạo ra đồ thị phẳng lặng. Hệ thống đánh giá trạng thái này thông qua việc tính toán phương sai (Variance) của chuỗi lịch sử 30 mẫu RSSI. Nếu mức chênh lệch cao (Delta > 5) nhưng độ nhiễu thấp (Stability < 4) kéo dài qua 2 giây, hệ thống xác nhận trạng thái "Nằm".

Kết luận té ngã (Fall) chỉ được kích hoạt khi trạng thái **Shock** xuất hiện ngay trước trạng thái **Still**. Hệ thống cũng bổ sung logic cảnh báo nếu nạn nhân ngã từ từ (Lying Long) và nằm bất động trên 5 giây.

---

## 3. CẤU TRÚC MÃ NGUỒN

Mã nguồn được viết bằng C++ và đóng gói dưới dạng thư viện C-API chuẩn để dễ dàng tích hợp vào mọi nền tảng (ESP-IDF, PlatformIO, Arduino IDE).

* `src/common/`: Định nghĩa các cấu trúc dữ liệu, cấu hình mạng và thư viện hệ thống.
* `src/tx/EspNowTransmitter`: Lớp quản lý cấu hình Wi-Fi và phát gói tin ESP-NOW.
* `src/rx/CsiReceiver`: Lớp quản lý chế độ Promiscuous, hàng đợi sự kiện (Queue), lọc tín hiệu (Median Filter) và xử lý logic phát hiện té ngã.
* `src/CSIFallGuard`: Tầng giao tiếp C-API giúp ẩn đi sự phức tạp của hệ thống bên dưới, cung cấp các hàm gọi trực tiếp cho người dùng.

---

## 4. HƯỚNG DẪN TÍCH HỢP VÀ SỬ DỤNG (API REFERENCE)

### 4.1. Khai báo thông tin mạng và khởi tạo

Trước khi khởi động, cần cung cấp địa chỉ MAC của hai thiết bị TX và RX.

```c
#include "CSIFallGuard.h"

uint8_t mac_tx[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
uint8_t mac_rx[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

void setup() {
    CSIFallGuard_set_channel(1); // Thiết lập kênh Wi-Fi chung
    CSIFallGuard_set_tx_mac(mac_tx);
    CSIFallGuard_set_rx_mac(mac_rx);

    // Khởi tạo ở chế độ TX hoặc RX
    CSIFallGuard_begin(MODE_RX, 1); 
}

```

### 4.2. Cấu hình thông số thuật toán (Dành cho RX)

Bạn có thể tùy chỉnh độ nhạy của hệ thống cho phù hợp với diện tích phòng và vị trí lắp đặt.

```c
// Bật chế độ in log trạng thái và RSSI ra Serial Monitor
CSIFallGuard_set_print_mode(PRINT_MODE_STATE | PRINT_MODE_RSSI);

// Thông số: Ngưỡng chênh lệch RSSI (dB), Thời gian xác nhận nằm (ms), Thời gian thoát (ms)
CSIFallGuard_set_lying_params(12, 3000, 2000);

```

### 4.3. Truy xuất trạng thái (Polling)

Kiểm tra trạng thái liên tục trong vòng lặp chính của hệ thống.

```c
void loop() {
    if (CSIFallGuard_is_fall()) {
        printf("CANH BAO: Phat hien te nga!\n");
        // Kích hoạt còi báo, gửi MQTT, hoặc gọi API
    }
    
    if (CSIFallGuard_is_person()) {
        printf("Co nguoi trong phong.\n");
    }
}

```

---

## 5. THÔNG SỐ VẬN HÀNH THỰC TẾ (BENCHMARK)

* **Vi điều khiển thử nghiệm:** ESP32-S3 (Hỗ trợ 2.4GHz Wi-Fi).
* **Tần số lấy mẫu (Sampling Rate):** ~15.6 Hz.
* **Độ trễ xử lý logic (Latency):** < 10ms.
* **Thời gian phản hồi cảnh báo té ngã (Time to Alert):** Tối đa 2.5 giây từ khi va chạm sàn.
* **Nhiệt độ hoạt động tĩnh:** 40°C - 45°C.

## 6. ĐÓNG GÓP VÀ PHÁT TRIỂN (OPEN SOURCE)

Dự án FossCSI được phát hành dưới tiêu chí mã nguồn mở. Cấu trúc hiện tại đã được modul hóa (System-ready Architecture), sẵn sàng để tích hợp vào các hệ sinh thái nhà thông minh (Smart Home) như Home Assistant hoặc xuất luồng dữ liệu chuẩn JSON qua MQTT để phát triển các ứng dụng quản lý tập trung tại viện dưỡng lão.
