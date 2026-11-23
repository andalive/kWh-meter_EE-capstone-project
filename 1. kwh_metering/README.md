# KWH Meter (ESP32 + PZEM-004T v3.0)

## Fitur <WIP>

- Pengukuran tegangan, arus, daya, energi (kWh), frekuensi, dan power factor (PF)
- Tampilan LCD 3 halaman yang dapat dinavigasi dengan tombol kiri/kanan
- Waktu dan tanggal real-time via NTP (GMT+7)
- Status alarm 3 tingkat: **NORMAL**, **WARNING**, **CRITICAL**
- Indikator LED (hijau/kuning/merah), buzzer, dan relay pemutus beban otomatis
- Tombol untuk mematikan (mute) buzzer saat kondisi kritis
- Rata-rata data setiap 5 menit ditampilkan di Serial Monitor

## Perangkat Keras <WIP>

- ESP32
- Sensor PZEM-004T v3.0
- LCD I2C 20x4
- 2 push button (kiri & kanan)
- Relay, buzzer, dan 3 LED (merah, kuning, hijau)

### Konfigurasi Pin

| Komponen          | Pin ESP32 |
|-------------------|-----------|
| PZEM RX           | GPIO 16   |
| PZEM TX           | GPIO 17   |
| Tombol kiri       | GPIO 14   |
| Tombol kanan      | GPIO 12   |
| Relay             | GPIO 32   |
| LED merah         | GPIO 25   |
| LED kuning        | GPIO 26   |
| LED hijau         | GPIO 27   |
| Buzzer            | GPIO 33   |

Pin SPI untuk SD Card (disiapkan, belum digunakan di kode): CS = 5, MOSI = 23, MISO = 19, SCK = 18.

## Library yang Dibutuhkan

- `PZEM004Tv30`
- `LiquidCrystal_I2C`
- `WiFi` (bawaan ESP32)
- `NTPClient` [lihat instruksi instalasi di bawah](#donwload-install-ntp-client-library)
- `WiFiUdp` (bawaan ESP32)

## Konfigurasi

Sebelum upload, ubah kredensial WiFi pada kode:

```cpp
const char* ssid = "NAMA_WIFI";
const char* password = "PASSWORD_WIFI";
```

> Hindari mengunggah SSID dan password asli ke repository publik.

## Threshold Alarm

| Parameter | Nilai |
|-----------|-------|
| V_HH (tegangan sangat tinggi) | 242 V |
| V_H (tegangan tinggi)         | 231 V |
| V_L (tegangan rendah)         | 200 V |
| V_LL (tegangan sangat rendah) | 198 V |
| I_HH (arus sangat tinggi)     | 8 A   |
| I_H (arus tinggi)             | 6.8 A |

| Status | Kondisi | LED | Relay | Buzzer |
|--------|---------|-----|-------|--------|
| NORMAL   | Semua parameter aman | Hijau | ON | OFF |
| WARNING  | V ≥ 231 V (< 242 V), 198 V < V ≤ 200 V, atau 6.8 A ≤ I < 8 A | Kuning | ON | OFF |
| CRITICAL | V ≥ 242 V, V ≤ 198 V, atau I ≥ 8 A | Merah | OFF | ON (bisa di-mute) |

Mute buzzer akan direset otomatis ketika kondisi kritis terjadi kembali.

## Tampilan LCD

Baris pertama selalu menampilkan waktu dan tanggal. Gunakan tombol kiri/kanan untuk berpindah halaman.

1. **Halaman 1** – Tegangan, arus, daya, energi, dan status alarm
2. **Halaman 2** – Frekuensi, power factor, dan status alarm
3. **Halaman 3** – System status: lama waktu menyala (runtime) dan status alarm

## Arsitektur FreeRTOS

| Task | Prioritas | Fungsi |
|------|-----------|--------|
| `TaskTimeUpdate`    | 1 | Memperbarui waktu/tanggal di LCD setiap 1 detik |
| `TaskSensorRead`    | 2 | Membaca PZEM setiap 1 detik, menampilkan ke Serial & LCD, menghitung rata-rata 5 menit, menjalankan logika alarm |
| `TaskButtonHandler` | 3 | Menerima event tombol dari queue untuk mute buzzer dan navigasi halaman |

- **Queue** (`buttonQueue`): mengirim event tombol (`'R'` / `'L'`) dari ISR ke task
- **Mutex** (`lcdMutex`): melindungi akses ke LCD antar task

---

### **Donwload-Install NTP Client Library**

1. Download folder .zip NTP Client Library (https://github.com/taranais/NTPClient/archive/master.zip)
2. Buka Arduino IDE, klik **Sketch** > **Include Library** > **Add . ZIP library…**
3. Pilih folder .zip yang telah didownload sebelumnya
4. NTP Client library terinstal kemudian

**Dokumentasi NTP Clienet Library**: https://github.com/taranais/NTPClient
