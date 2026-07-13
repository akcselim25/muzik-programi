import os
import json
import threading
import time
import tkinter as tk
from tkinter import filedialog
import customtkinter as ctk

from player import AudioPlayer
from downloader import search_youtube, search_soundcloud, download_audio, get_stream_url

# CustomTkinter Ayarları
ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")

CONFIG_FILE = "config.json"

class SearchWindow(ctk.CTkToplevel):
    def __init__(self, parent):
        super().__init__(parent)
        self.parent = parent
        
        self.title("Şarkı Bul & İndir (MYT)")
        self.geometry("650x550")
        self.minsize(550, 450)
        
        # Arayüz nesne listeleri (Tasarımın çökmesini engellemek için widget referans takibi)
        self.yt_result_widgets = []
        self.sc_result_widgets = []
        
        # Grid Arayüz Düzeni
        self.grid_rowconfigure(0, weight=0) # Arama Çubuğu
        self.grid_rowconfigure(1, weight=1) # Sonuçlar Sekmesi
        self.grid_columnconfigure(0, weight=1)
        
        self.create_widgets()
        
        # Pencereyi öne getir ve odaklan
        self.lift()
        self.focus()
        
    def create_widgets(self):
        # Üst Arama Kutusu Frame
        search_bar_frame = ctk.CTkFrame(self, fg_color="transparent")
        search_bar_frame.grid(row=0, column=0, sticky="ew", padx=15, pady=15)
        search_bar_frame.grid_columnconfigure(0, weight=1)
        search_bar_frame.grid_columnconfigure(1, weight=0)
        
        self.search_entry = ctk.CTkEntry(
            search_bar_frame, 
            placeholder_text="Şarkı adı veya sanatçı yazın...",
            height=40,
            font=("Arial", 13)
        )
        self.search_entry.grid(row=0, column=0, sticky="ew", padx=(0, 10))
        self.search_entry.bind("<Return>", lambda e: self.start_search())
        
        self.search_btn = ctk.CTkButton(
            search_bar_frame, 
            text="🔍 Şarkı Ara", 
            command=self.start_search,
            width=100,
            height=40,
            font=("Arial", 12, "bold")
        )
        self.search_btn.grid(row=0, column=1, sticky="e")
        
        # Sonuçlar için Sekmeli Yapı (Tabview)
        self.tabview = ctk.CTkTabview(self)
        self.tabview.grid(row=1, column=0, sticky="nsew", padx=15, pady=(0, 15))
        
        self.tab_yt = self.tabview.add("YouTube")
        self.tab_sc = self.tabview.add("SoundCloud")
        
        # Her sekme için kaydırılabilir panel
        self.tab_yt.grid_rowconfigure(0, weight=1)
        self.tab_yt.grid_columnconfigure(0, weight=1)
        self.yt_scroll_frame = ctk.CTkScrollableFrame(self.tab_yt, fg_color="transparent")
        self.yt_scroll_frame.grid(row=0, column=0, sticky="nsew")
        
        self.tab_sc.grid_rowconfigure(0, weight=1)
        self.tab_sc.grid_columnconfigure(0, weight=1)
        self.sc_scroll_frame = ctk.CTkScrollableFrame(self.tab_sc, fg_color="transparent")
        self.sc_scroll_frame.grid(row=0, column=0, sticky="nsew")
        
        # Başlangıçta boş mesajlar göster
        lbl_yt = ctk.CTkLabel(self.yt_scroll_frame, text="Arama yapmak için yukarıdaki çubuğu kullanın.", text_color="gray")
        lbl_yt.pack(pady=40)
        self.yt_result_widgets.append(lbl_yt)
        
        lbl_sc = ctk.CTkLabel(self.sc_scroll_frame, text="Arama yapmak için yukarıdaki çubuğu kullanın.", text_color="gray")
        lbl_sc.pack(pady=40)
        self.sc_result_widgets.append(lbl_sc)

    def start_search(self):
        """Arama işlemini arka plan iş parçacığında (Thread) başlatır."""
        query = self.search_entry.get().strip()
        if not query:
            return
            
        self.search_btn.configure(state="disabled", text="Aranıyor...")
        self.search_entry.configure(state="disabled")
        
        # Eski YouTube arama widgetlarını temizle
        for w in self.yt_result_widgets:
            try:
                w.destroy()
            except:
                pass
        self.yt_result_widgets.clear()
        
        # Eski SoundCloud arama widgetlarını temizle
        for w in self.sc_result_widgets:
            try:
                w.destroy()
            except:
                pass
        self.sc_result_widgets.clear()
        
        # Yükleniyor yazıları ekle
        loading_lbl_yt = ctk.CTkLabel(self.yt_scroll_frame, text="Sonuçlar aranıyor, lütfen bekleyin...", font=("Arial", 13))
        loading_lbl_yt.pack(pady=30)
        self.yt_result_widgets.append(loading_lbl_yt)
        
        loading_lbl_sc = ctk.CTkLabel(self.sc_scroll_frame, text="Sonuçlar aranıyor, lütfen bekleyin...", font=("Arial", 13))
        loading_lbl_sc.pack(pady=30)
        self.sc_result_widgets.append(loading_lbl_sc)
            
        thread = threading.Thread(target=self.bg_search_thread, args=(query,), daemon=True)
        thread.start()

    def bg_search_thread(self, query):
        """Arka planda aramaları gerçekleştirir."""
        yt_results = search_youtube(query, max_results=10)
        sc_results = search_soundcloud(query, max_results=10)
        self.after(0, lambda: self.show_search_results(yt_results, sc_results))

    def show_search_results(self, yt_results, sc_results):
        """Arama sonuçlarını listeler ve butonları aktif hale getirir."""
        self.search_btn.configure(state="normal", text="🔍 Şarkı Ara")
        self.search_entry.configure(state="normal")
        
        # Eski yükleniyor yazılarını temizle
        for w in self.yt_result_widgets:
            try:
                w.destroy()
            except:
                pass
        self.yt_result_widgets.clear()
        
        for w in self.sc_result_widgets:
            try:
                w.destroy()
            except:
                pass
        self.sc_result_widgets.clear()
        
        # YouTube Sonuçlarını Göster
        if not yt_results:
            lbl = ctk.CTkLabel(self.yt_scroll_frame, text="YouTube'da sonuç bulunamadı.", text_color="gray")
            lbl.pack(pady=30)
            self.yt_result_widgets.append(lbl)
        else:
            self.render_result_list(self.yt_scroll_frame, yt_results, self.yt_result_widgets)
            
        # SoundCloud Sonuçlarını Göster
        if not sc_results:
            lbl = ctk.CTkLabel(self.sc_scroll_frame, text="SoundCloud'da sonuç bulunamadı.", text_color="gray")
            lbl.pack(pady=30)
            self.sc_result_widgets.append(lbl)
        else:
            self.render_result_list(self.sc_scroll_frame, sc_results, self.sc_result_widgets)

    def render_result_list(self, parent_frame, results, widget_list):
        """Arama sonuçlarını listelemek için arayüz kartları oluşturur."""
        for item in results:
            card = ctk.CTkFrame(parent_frame, fg_color="#252525", height=50)
            card.pack(fill="x", pady=4, padx=5)
            card.pack_propagate(False)
            widget_list.append(card)
            
            card.grid_columnconfigure(0, weight=1) # Başlık
            card.grid_columnconfigure(1, weight=0) # Süre / Kanal
            card.grid_columnconfigure(2, weight=0) # Dinle Butonu
            card.grid_columnconfigure(3, weight=0) # İndir Butonu
            
            # Şarkı Adı (Sınırlandırılmış uzunluk)
            title_text = item['title']
            if len(title_text) > 40:
                title_text = title_text[:37] + "..."
                
            title_lbl = ctk.CTkLabel(
                card, 
                text=title_text, 
                anchor="w", 
                font=("Arial", 12, "bold"),
                text_color="#f0f0f0"
            )
            title_lbl.grid(row=0, column=0, sticky="w", padx=12, pady=12)
            
            # Kanal & Süre Bilgisi
            duration_str = self.parent.format_time(item['duration']) if item['duration'] else "Süre Bilinmiyor"
            info_text = f"{item['uploader']}  |  ⏱ {duration_str}"
            info_lbl = ctk.CTkLabel(
                card, 
                text=info_text, 
                font=("Arial", 11),
                text_color="gray"
            )
            info_lbl.grid(row=0, column=1, padx=10, pady=12)
            
            # Dinle Butonu
            preview_btn = ctk.CTkButton(
                card, 
                text="🎵 Dinle", 
                width=75, 
                height=30,
                fg_color="#1f538d",
                hover_color="#1a4473",
                font=("Arial", 11, "bold")
            )
            preview_btn.grid(row=0, column=2, padx=(5, 5), pady=10)
            preview_btn.configure(command=lambda u=item['url'], t=item['title'], b=preview_btn: self.parent.start_preview(u, t, b))
            
            # İndir Butonu
            download_btn = ctk.CTkButton(
                card, 
                text="📥 İndir", 
                width=75, 
                height=30,
                fg_color="#10b981",
                hover_color="#059669",
                font=("Arial", 11, "bold")
            )
            download_btn.grid(row=0, column=3, padx=(5, 12), pady=10)
            download_btn.configure(command=lambda u=item['url'], i=item['id'], b=download_btn: self.parent.start_download(u, i, b))


class MusicApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        
        self.title("Antigravity Müzik Çalar")
        self.geometry("500x650")
        self.minsize(450, 550)
        
        # Oynatıcı ve İndirici Durum Değişkenleri
        self.player = AudioPlayer()
        self.current_folder = ""
        self.song_files = []
        self.current_song_index = -1
        self.is_seeking = False
        self.active_downloads = {}  # {id: {"button": btn, "percent": int, "status": str}}
        self.was_playing = False
        self.search_window = None # Toplevel arama penceresi referansı
        
        # Arayüz nesne listeleri (Tasarımın çökmesini engellemek için widget referans takibi)
        self.local_song_buttons = []
        
        # Grid Arayüz Düzeni (Sadece tek sütun)
        self.grid_rowconfigure(0, weight=1)    # Üst klasör & Liste paneli
        self.grid_rowconfigure(1, weight=0)    # Alt player paneli
        self.grid_columnconfigure(0, weight=1)
        
        # Bileşenleri Oluştur
        self.create_left_panel()
        self.create_player_panel()
        
        # Ayarları yükle ve son klasörü aç
        self.load_last_folder()
        
        # Güncelleme döngüsünü başlat
        self.update_loop()
        
    def load_last_folder(self):
        """Son seçilen klasörü config.json dosyasından yükler."""
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                    config = json.load(f)
                    folder = config.get("last_folder", "")
                    if folder and os.path.exists(folder):
                        self.set_active_folder(folder)
            except Exception as e:
                print(f"Ayarlar yüklenirken hata: {e}")

    def save_last_folder(self, folder):
        """Seçilen klasör yolunu config.json dosyasına kaydeder."""
        try:
            with open(CONFIG_FILE, "w", encoding="utf-8") as f:
                json.dump({"last_folder": folder}, f, ensure_ascii=False, indent=4)
        except Exception as e:
            print(f"Ayarlar kaydedilirken hata: {e}")

    def set_active_folder(self, folder):
        """Aktif klasörü ayarlar ve yerel dosyaları listeler."""
        self.stop_song() # Eski çalan şarkıyı durdur ve sıfırla
        self.current_folder = folder
        self.folder_label.configure(text=os.path.basename(folder) if folder else "Klasör Seçilmedi")
        self.refresh_local_files()
        self.save_last_folder(folder)

    def select_folder(self):
        """Kullanıcıya klasör seçtirir."""
        folder = filedialog.askdirectory()
        if folder:
            self.set_active_folder(folder)

    def refresh_local_files(self):
        """Klasördeki desteklenen ses dosyalarını listeler (Çift dosyaları eler)."""
        if not self.current_folder or not os.path.exists(self.current_folder):
            return
            
        valid_extensions = ('.mp3', '.m4a', '.wav', '.flac', '.ogg')
        try:
            all_files = [f for f in os.listdir(self.current_folder) if f.lower().endswith(valid_extensions)]
        except Exception as e:
            all_files = []
            print(f"Klasör okunurken hata: {e}")
            
        # Öncelik sırası: .mp3 > .m4a > .wav > .flac > .ogg
        priority = {'.mp3': 5, '.m4a': 4, '.wav': 3, '.flac': 2, '.ogg': 1}
        unique_songs = {}
        
        for f in all_files:
            base_name, ext = os.path.splitext(f)
            ext_lower = ext.lower()
            
            if base_name not in unique_songs:
                unique_songs[base_name] = (f, ext_lower)
            else:
                current_file, current_ext = unique_songs[base_name]
                if priority.get(ext_lower, 0) > priority.get(current_ext, 0):
                    unique_songs[base_name] = (f, ext_lower)
                    
        sorted_bases = sorted(unique_songs.keys())
        files = [unique_songs[base][0] for base in sorted_bases]
        
        self.song_files = [os.path.join(self.current_folder, f) for f in files]
        
        for btn in self.local_song_buttons:
            try:
                btn.destroy()
            except:
                pass
        self.local_song_buttons.clear()
            
        if not files:
            no_songs_label = ctk.CTkLabel(self.song_list_frame, text="Bu klasörde müzik bulunamadı.", text_color="gray")
            no_songs_label.pack(pady=20)
            self.local_song_buttons.append(no_songs_label)
            return

        for index, file_name in enumerate(files):
            btn = ctk.CTkButton(
                self.song_list_frame,
                text=f"🎵 {file_name}",
                anchor="w",
                fg_color="transparent",
                text_color="#e0e0e0",
                hover_color="#2b2b2b",
                height=35,
                corner_radius=6,
                command=lambda idx=index: self.play_song_at_index(idx)
            )
            btn.pack(fill="x", padx=5, pady=2)
            self.local_song_buttons.append(btn)
            
        self.highlight_current_song()

    def highlight_current_song(self):
        """Aktif oynatılan şarkı butonunu görsel olarak vurgular."""
        for idx, widget in enumerate(self.local_song_buttons):
            if isinstance(widget, ctk.CTkButton):
                if idx == self.current_song_index:
                    widget.configure(fg_color="#1f538d", text_color="white")
                else:
                    widget.configure(fg_color="transparent", text_color="#e0e0e0")

    def play_song_at_index(self, index):
        """Belirtilen indeksteki şarkıyı oynatır."""
        if not self.song_files or index < 0 or index >= len(self.song_files):
            return
            
        self.current_song_index = index
        song_path = self.song_files[index]
        song_name = os.path.basename(song_path)
        
        if self.player.play(song_path):
            self.song_title_label.configure(text=song_name)
            self.play_pause_button.configure(text="⏸")
            self.highlight_current_song()
            self.was_playing = True
            self.player.set_volume(self.volume_slider.get())

    def play_next_song(self):
        """Sonraki şarkıya geçer."""
        if not self.song_files:
            return
        next_idx = (self.current_song_index + 1) % len(self.song_files)
        self.play_song_at_index(next_idx)

    def play_prev_song(self):
        """Önceki şarkıya geçer."""
        if not self.song_files:
            return
        prev_idx = (self.current_song_index - 1) % len(self.song_files)
        if prev_idx < 0:
            prev_idx = len(self.song_files) - 1
        self.play_song_at_index(prev_idx)

    def toggle_play_pause(self):
        """Oynatmayı duraklatır veya devam ettirir."""
        if self.current_song_index == -1 and self.song_files:
            self.play_song_at_index(0)
            return
            
        state = self.player.get_state()
        if state == 3: # Playing
            self.player.pause()
            self.play_pause_button.configure(text="▶")
        elif state == 2: # Paused
            self.player.resume()
            self.play_pause_button.configure(text="⏸")
        elif state == 1 or state == 8 or state == 0:
            self.play_song_at_index(self.current_song_index)

    def stop_song(self):
        """Müziği duraklatır ve yerini korur (sıfırlanıp başa sarmaması için)."""
        self.was_playing = False
        self.player.pause()
        self.play_pause_button.configure(text="▶")

    def on_seek_drag(self, val):
        dur = self.player.get_duration()
        self.time_label.configure(text=f"{self.format_time(val)} / {self.format_time(dur)}")

    def on_seek_release(self, event):
        new_pos = self.seek_slider.get()
        self.player.set_position(new_pos)
        self.after(100, lambda: setattr(self, 'is_seeking', False))

    def on_volume_change(self, val):
        self.player.set_volume(val)
        self.volume_label.configure(text=f"🔊 {int(val)}%")

    def format_time(self, seconds):
        mins = int(seconds // 60)
        secs = int(seconds % 60)
        return f"{mins:02d}:{secs:02d}"

    def create_left_panel(self):
        """Ana panel: Klasör seçme, Müzik Arama Butonu ve Yerel şarkı listesi."""
        self.left_frame = ctk.CTkFrame(self, corner_radius=0)
        self.left_frame.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)
        
        self.left_frame.grid_rowconfigure(0, weight=0) # Butonlar
        self.left_frame.grid_rowconfigure(1, weight=0) # Klasör ismi
        self.left_frame.grid_rowconfigure(2, weight=0) # Başlık
        self.left_frame.grid_rowconfigure(3, weight=1) # Liste
        self.left_frame.grid_columnconfigure(0, weight=1)
        self.left_frame.grid_columnconfigure(1, weight=1)
        
        # Klasör Seç Butonu (Sol)
        self.select_folder_btn = ctk.CTkButton(
            self.left_frame, 
            text="📁 Klasör Seç", 
            command=self.select_folder,
            fg_color="#2b2b2b",
            hover_color="#3d3d3d",
            height=40,
            font=("Arial", 12, "bold")
        )
        self.select_folder_btn.grid(row=0, column=0, padx=(15, 5), pady=(15, 5), sticky="ew")
        
        # Şarkı Bul & İndir Butonu (Sağ)
        self.open_search_btn = ctk.CTkButton(
            self.left_frame, 
            text="🔍 Şarkı Bul & İndir (MYT)", 
            command=self.open_search_window,
            fg_color="#10b981",
            hover_color="#059669",
            height=40,
            font=("Arial", 12, "bold")
        )
        self.open_search_btn.grid(row=0, column=1, padx=(5, 15), pady=(15, 5), sticky="ew")
        
        # Aktif Klasör İsmi
        self.folder_label = ctk.CTkLabel(
            self.left_frame, 
            text="Klasör Seçilmedi", 
            text_color="gray",
            font=("Arial", 11, "italic")
        )
        self.folder_label.grid(row=1, column=0, columnspan=2, padx=15, pady=(0, 15), sticky="ew")
        
        # Müziklerim Başlığı
        self.local_title = ctk.CTkLabel(
            self.left_frame, 
            text="KİTAPLIĞIM (Müzikler)", 
            font=("Arial", 12, "bold"),
            text_color="#1f538d",
            anchor="w"
        )
        self.local_title.grid(row=2, column=0, columnspan=2, padx=15, pady=(5, 5), sticky="w")
        
        # Şarkı Listesi Kaydırılabilir Pencere
        self.song_list_frame = ctk.CTkScrollableFrame(self.left_frame, fg_color="transparent")
        self.song_list_frame.grid(row=3, column=0, columnspan=2, sticky="nsew", padx=10, pady=(0, 10))

    def create_player_panel(self):
        """Alt panel: Çalma kontrolleri, süre ve ses çubukları."""
        self.player_frame = ctk.CTkFrame(self, height=100, corner_radius=0)
        self.player_frame.grid(row=1, column=0, sticky="ew")
        
        self.player_frame.grid_rowconfigure(0, weight=1)
        self.player_frame.grid_columnconfigure(0, weight=1) # Başlık
        self.player_frame.grid_columnconfigure(1, weight=2) # Kontroller
        self.player_frame.grid_columnconfigure(2, weight=1) # Ses
        
        # SOL: Şarkı Başlığı
        title_frame = ctk.CTkFrame(self.player_frame, fg_color="transparent")
        title_frame.grid(row=0, column=0, sticky="w", padx=20)
        self.song_title_label = ctk.CTkLabel(
            title_frame, 
            text="Müzik Çalınmıyor", 
            font=("Arial", 13, "bold"),
            anchor="w",
            text_color="#e0e0e0"
        )
        self.song_title_label.pack(fill="x")
        
        # ORTA: Oynatma Kontrolleri & İlerleme Çubuğu
        center_controls = ctk.CTkFrame(self.player_frame, fg_color="transparent")
        center_controls.grid(row=0, column=1, sticky="nsew", pady=5)
        
        # Butonlar Üst Satır
        buttons_row = ctk.CTkFrame(center_controls, fg_color="transparent")
        buttons_row.pack(pady=(5, 0))
        
        self.prev_button = ctk.CTkButton(buttons_row, text="⏮", width=40, height=30, fg_color="transparent", font=("Arial", 16), command=self.play_prev_song)
        self.prev_button.pack(side="left", padx=5)
        
        self.play_pause_button = ctk.CTkButton(buttons_row, text="▶", width=45, height=35, fg_color="#1f538d", hover_color="#296eb4", font=("Arial", 18), command=self.toggle_play_pause)
        self.play_pause_button.pack(side="left", padx=5)
        
        self.stop_button = ctk.CTkButton(buttons_row, text="⏹", width=40, height=30, fg_color="transparent", font=("Arial", 16), command=self.stop_song)
        self.stop_button.pack(side="left", padx=5)
        
        self.next_button = ctk.CTkButton(buttons_row, text="⏭", width=40, height=30, fg_color="transparent", font=("Arial", 16), command=self.play_next_song)
        self.next_button.pack(side="left", padx=5)
        
        # Süre Barı Alt Satır
        slider_row = ctk.CTkFrame(center_controls, fg_color="transparent")
        slider_row.pack(fill="x", padx=10, pady=(2, 5))
        
        self.seek_slider = ctk.CTkSlider(
            slider_row, 
            from_=0, 
            to=100, 
            height=12,
            command=self.on_seek_drag
        )
        self.seek_slider.set(0)
        self.seek_slider.pack(side="left", fill="x", expand=True, padx=(0, 10))
        
        self.seek_slider.bind("<ButtonPress-1>", lambda e: setattr(self, 'is_seeking', True))
        self.seek_slider.bind("<ButtonRelease-1>", self.on_seek_release)
        
        self.time_label = ctk.CTkLabel(slider_row, text="00:00 / 00:00", font=("Arial", 11), width=85)
        self.time_label.pack(side="right")
        
        # SAĞ: Ses Ayarı
        volume_frame = ctk.CTkFrame(self.player_frame, fg_color="transparent")
        volume_frame.grid(row=0, column=2, sticky="e", padx=20)
        
        self.volume_label = ctk.CTkLabel(volume_frame, text="🔊 50%", font=("Arial", 11), width=50)
        self.volume_label.pack(side="left", padx=(0, 5))
        
        self.volume_slider = ctk.CTkSlider(
            volume_frame, 
            from_=0, 
            to=100, 
            width=100, 
            height=12,
            command=self.on_volume_change
        )
        self.volume_slider.set(50)
        self.volume_slider.pack(side="right")

    def open_search_window(self):
        """Müzik arama penceresini açar (Eğer açıksa odağa getirir)."""
        if self.search_window is None or not self.search_window.winfo_exists():
            self.search_window = SearchWindow(self)
        else:
            self.search_window.focus()
            self.search_window.lift()

    def start_download(self, url, item_id, button_widget):
        """Müzik indirme işlemini başlatır."""
        if not self.current_folder:
            tk.messagebox.showwarning("Klasör Seçilmedi", "Müzik indirebilmek için öncelikle bir klasör seçmelisiniz!")
            return
            
        button_widget.configure(state="disabled", text="Bekliyor...")
        
        self.active_downloads[item_id] = {
            "button": button_widget,
            "percent": -1,
            "status": "downloading"
        }
        
        thread = threading.Thread(
            target=self.bg_download_thread, 
            args=(url, item_id), 
            daemon=True
        )
        thread.start()

    def bg_download_thread(self, url, item_id):
        """Arka planda indirme işlemini yönetir."""
        def progress_cb(percent, status_msg):
            if item_id in self.active_downloads:
                if self.active_downloads[item_id]["status"] == "finished":
                    return
                self.active_downloads[item_id]["percent"] = percent
                if percent == 100:
                    self.active_downloads[item_id]["status"] = "finished"
                else:
                    self.active_downloads[item_id]["status"] = "downloading"
                
        success, error_msg = download_audio(url, self.current_folder, progress_cb)
        
        if success:
            if item_id in self.active_downloads:
                self.active_downloads[item_id]["percent"] = 100
                self.active_downloads[item_id]["status"] = "finished"
        else:
            if item_id in self.active_downloads:
                self.active_downloads[item_id]["status"] = "error"
                print(f"İndirme hatası ({item_id}): {error_msg}")

    def start_preview(self, url, title, button_widget):
        """Çevrimiçi önizleme oynatımını başlatır."""
        button_widget.configure(state="disabled", text="Bağlanıyor...")
        self.stop_song()
        
        thread = threading.Thread(
            target=self.bg_preview_thread,
            args=(url, title, button_widget),
            daemon=True
        )
        thread.start()

    def bg_preview_thread(self, url, title, button_widget):
        stream_url, error = get_stream_url(url)
        self.after(0, lambda: self.finish_preview_start(stream_url, title, button_widget, error))

    def finish_preview_start(self, stream_url, title, button_widget, error):
        button_widget.configure(state="normal", text="🎵 Dinle")
        
        if error or not stream_url:
            tk.messagebox.showerror("Önizleme Hatası", f"Müzik yayını başlatılamadı:\n{error}")
            return
            
        self.current_song_index = -1
        self.highlight_current_song()
        
        if self.player.play(stream_url):
            self.song_title_label.configure(text=f"[Önizleme] {title}")
            self.play_pause_button.configure(text="⏸")
            self.was_playing = True
            self.player.set_volume(self.volume_slider.get())

    def update_loop(self):
        """Süreleri, barda oynatım konumunu ve indirmeleri günceller."""
        state = self.player.get_state()
        
        # 8: MediaEnded (WMP şarkıyı bitirdi)
        if state == 8 and self.was_playing:
            self.was_playing = False
            self.play_next_song()
            
        elif state == 3 and not self.is_seeking:
            curr_pos = self.player.get_position()
            duration = self.player.get_duration()
            if duration > 0:
                self.seek_slider.configure(to=duration)
                self.seek_slider.set(curr_pos)
                self.time_label.configure(text=f"{self.format_time(curr_pos)} / {self.format_time(duration)}")

        # İndirme Durumları Güncellemesi
        finished_downloads = []
        for item_id, info in list(self.active_downloads.items()):
            btn = info["button"]
            percent = info["percent"]
            status = info["status"]
            
            try:
                if btn.winfo_exists():
                    if status == "downloading":
                        if percent >= 0:
                            btn.configure(text=f"% {percent}")
                        else:
                            btn.configure(text="İndiriliyor...")
                    elif status == "finished":
                        btn.configure(text="İndi ✅", fg_color="#059669")
                        finished_downloads.append(item_id)
                        self.refresh_local_files()
                    elif status == "error":
                        btn.configure(text="Hata ❌", fg_color="#ef4444", state="normal")
                        finished_downloads.append(item_id)
            except Exception:
                pass
                
        for item_id in finished_downloads:
            if item_id in self.active_downloads:
                del self.active_downloads[item_id]

        self.after(200, self.update_loop)

if __name__ == "__main__":
    app = MusicApp()
    app.mainloop()
