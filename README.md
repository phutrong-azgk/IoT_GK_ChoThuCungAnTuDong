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

## Linh kiện mô phỏng

| Linh kiện | Vai trò |
| --- | --- |
| ESP32 DevKit | Vi điều khiển trung tâm |
| LCD 1602 I2C | Hiển thị trạng thái |
| DS1307 RTC | Lưu thời gian và lịch cho ăn |
| HC-SR04 | Đo khoảng cách để ước lượng mức thức ăn |
| Servo | Mở/đóng cửa xả thức ăn |
| Nút nhấn | Cho ăn thủ công |
| Buzzer | Báo hiệu khi cho ăn |

## Sơ đồ chân ESP32

| Thiết bị | Chân ESP32 |
| --- | --- |
| LCD / RTC (SDA, SCL) | GPIO 21, GPIO 22 |
| HC-SR04 (TRIG, ECHO) | GPIO 5, GPIO 19 |
| Servo | GPIO 18 |
| Nút CHO AN | GPIO 4 |
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

## Chế độ demo và chế độ thực tế

Trong `sketch.ino`:

```cpp
const bool DEMO_MODE = true;
const unsigned long DEMO_FEED_INTERVAL = 10000;
```

- `DEMO_MODE = true`: servo tự chạy mỗi 10 giây, phù hợp trình bày giữa kỳ.
- `DEMO_MODE = false`: hệ thống chỉ cho ăn theo lịch 07:00, 12:00, 18:00 hoặc khi nhấn nút thủ công.

> Khi mô phỏng, nhấp vào HC-SR04 và thay đổi thuộc tính `distance`: khoảng cách nhỏ tương ứng nhiều thức ăn; khoảng cách lớn tương ứng ít thức ăn.

## Cấu trúc dự án

```text
pet-feeder-wokwi/
├── src/main.cpp       # Điểm vào cho PlatformIO
├── sketch.ino         # Chương trình điều khiển chính
├── diagram.json       # Sơ đồ mạch Wokwi
├── platformio.ini     # Cấu hình PlatformIO
└── wokwi.toml         # Cấu hình chạy Wokwi trong VS Code
```
