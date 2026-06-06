#ifndef CSI_RECEIVER_HPP
#define CSI_RECEIVER_HPP

#include "common/config.h"
#include "common/esp_now_system.h"
#include "freertos/queue.h"
#include <math.h>

/**
 * @brief Chế độ in dữ liệu (bitmask, có thể kết hợp nhiều chế độ)
 * 
 * Ví dụ: setPrintMode(PRINT_MODE_RSSI | PRINT_MODE_STATE);
 */
#define PRINT_MODE_NONE      0       //!< Không in gì (tiết kiệm CPU)
#define PRINT_MODE_RSSI      1       //!< In RSSI và delta mỗi giây
#define PRINT_MODE_STATE     2       //!< In trạng thái (có người, nằm, té ngã)
#define PRINT_MODE_RAW       4       //!< In raw I/Q (rất nhiều, chỉ debug)
#define PRINT_MODE_AMPLITUDE 8       //!< In % thay đổi amplitude

/**
 * @brief Lớp CsiReceiver - dành cho board NHẬN (RX)
 * 
 * Chức năng chính:
 *   - Thu gói tin WiFi qua chế độ promiscuous
 *   - Lọc gói từ board TX dựa trên MAC
 *   - Phân tích RSSI và CSI (52 subcarrier)
 *   - Phát hiện có người, phát hiện nằm ổn định, phát hiện té ngã
 * 
 * Cách dùng cơ bản:
 * @code
 * CsiReceiver rx;
 * rx.setPrintMode(PRINT_MODE_STATE | PRINT_MODE_RSSI);
 * rx.setLyingParams(12, 3000, 2000);  // ngưỡng 12dB, xác nhận 3s, thoát 2s
 * rx.begin();
 * @endcode
 */
class CsiReceiver {
public:
    /**
     * @brief Khởi tạo và bắt đầu thu CSI
     * 
     * Quá trình:
     *   1. Khởi tạo NVS
     *   2. Khởi tạo WiFi ở chế độ promiscuous
     *   3. Tạo queue và các task xử lý
     */
    void begin();

    /**
     * @brief Giải phóng tài nguyên (queue)
     */
    ~CsiReceiver();

    /**
     * @brief Chọn chế độ in dữ liệu
     * @param mode Bitmask của PRINT_MODE_xxx
     */
    void setPrintMode(uint8_t mode);

    /**
     * @brief Cấu hình ngưỡng phát hiện cơ bản
     * @param standing Ngưỡng phát hiện có người (dB, mặc định 5)
     * @param lying    Ngưỡng phát hiện té ngã (dB, mặc định 14)
     */
    void setThreshold(uint8_t standing, uint8_t lying);

    /**
     * @brief Cấu hình tham số phát hiện nằm (quan trọng)
     * @param delta_threshold Ngưỡng RSSI giảm để coi là ứng viên nằm (dB)
     * @param confirm_ms      Thời gian duy trì ngưỡng để xác nhận nằm (ms)
     * @param exit_ms         Thời gian hồi phục để thoát khỏi trạng thái nằm (ms)
     * 
     * Giải thích:
     *   - Khi RSSI giảm >= delta_threshold trong confirm_ms ms -> chuyển sang trạng thái NẰM
     *   - Khi RSSI hồi phục (delta < delta_threshold/2) trong exit_ms ms -> thoát NẰM
     */
    void setLyingParams(int delta_threshold, int confirm_ms, int exit_ms);

    /**
     * @brief Kiểm tra trạng thái nằm hiện tại
     * @return true đang nằm, false không nằm
     */
    bool isLying();
    bool isPersonPresent();

    bool isFallDetected();
private:
    QueueHandle_t      event_queue_ = nullptr;   //!< Hàng đợi dữ liệu từ callback ISR
    volatile uint32_t  pkt_count_   = 0;         //!< Đếm số gói đã nhận
    uint8_t            print_mode_  = PRINT_MODE_STATE | PRINT_MODE_RSSI;  //!< Chế độ in mặc định
    uint8_t            thresh_standing_ = 5;     //!< Ngưỡng phát hiện có người (dB)
    uint8_t            thresh_lying_    = 14;    //!< Ngưỡng phát hiện té ngã (dB)

    // Tham số phát hiện nằm
    int    lying_delta_threshold_ = 12;   //!< Ngưỡng RSSI giảm để coi là ứng viên nằm (dB)
    int    lying_confirm_ms_      = 3000; //!< Thời gian duy trì ngưỡng để xác nhận nằm (ms)
    int    lying_exit_ms_         = 2000; //!< Thời gian hồi phục để thoát nằm (ms)

    bool   is_lying_          = false;
    bool   person_present_ = false;
    bool   fall_detected_  = false;//!< Trạng thái nằm hiện tại
    uint32_t lying_start_time_ = 0;      //!< Thời gian bắt đầu trạng thái nằm

    // Các hàm khởi tạo
    void init_nvs();      //!< Khởi tạo NVS (lưu cấu hình WiFi)
    void init_wifi();     //!< Khởi tạo WiFi ở chế độ promiscuous

    // Các hàm xử lý dữ liệu
    void processRSSI(int rssi, uint32_t seq);          //!< Xử lý RSSI: lọc, baseline, phát hiện người/nằm
    void processCSI(int16_t *csi_raw, uint32_t seq);   //!< Xử lý CSI: amplitude, té ngã

    // Callback static (bắt buộc cho C-API)
    static void IRAM_ATTR s_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type);
    static void s_process_task(void *arg);
    static void s_stats_task(void *arg);

    static CsiReceiver *s_instance;   //!< Con trỏ instance (dùng trong callback static)
};

#endif // CSI_RECEIVER_HPP
