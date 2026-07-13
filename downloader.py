import yt_dlp
import os

BROWSERS = ['chrome', 'edge', 'firefox', 'brave', 'opera']

def run_ytdl_with_cookies(ydl_opts_base, action_func):
    """
    YouTube hız sınırlandırmasını (throttling) aşmak için tarayıcı çerezlerini kullanarak
    işlemi çalıştırır. Sırayla popüler tarayıcıları dener, hata alırsa çerezsiz devam eder.
    """
    for browser in BROWSERS:
        opts = ydl_opts_base.copy()
        opts['cookiesfrombrowser'] = (browser,)
        try:
            with yt_dlp.YoutubeDL(opts) as ydl:
                return action_func(ydl)
        except Exception as e:
            # Tarayıcı yüklü değilse veya kilitliyse bir sonrakini dene
            print(f"{browser} çerezleri ile deneme başarısız: {e}")
            continue
            
    # Hiçbir tarayıcı çerezi çalışmazsa, çerezsiz (fallback) çalıştır
    print("Tüm tarayıcı çerezleri başarısız oldu. Çerezsiz indirme deneniyor...")
    with yt_dlp.YoutubeDL(ydl_opts_base) as ydl:
        return action_func(ydl)

def search_youtube(query, max_results=10):
    """
    YouTube üzerinde arama yapar. Çerez kullanımı hız sınırlarını aşmayı sağlar.
    """
    ydl_opts_base = {
        'quiet': True,
        'extract_flat': True,
        'skip_download': True,
        'no_warnings': True,
    }
    
    def action(ydl):
        results = ydl.extract_info(f"ytsearch{max_results}:{query}", download=False)
        entries = results.get('entries', [])
        return [
            {
                'id': entry.get('id'),
                'title': entry.get('title', 'Bilinmeyen Başlık'),
                'duration': entry.get('duration'),  # in seconds
                'uploader': entry.get('uploader', 'Bilinmeyen Sanatçı'),
                'url': f"https://www.youtube.com/watch?v={entry.get('id')}",
                'source': 'YouTube'
            }
            for entry in entries if entry
        ]
        
    try:
        return run_ytdl_with_cookies(ydl_opts_base, action)
    except Exception as e:
        print(f"YouTube arama hatası: {e}")
        return []

def search_soundcloud(query, max_results=10):
    """
    SoundCloud üzerinde arama yapar (Çerez gerektirmez).
    """
    ydl_opts = {
        'quiet': True,
        'extract_flat': True,
        'skip_download': True,
        'no_warnings': True,
    }
    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        try:
            results = ydl.extract_info(f"scsearch{max_results}:{query}", download=False)
            entries = results.get('entries', [])
            return [
                {
                    'id': entry.get('id'),
                    'title': entry.get('title', 'Bilinmeyen Başlık'),
                    'duration': entry.get('duration'),  # in seconds
                    'uploader': entry.get('uploader', 'Bilinmeyen Sanatçı'),
                    'url': entry.get('url') or f"https://soundcloud.com/{entry.get('id')}",
                    'source': 'SoundCloud'
                }
                for entry in entries if entry
            ]
        except Exception as e:
            print(f"SoundCloud arama hatası: {e}")
            return []

def download_audio(url, download_dir, progress_callback=None):
    """
    Müziği doğrudan m4a formatında ve tarayıcı çerezlerini kullanarak en yüksek hızda indirir.
    """
    def hook(d):
        if progress_callback:
            if d['status'] == 'downloading':
                total = d.get('total_bytes') or d.get('total_bytes_estimate') or 0
                downloaded = d.get('downloaded_bytes', 0)
                if total > 0:
                    percent = int((downloaded / total) * 100)
                    progress_callback(percent, f"İndiriliyor... %{percent}")
                else:
                    progress_callback(-1, "İndiriliyor...")
            elif d['status'] == 'finished':
                progress_callback(100, "Tamamlandı!")

    ydl_opts_base = {
        'format': 'ba[ext=m4a]/ba',
        'outtmpl': os.path.join(download_dir, '%(title)s.%(ext)s'),
        'progress_hooks': [hook],
        'quiet': True,
        'no_warnings': True,
    }
    
    def action(ydl):
        ydl.download([url])
        return True, None
        
    try:
        return run_ytdl_with_cookies(ydl_opts_base, action)
    except Exception as e:
        print(f"İndirme hatası: {e}")
        return False, str(e)

def get_stream_url(url):
    """
    Önizleme için doğrudan yayın bağlantısını (stream URL) çeker.
    """
    ydl_opts_base = {
        'format': 'ba[ext=m4a]/ba',
        'quiet': True,
        'no_warnings': True,
    }
    
    def action(ydl):
        info = ydl.extract_info(url, download=False)
        return info.get('url'), None
        
    try:
        return run_ytdl_with_cookies(ydl_opts_base, action)
    except Exception as e:
        print(f"Yayın adresi alma hatası: {e}")
        return None, str(e)
