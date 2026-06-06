#ifndef ESP_NOW_TRANSMITTER_HPP
#define ESP_NOW_TRANSMITTER_HPP

#include "common/config.h"
#include "common/esp_now_system.h"

/**
 * @brief Lớp EspNowTransmitter - dành cho board PHÁT (TX)
 * 
 * Chức năng: 
 *   - Gửi gói tin qua ESP-NOW liên tục với tần số 20Hz
 *   - Dùng cho board đặt trong phòng tắm, phát sóng Wi-Fi để RX thu CSI
 * 
 * Cách dùng:
 *   EspNowTransmitter tx;
 *   tx.begin();
 * 
 * @note Cần set MAC của board RX trước khi gọi begin() qua API:
 *       CSIFallGuard_set_rx_mac(mac)
 */
class EspNowTransmitter {
public:
    /**
     * @brief Khởi tạo và bắt đầu gửi gói tin
     * 
     * Quá trình:
     *   1. Khởi tạo NVS (lưu cấu hình WiFi)
     *   2. Khởi tạo WiFi ở chế độ STA (client)
     *   3. Khởi tạo ESP-NOW và đăng ký peer (board RX)
     *   4. Tạo task riêng để gửi gói với tần số 20Hz
     */
    void begin();

private:
    uint32_t           send_count_ = 0;      ///< Đếm số gói đã gửi thành công
    esp_now_payload_t  tx_data_    = {0, 0, 11.7f};  ///< Dữ liệu gói tin (seq, timestamp, giá trị test)
    TickType_t         last_wake_time;        ///< Thời gian thức cuối cùng (dùng cho vTaskDelayUntil)

    /**
     * @brief Khởi tạo NVS (Non-Volatile Storage) - lưu cấu hình WiFi
     * 
     * Quan trọng: NVS phải được khởi tạo trước khi dùng WiFi
     */
    void init_nvs();

    /**
     * @brief Khởi tạo WiFi ở chế độ STA (Station)
     * 
     * Thiết lập:
     *   - Kênh WiFi: từ biến toàn cục s_channel (mặc định 1)
     *   - Công suất phát: 20 (giảm nhiệt)
     *   - Giao thức: 11B | 11G | 11N (bắt buộc để có CSI)
     */
    void init_wifi();

    /**
     * @brief Khởi tạo ESP-NOW và đăng ký peer (board nhận - RX)
     * 
     * Thiết lập:
     *   - Đăng ký callback khi gửi xong
     *   - Thêm peer với MAC là s_rx_mac (do người dùng set)
     *   - Cấu hình tốc độ 2Mbps (ổn định)
     */
    void init_espnow();

    /**
     * @brief Callback được gọi khi ESP-NOW gửi gói xong
     * 
     * @param info Thông tin về gói đã gửi (không dùng)
     * @param status Kết quả: ESP_NOW_SEND_SUCCESS (0) hoặc ESP_NOW_SEND_FAIL (1)
     * 
     * @note Hàm này chạy trong context của WiFi driver, cần giữ nhẹ và nhanh
     * @note Là static vì ESP-NOW yêu cầu hàm callback là C-style
     */
    static void s_send_cb(const wifi_tx_info_t *info, esp_now_send_status_t status);

    /**
     * @brief Task chính: vòng lặp gửi gói liên tục
     * 
     * Chu kỳ: 50ms (20Hz)
     * Mỗi lần gửi:
     *   - Tăng số thứ tự gói
     *   - Ghi timestamp
     *   - Gửi qua esp_now_send() đến MAC đã đăng ký
     * 
     * @note Task này chạy trên Core 1 (priority 5)
     */
    static void s_send_task(void *arg);

    static EspNowTransmitter *s_instance;  ///< Con trỏ instance (dùng trong callback static)
};

#endif
