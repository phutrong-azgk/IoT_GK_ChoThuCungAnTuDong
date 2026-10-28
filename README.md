# Hệ thống cho thú cưng ăn tự động

Mô phỏng giữa kỳ môn IoT: máy cho thú cưng ăn tự động sử dụng **ESP32** và Wokwi. Hệ thống cho ăn theo lịch, cho ăn thủ công qua nút nhấn, hiển thị trạng thái trên LCD và cảnh báo khi thức ăn sắp hết.

## Chức năng

- Cho ăn tự động theo lịch: 07:00, 12:00 và 18:00.
- Cho ăn thủ công bằng nút **CHO AN**.
- Servo mô phỏng cửa xả thức ăn.
- LCD 1602 hiển thị giờ, mức thức ăn và trạng thái hệ thống.
- HC-SR04 mô phỏng việc đo mức thức ăn trong hộp.
- Buzzer kêu khi bắt đầu cho ăn.
- Chế độ demo tự động mỗi 10 giây để trình diễn nhanh trên Wokwi.
- Điều khiển từ xa qua MQTT: gửi `FEED` để cho ăn, `REFILL` để nạp đầy.
- Gửi mức thức ăn, trạng thái và cảnh báo sắp hết đến ứng dụng web qua MQTT.

## Linh kiện mô phỏng

| Linh kiện | Vai trò |
| --- | --- |
| ESP32 DevKit | Vi điều khiển trung tâm |
| LCD 1602 I2C | Hiển thị trạng thái |
| DS1307 RTC | Lưu thời gian và lịch cho ăn |
| HC-SR04 | Đo khoảng cách để ước lượng mức thức ăn |
| Servo | Mở/đóng cửa xả thức ăn |
| Nút nhấn | Cho ăn thủ công |
| Nút nhấn thứ hai | Nạp lại mức thức ăn 100% |
| Buzzer | Báo hiệu khi cho ăn |

## Sơ đồ chân ESP32

| Thiết bị | Chân ESP32 |
| --- | --- |
| LCD / RTC (SDA, SCL) | GPIO 21, GPIO 22 |
| HC-SR04 (TRIG, ECHO) | GPIO 5, GPIO 19 |
| Servo | GPIO 18 |
| Nút CHO AN | GPIO 4 |
| Nút NAP THUC AN | GPIO 15 |
| Buzzer | GPIO 23 |

## Chạy trong VS Code với Wokwi

### Chuẩn bị

1. Cài extension **PlatformIO IDE** và **Wokwi for VS Code**.
2. Mở thư mục dự án bằng VS Code.
3. Kích hoạt Wokwi lần đầu: nhấn `F1` → chọn `Wokwi: Request a new License`.

### Chạy mô phỏng

1. Mở Terminal trong thư mục dự án.
2. Biên dịch firmware:

   ```powershell
   pio run
   ```

3. Mở `diagram.json` và nhấn nút Play màu xanh của Wokwi.
4. Nhấn nút **CHO AN** trên mạch để thử chế độ thủ công.

## Điều khiển qua MQTT

Firmware kết nối Wi-Fi `Wokwi-GUEST` và HiveMQ Public Broker (`broker.hivemq.com`, cổng `1883`).

| Topic | Chiều dữ liệu | Nội dung |
| --- | --- | --- |
| `petfeeder_phutrong_azgk_2026/feed/cmd` | Web → ESP32 | `FEED` hoặc `REFILL` |
| `petfeeder_phutrong_azgk_2026/food-level` | ESP32 → Web | Mức thức ăn `0` đến `100` |
| `petfeeder_phutrong_azgk_2026/status` | ESP32 → Web | Trạng thái JSON |
| `petfeeder_phutrong_azgk_2026/alert` | ESP32 → Web | Cảnh báo mức thức ăn thấp |
| `petfeeder_phutrong_azgk_2026/schedule/set` | Web → ESP32 | Ba mốc giờ-phút, ví dụ `07:00,12:30,18:00` |
| `petfeeder_phutrong_azgk_2026/schedule` | ESP32 → Web | Lịch hiện tại theo dạng `HH:MM,HH:MM,HH:MM` |
| `petfeeder_phutrong_azgk_2026/demo/set` | Web → ESP32 | Bật/tắt demo: `ON,10` hoặc `OFF,10` |
| `petfeeder_phutrong_azgk_2026/demo` | ESP32 → Web | Trạng thái demo và số giây giữa hai lượt |

> Topic mặc định đã dùng tiền tố riêng của nhóm. Nếu đổi nó, phải thay cùng lúc trong `sketch.ino` và trang web.

### Chạy giao diện web

1. Cài extension **Live Server** trong VS Code.
2. Nhấp phải `web-dashboard/index.html` → chọn **Open with Live Server**.
3. Chờ LCD Wokwi hiện `Web: da ket noi`.
4. Trên trang web, nhấn **Kết nối**. Sau đó hai nút **Cho ăn ngay** và **Nạp đầy thức ăn** sẽ hoạt động.
5. Trong mục **Lịch tự động**, chọn ba mốc giờ-phút và nhấn **Lưu lịch**.
6. Trong mục **Chế độ demo**, bật/tắt demo và chọn khoảng thời gian từ 3 đến 3600 giây, rồi nhấn **Lưu demo**.

Website và ESP32 phải dùng cùng `Topic prefix`. Khi chạy qua Live Server, địa chỉ mặc định `ws://broker.hivemq.com:8000/mqtt` được dùng. Nếu sau này đưa website lên HTTPS, hãy thay bằng MQTT broker có WebSocket bảo mật `wss://`.

> Lịch và cài đặt demo lưu trong bộ nhớ đang chạy của ESP32. Khi khởi động lại mô phỏng, lịch trở về `07:00`, `12:00`, `18:00` và demo trở về bật mỗi 10 giây.

## Chế độ demo và chế độ thực tế

Điều chỉnh trực tiếp trên dashboard web:

- Bật demo và đặt thời gian, ví dụ 10 giây, để trình diễn nhanh.
- Tắt demo để máy chạy theo lịch đã lưu, nút bấm và lệnh web vẫn luôn hoạt động.

> Khi mô phỏng, nhấp vào HC-SR04 và thay đổi thuộc tính `distance`: khoảng cách nhỏ tương ứng nhiều thức ăn; khoảng cách lớn tương ứng ít thức ăn.

## Cấu trúc dự án

```text
pet-feeder-wokwi/
├── src/main.cpp       # Điểm vào cho PlatformIO
├── sketch.ino         # Chương trình điều khiển chính
├── diagram.json       # Sơ đồ mạch Wokwi
├── platformio.ini     # Cấu hình PlatformIO
├── wokwi.toml         # Cấu hình chạy Wokwi trong VS Code
└── web-dashboard/     # Giao diện điều khiển MQTT
```
