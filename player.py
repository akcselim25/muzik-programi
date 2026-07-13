import os
import win32com.client

class AudioPlayer:
    def __init__(self):
        try:
            # Windows Media Player COM nesnesini başlat
            self.player = win32com.client.Dispatch("WMPlayer.OCX")
            self.player.settings.volume = 50  # Varsayılan ses seviyesi
        except Exception as e:
            print(f"Windows Media Player COM nesnesi başlatılamadı: {e}")
            self.player = None

    def play(self, file_path):
        if not self.player:
            return False
        abs_path = os.path.abspath(file_path)
        try:
            self.player.URL = abs_path
            self.player.controls.play()
            return True
        except Exception as e:
            print(f"Oynatma hatası: {e}")
            return False

    def pause(self):
        if not self.player:
            return
        try:
            self.player.controls.pause()
        except Exception as e:
            print(f"Duraklatma hatası: {e}")

    def resume(self):
        if not self.player:
            return
        try:
            self.player.controls.play()
        except Exception as e:
            print(f"Devam ettirme hatası: {e}")

    def stop(self):
        if not self.player:
            return
        try:
            self.player.controls.stop()
        except Exception as e:
            print(f"Durdurma hatası: {e}")

    def set_volume(self, val):
        if not self.player:
            return
        try:
            val = max(0, min(100, int(val)))
            self.player.settings.volume = val
        except Exception as e:
            print(f"Ses ayar hatası: {e}")

    def get_volume(self):
        if not self.player:
            return 50
        try:
            return self.player.settings.volume
        except Exception as e:
            return 50

    def get_position(self):
        if not self.player:
            return 0.0
        try:
            return self.player.controls.currentPosition
        except Exception as e:
            return 0.0

    def set_position(self, sec):
        if not self.player:
            return
        try:
            self.player.controls.currentPosition = sec
        except Exception as e:
            print(f"İleri/Geri sarma hatası: {e}")

    def get_duration(self):
        if not self.player:
            return 0.0
        try:
            media = self.player.currentMedia
            if media:
                return media.duration
        except Exception as e:
            pass
        return 0.0

    def get_state(self):
        """
        Oynatma durumunu döner:
        1: Stopped (Durduruldu)
        2: Paused (Duraklatıldı)
        3: Playing (Oynatılıyor)
        8: MediaEnded (Bitti)
        Diğer durumlar için WMP dökümantasyonuna bakılabilir.
        """
        if not self.player:
            return 0
        try:
            return self.player.playState
        except Exception as e:
            return 0
