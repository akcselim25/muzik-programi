# 📱 ΛURNS for Android (v1.1.0)
### Ultra Hafif, Apple Akıcı Yay Fiziği & Dahili YouTube İndiricili Yerel Müzik Çalar

Bu proje, Windows için geliştirilen **ΛURNS** masaüstü oynatıcısının resmi Android sürümüdür. 

---

## ⚡ Özellikler

1. **YouTube Doğrudan İndirici (MYT Hızı & Sıfır Sunucu Yükü):**
   * YouTube'un mobil iç API'si (Innertube) üzerinden 0.1 saniyede doğrudan ses akışı (`160 kbps M4A`) linkini çözer.
   * Dosyayı doğrudan Android sisteminin `DownloadManager` servisine teslim eder; telefon kapalıyken bile arka planda canlı `%` ile indirir.
2. **Obsidian & Sunset-Neon Arayüz:**
   * `#050505` siyah zemin, `#FF925C` neon turuncu ve `#F044B8` siber pembe vurgular.
   * Çalan parçada parlayan dinamik nefes alan neon durum noktası.
   * Yüzen cam oynatıcı kartı (Floating Glass Dock).
3. **Android 14+ Kilit Ekranı ve Arka Planda Çalma:**
   * Google Media3 / ExoPlayer motoru ile ekran kapalıyken kesintisiz çalma.
   * Kulaklık tuşları, Bluetooth araç kontrolleri ve dalgalı ilerleme çubuğuna sahip modern kilit ekranı kartı.
4. **P2P Canlı Jam Senkronizasyonu:**
   * Bilgisayardaki Windows ΛURNS ile aynı MQTT broker protokolü üzerinden konuşarak telefon ve PC arasında aynı anda canlı müzik dinleme.

---

## 🛠️ Derleme ve Çalıştırma

1. **Android Studio** ile `android/` klasörünü açın.
2. Gradle senkronizasyonunun tamamlanmasını bekleyin.
3. `Run 'app'` (Shift + F10) tuşuna basarak cihazınızda veya emülatörde çalıştırın.
4. APK çıktısı almak için: `Build > Build Bundle(s) / APK(s) > Build APK(s)`.
