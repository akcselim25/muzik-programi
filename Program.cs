using System;
using System.IO;
using System.Diagnostics;
using System.Net;
using System.Text;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;
using System.Drawing;
using System.Collections.Generic;

namespace MYTPlayer
{
    static class Program
    {
        [DllImport("kernel32.dll")]
        private static extern bool SetProcessWorkingSetSize(IntPtr proc, int min, int max);

        public static void MinimizeMemory()
        {
            try
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
                if (Environment.OSVersion.Platform == PlatformID.Win32NT)
                {
                    SetProcessWorkingSetSize(Process.GetCurrentProcess().Handle, -1, -1);
                }
            }
            catch { }
        }

        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            
            try
            {
                ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12 | SecurityProtocolType.Tls11 | SecurityProtocolType.Tls;
            }
            catch { }

            // yt-dlp.exe kontrolü
            if (!File.Exists("yt-dlp.exe"))
            {
                DialogResult res = MessageBox.Show(
                    "Arama ve indirme motoru (yt-dlp.exe) bulunamadı.\nOtomatik indirilmesini ister misiniz? (Yaklaşık 15-20 MB)", 
                    "Sistem Kurulumu", 
                    MessageBoxButtons.YesNo, 
                    MessageBoxIcon.Question
                );
                
                if (res == DialogResult.Yes)
                {
                    Form downloadForm = new Form
                    {
                        Text = "İndiriliyor...",
                        Size = new Size(300, 120),
                        FormBorderStyle = FormBorderStyle.FixedDialog,
                        StartPosition = FormStartPosition.CenterScreen,
                        MaximizeBox = false,
                        MinimizeBox = false,
                        BackColor = Color.FromArgb(30, 30, 30),
                        ForeColor = Color.White
                    };
                    Label lbl = new Label { Text = "yt-dlp.exe indiriliyor, lütfen bekleyin...", AutoSize = true, Location = new Point(20, 20) };
                    ProgressBar pb = new ProgressBar { Location = new Point(20, 50), Size = new Size(240, 20), Style = ProgressBarStyle.Marquee };
                    downloadForm.Controls.Add(lbl);
                    downloadForm.Controls.Add(pb);

                    bool success = false;
                    string errMsg = "";

                    Thread t = new Thread(() =>
                    {
                        try
                        {
                            using (WebClient wc = new WebClient())
                            {
                                wc.DownloadFile("https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe", "yt-dlp.exe");
                            }
                            success = true;
                        }
                        catch (Exception ex)
                        {
                            errMsg = ex.Message;
                        }
                        downloadForm.Invoke((MethodInvoker)(() => downloadForm.Close()));
                    });
                    t.Start();
                    downloadForm.ShowDialog();

                    if (!success)
                    {
                        MessageBox.Show("İndirme başarısız oldu:\n" + errMsg + "\n\nProgram sadece çevrimdışı çalma modunda çalışacaktır.", "Hata", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    }
                }
            }

            MinimizeMemory();
            Application.Run(new PlayerForm());
        }
    }

    public class SearchResult
    {
        public string Id { get; set; }
        public string Title { get; set; }
        public int Duration { get; set; }
        public string Uploader { get; set; }
        public string Url { get; set; }
    }

    // DINAMIK LATE-BOUND WINDOWS MEDIA PLAYER KONTROLCÜSÜ (WPF Olmadan 1.5 MB RAM)
    public static class WMPPlayerManager
    {
        private static dynamic player;
        private static bool isInitialized = false;

        static WMPPlayerManager()
        {
            try
            {
                Type wmpType = Type.GetTypeFromProgID("WMPlayer.OCX");
                if (wmpType != null)
                {
                    player = Activator.CreateInstance(wmpType);
                    isInitialized = true;
                }
            }
            catch { }
        }

        public static bool IsInitialized { get { return isInitialized; } }

        public static bool Open(string filePathOrUrl)
        {
            if (!isInitialized) return false;
            try
            {
                player.URL = filePathOrUrl;
                return true;
            }
            catch
            {
                return false;
            }
        }

        public static void Play()
        {
            if (!isInitialized) return;
            player.controls.play();
        }

        public static void Pause()
        {
            if (!isInitialized) return;
            player.controls.pause();
        }

        public static void Close()
        {
            if (!isInitialized) return;
            player.close();
        }

        public static void SetPosition(int seconds)
        {
            if (!isInitialized) return;
            player.controls.currentPosition = (double)seconds;
        }

        public static int GetPosition()
        {
            if (!isInitialized) return 0;
            try
            {
                return (int)player.controls.currentPosition;
            }
            catch { return 0; }
        }

        public static int GetDuration()
        {
            if (!isInitialized) return 0;
            try
            {
                return (int)player.currentMedia.duration;
            }
            catch { return 0; }
        }

        public static void SetVolume(int volume)
        {
            if (!isInitialized) return;
            player.settings.volume = volume;
        }

        public static int GetPlayState()
        {
            if (!isInitialized) return 0;
            try
            {
                return (int)player.playState;
            }
            catch { return 0; }
        }
    }

    // ANA PENCERE FORM SINIFI
    public class PlayerForm : Form
    {
        private string currentFolder = "";
        private List<string> songFiles = new List<string>();
        private int currentSongIndex = -1;
        private bool isSeeking = false;
        private bool wasPlaying = false;
        private System.Windows.Forms.Timer updateTimer;
        private SearchForm searchForm = null;
        private const string CONFIG_FILE = "config.json";
        private int garbageCount = 0;

        // Kontroller
        private Button btnSelectFolder;
        private Button btnOpenSearch;
        private Label lblFolder;
        private Label lblTitle;
        private ListBox lstSongs;
        private Button btnPrev;
        private Button btnPlay;
        private Button btnStop;
        private Button btnNext;
        private TrackBar trackBarSeek;
        private Label lblTime;
        private TrackBar trackBarVolume;
        private Label lblVolume;

        public PlayerForm()
        {
            InitializeComponent();
            InitializePlayer();
            LoadConfig();
        }

        private void InitializePlayer()
        {
            if (!WMPPlayerManager.IsInitialized)
            {
                MessageBox.Show("Windows Media Player sistemi başlatılamadı. Program çalışamayabilir.", "Sistem Hatası", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }

            updateTimer = new System.Windows.Forms.Timer();
            updateTimer.Interval = 250;
            updateTimer.Tick += UpdateTimer_Tick;
            updateTimer.Start();
        }

        private void InitializeComponent()
        {
            this.Text = "Selo Player";
            this.Size = new Size(460, 600);
            this.MinimumSize = new Size(400, 500);
            this.BackColor = Color.FromArgb(20, 20, 20);
            this.ForeColor = Color.White;
            this.StartPosition = FormStartPosition.CenterScreen;

            TableLayoutPanel mainLayout = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 1,
                RowCount = 3
            };
            mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 100)); // Üst Klasör/Arama
            mainLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));  // Orta Liste
            mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 110)); // Alt Kontroller
            this.Controls.Add(mainLayout);

            // 1. ÜST PANEL
            Panel topPanel = new Panel { Dock = DockStyle.Fill };
            mainLayout.Controls.Add(topPanel, 0, 0);

            btnSelectFolder = new Button
            {
                Text = "📁 Klasör Seç",
                Location = new Point(15, 15),
                Size = new Size(130, 35),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(35, 35, 35),
                ForeColor = Color.White
            };
            btnSelectFolder.FlatAppearance.BorderColor = Color.FromArgb(50, 50, 50);
            btnSelectFolder.Click += BtnSelectFolder_Click;
            topPanel.Controls.Add(btnSelectFolder);

            btnOpenSearch = new Button
            {
                Text = "🔍 Şarkı Bul & İndir (MYT)",
                Location = new Point(155, 15),
                Size = new Size(200, 35),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(16, 185, 129),
                ForeColor = Color.White
            };
            btnOpenSearch.FlatAppearance.BorderColor = Color.FromArgb(5, 150, 105);
            btnOpenSearch.Click += BtnOpenSearch_Click;
            topPanel.Controls.Add(btnOpenSearch);

            lblFolder = new Label
            {
                Text = "Klasör Seçilmedi",
                Location = new Point(15, 60),
                Size = new Size(400, 20),
                ForeColor = Color.Gray,
                Font = new Font("Arial", 9, FontStyle.Italic)
            };
            topPanel.Controls.Add(lblFolder);

            // 2. ORTA PANEL (ŞARKI LİSTESİ)
            lstSongs = new ListBox
            {
                Dock = DockStyle.Fill,
                BackColor = Color.FromArgb(15, 15, 15),
                ForeColor = Color.FromArgb(200, 200, 200),
                BorderStyle = BorderStyle.None,
                Font = new Font("Arial", 11),
                ItemHeight = 25
            };
            lstSongs.SelectedIndexChanged += LstSongs_SelectedIndexChanged;
            mainLayout.Controls.Add(lstSongs, 0, 1);

            // 3. ALT OYNATICI PANELİ
            Panel playerPanel = new Panel { Dock = DockStyle.Fill };
            mainLayout.Controls.Add(playerPanel, 0, 2);

            lblTitle = new Label
            {
                Text = "Müzik Çalınmıyor",
                Location = new Point(15, 5),
                Size = new Size(410, 20),
                Font = new Font("Arial", 10, FontStyle.Bold),
                ForeColor = Color.FromArgb(224, 224, 224)
            };
            playerPanel.Controls.Add(lblTitle);

            // Kontrol Butonları
            btnPrev = new Button { Text = "⏮", Location = new Point(15, 30), Size = new Size(40, 30), FlatStyle = FlatStyle.Flat };
            btnPlay = new Button { Text = "▶", Location = new Point(60, 30), Size = new Size(45, 30), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(31, 83, 141) };
            btnStop = new Button { Text = "⏹", Location = new Point(110, 30), Size = new Size(40, 30), FlatStyle = FlatStyle.Flat };
            btnNext = new Button { Text = "⏭", Location = new Point(155, 30), Size = new Size(40, 30), FlatStyle = FlatStyle.Flat };

            btnPrev.FlatAppearance.BorderColor = Color.FromArgb(50, 50, 50);
            btnPlay.FlatAppearance.BorderColor = Color.FromArgb(41, 110, 180);
            btnStop.FlatAppearance.BorderColor = Color.FromArgb(50, 50, 50);
            btnNext.FlatAppearance.BorderColor = Color.FromArgb(50, 50, 50);

            btnPrev.Click += (s, e) => PlayPrevSong();
            btnPlay.Click += (s, e) => TogglePlayPause();
            btnStop.Click += (s, e) => StopSong();
            btnNext.Click += (s, e) => PlayNextSong();

            playerPanel.Controls.Add(btnPrev);
            playerPanel.Controls.Add(btnPlay);
            playerPanel.Controls.Add(btnStop);
            playerPanel.Controls.Add(btnNext);

            // Süre Slider
            trackBarSeek = new TrackBar
            {
                Location = new Point(205, 25),
                Size = new Size(160, 30),
                Minimum = 0,
                Maximum = 100,
                TickStyle = TickStyle.None,
                Height = 20
            };
            trackBarSeek.MouseDown += (s, e) => isSeeking = true;
            trackBarSeek.MouseUp += TrackBarSeek_MouseUp;
            trackBarSeek.Scroll += TrackBarSeek_Scroll;
            playerPanel.Controls.Add(trackBarSeek);

            lblTime = new Label
            {
                Text = "00:00 / 00:00",
                Location = new Point(370, 35),
                Size = new Size(70, 20),
                Font = new Font("Arial", 8)
            };
            playerPanel.Controls.Add(lblTime);

            // Ses Ayarı
            lblVolume = new Label
            {
                Text = "🔊 50%",
                Location = new Point(15, 75),
                Size = new Size(50, 20),
                Font = new Font("Arial", 8)
            };
            playerPanel.Controls.Add(lblVolume);

            trackBarVolume = new TrackBar
            {
                Location = new Point(70, 70),
                Size = new Size(100, 30),
                Minimum = 0,
                Maximum = 100,
                Value = 50,
                TickStyle = TickStyle.None,
                Height = 20
            };
            trackBarVolume.Scroll += TrackBarVolume_Scroll;
            playerPanel.Controls.Add(trackBarVolume);

            this.FormClosing += (s, e) => WMPPlayerManager.Close();
        }

        // --- DOSYA YÖNETİMİ ---

        private void LoadConfig()
        {
            if (File.Exists(CONFIG_FILE))
            {
                try
                {
                    string json = File.ReadAllText(CONFIG_FILE);
                    int idxStart = json.IndexOf("last_folder\": \"") + 15;
                    int idxEnd = json.IndexOf("\"", idxStart);
                    if (idxStart > 14 && idxEnd > idxStart)
                    {
                        string folder = json.Substring(idxStart, idxEnd - idxStart).Replace("\\\\", "\\").Replace("\\/", "/");
                        if (Directory.Exists(folder))
                        {
                            SetActiveFolder(folder);
                        }
                    }
                }
                catch { }
            }
        }

        private void SaveConfig(string folder)
        {
            try
            {
                string json = "{\n    \"last_folder\": \"" + folder.Replace("\\", "\\\\") + "\"\n}";
                File.WriteAllText(CONFIG_FILE, json);
            }
            catch { }
        }

        public void SetActiveFolder(string folder)
        {
            StopSong();
            currentFolder = folder;
            lblFolder.Text = Path.GetFileName(folder);
            RefreshLocalFiles();
            SaveConfig(folder);
            Program.MinimizeMemory();
        }

        public void RefreshLocalFiles()
        {
            if (string.IsNullOrEmpty(currentFolder) || !Directory.Exists(currentFolder))
                return;

            string[] validExtensions = { ".mp3", ".m4a", ".wav", ".flac", ".ogg" };
            var allFiles = new List<string>();
            try
            {
                foreach (var file in Directory.GetFiles(currentFolder))
                {
                    string ext = Path.GetExtension(file).ToLower();
                    foreach (var valExt in validExtensions)
                    {
                        if (ext == valExt)
                        {
                            allFiles.Add(file);
                            break;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("Klasör okunurken hata: " + ex.Message);
                return;
            }

            // Çiftleme engelleme mantığı (.mp3 > .m4a > .wav > .flac > .ogg)
            var priority = new Dictionary<string, int> {
                { ".mp3", 5 }, { ".m4a", 4 }, { ".wav", 3 }, { ".flac", 2 }, { ".ogg", 1 }
            };
            var uniqueSongs = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

            foreach (var filePath in allFiles)
            {
                string baseName = Path.GetFileNameWithoutExtension(filePath);
                string ext = Path.GetExtension(filePath).ToLower();

                if (!uniqueSongs.ContainsKey(baseName))
                {
                    uniqueSongs[baseName] = filePath;
                }
                else
                {
                    string currentPath = uniqueSongs[baseName];
                    string currentExt = Path.GetExtension(currentPath).ToLower();
                    if (priority.ContainsKey(ext) && priority.ContainsKey(currentExt))
                    {
                        if (priority[ext] > priority[currentExt])
                        {
                            uniqueSongs[baseName] = filePath;
                        }
                    }
                }
            }

            var sortedBases = new List<string>(uniqueSongs.Keys);
            sortedBases.Sort();

            songFiles.Clear();
            lstSongs.Items.Clear();

            foreach (var baseName in sortedBases)
            {
                string fullPath = uniqueSongs[baseName];
                songFiles.Add(fullPath);
                lstSongs.Items.Add("🎵  " + Path.GetFileName(fullPath));
            }
        }

        // --- SES OYNATICI KONTROLÜ ---

        private void LstSongs_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (lstSongs.SelectedIndex != -1)
            {
                PlaySongAtIndex(lstSongs.SelectedIndex);
            }
        }

        private void PlaySongAtIndex(int index)
        {
            if (songFiles.Count == 0 || index < 0 || index >= songFiles.Count)
                return;

            currentSongIndex = index;
            string path = songFiles[index];
            lblTitle.Text = Path.GetFileName(path);

            if (WMPPlayerManager.Open(path))
            {
                WMPPlayerManager.Play();
                WMPPlayerManager.SetVolume(trackBarVolume.Value);
                btnPlay.Text = "⏸";
                wasPlaying = true;
                Program.MinimizeMemory();
            }
            else
            {
                MessageBox.Show("Ses oynatılamadı.", "Hata", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        public void PlayStream(string streamUrl, string title)
        {
            StopSong();
            currentSongIndex = -1;
            lstSongs.ClearSelected();
            lblTitle.Text = "[Önizleme] " + title;

            if (WMPPlayerManager.Open(streamUrl))
            {
                WMPPlayerManager.Play();
                WMPPlayerManager.SetVolume(trackBarVolume.Value);
                btnPlay.Text = "⏸";
                wasPlaying = true;
                Program.MinimizeMemory();
            }
            else
            {
                MessageBox.Show("Yayın oynatılamadı.", "Hata", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void TogglePlayPause()
        {
            if (currentSongIndex == -1 && songFiles.Count > 0)
            {
                PlaySongAtIndex(0);
                return;
            }

            if (wasPlaying)
            {
                WMPPlayerManager.Pause();
                btnPlay.Text = "▶";
                wasPlaying = false;
            }
            else
            {
                WMPPlayerManager.Play();
                btnPlay.Text = "⏸";
                wasPlaying = true;
            }
            Program.MinimizeMemory();
        }

        private void StopSong()
        {
            wasPlaying = false;
            WMPPlayerManager.Pause(); // duraklatır ve yerini korur (başa sarmaz)
            btnPlay.Text = "▶";
            Program.MinimizeMemory();
        }

        private void PlayNextSong()
        {
            if (songFiles.Count == 0) return;
            int nextIdx = (currentSongIndex + 1) % songFiles.Count;
            PlaySongAtIndex(nextIdx);
        }

        private void PlayPrevSong()
        {
            if (songFiles.Count == 0) return;
            int prevIdx = currentSongIndex - 1;
            if (prevIdx < 0) prevIdx = songFiles.Count - 1;
            PlaySongAtIndex(prevIdx);
        }

        private void TrackBarSeek_Scroll(object sender, EventArgs e)
        {
            int dur = WMPPlayerManager.GetDuration();
            lblTime.Text = FormatTime(trackBarSeek.Value) + " / " + FormatTime(dur);
        }

        private void TrackBarSeek_MouseUp(object sender, MouseEventArgs e)
        {
            WMPPlayerManager.SetPosition(trackBarSeek.Value);
            isSeeking = false;
            Program.MinimizeMemory();
        }

        private void TrackBarVolume_Scroll(object sender, EventArgs e)
        {
            WMPPlayerManager.SetVolume(trackBarVolume.Value);
            lblVolume.Text = "🔊 " + trackBarVolume.Value + "%";
        }

        private void UpdateTimer_Tick(object sender, EventArgs e)
        {
            // Windows Media Player durum takibi
            int state = WMPPlayerManager.GetPlayState();
            
            // 8: wmppsMediaEnded (Şarkı bitti)
            if (state == 8 && wasPlaying)
            {
                wasPlaying = false;
                if (currentSongIndex != -1)
                {
                    PlayNextSong();
                    return;
                }
            }

            if (wasPlaying && !isSeeking)
            {
                int pos = WMPPlayerManager.GetPosition();
                int dur = WMPPlayerManager.GetDuration();
                if (dur > 0)
                {
                    trackBarSeek.Maximum = dur;
                    trackBarSeek.Value = Math.Min(pos, dur);
                    lblTime.Text = FormatTime(pos) + " / " + FormatTime(dur);
                }
            }

            // Periyodik bellek optimizasyonu
            garbageCount++;
            if (garbageCount >= 60)
            {
                garbageCount = 0;
                Program.MinimizeMemory();
            }
        }

        private string FormatTime(double seconds)
        {
            int mins = (int)(seconds / 60);
            int secs = (int)(seconds % 60);
            return string.Format("{0:00}:{1:00}", mins, secs);
        }

        private void BtnSelectFolder_Click(object sender, EventArgs e)
        {
            using (FolderBrowserDialog fbd = new FolderBrowserDialog())
            {
                if (fbd.ShowDialog() == DialogResult.OK)
                {
                    SetActiveFolder(fbd.SelectedPath);
                }
            }
        }

        private void BtnOpenSearch_Click(object sender, EventArgs e)
        {
            if (!File.Exists("yt-dlp.exe"))
            {
                MessageBox.Show("Arama motoru (yt-dlp.exe) indirilmeden arama penceresi açılamaz.", "Uyarı", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (searchForm == null || searchForm.IsDisposed)
            {
                searchForm = new SearchForm(this, currentFolder);
                searchForm.Show();
            }
            else
            {
                searchForm.Focus();
            }
        }
    }

    // ARAMA VE İNDİRME PENCERESİ FORM SINIFI
    public class SearchForm : Form
    {
        private PlayerForm parent;
        private string activeFolder;
        
        // Kontroller
        private TextBox txtSearch;
        private Button btnSearch;
        private TabControl tabControl;
        private TabPage tabYT;
        private TabPage tabSC;
        private ListView lstYT;
        private ListView lstSC;
        private Button btnPreview;
        private Button btnDownload;
        private Label lblStatus;

        private List<SearchResult> ytResults = new List<SearchResult>();
        private List<SearchResult> scResults = new List<SearchResult>();

        public SearchForm(PlayerForm parent, string activeFolder)
        {
            this.parent = parent;
            this.activeFolder = activeFolder;
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            this.Text = "Şarkı Bul & İndir (MYT)";
            this.Size = new Size(650, 520);
            this.MinimumSize = new Size(550, 420);
            this.BackColor = Color.FromArgb(20, 20, 20);
            this.ForeColor = Color.White;
            this.StartPosition = FormStartPosition.CenterScreen;

            TableLayoutPanel layout = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 1,
                RowCount = 3
            };
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));  // Arama Barı
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));  // TabControl
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));  // Alt Butonlar
            this.Controls.Add(layout);

            // 1. ÜST ARAMA BARI PANELİ
            Panel topPanel = new Panel { Dock = DockStyle.Fill };
            layout.Controls.Add(topPanel, 0, 0);

            txtSearch = new TextBox
            {
                Location = new Point(15, 15),
                Size = new Size(380, 30),
                BackColor = Color.FromArgb(35, 35, 35),
                ForeColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle,
                Font = new Font("Arial", 11)
            };
            txtSearch.KeyDown += (s, e) => { if (e.KeyCode == Keys.Enter) StartSearch(); };
            topPanel.Controls.Add(txtSearch);

            btnSearch = new Button
            {
                Text = "🔍 Şarkı Ara",
                Location = new Point(410, 13),
                Size = new Size(110, 30),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(31, 83, 141),
                ForeColor = Color.White
            };
            btnSearch.FlatAppearance.BorderColor = Color.FromArgb(41, 110, 180);
            btnSearch.Click += (s, e) => StartSearch();
            topPanel.Controls.Add(btnSearch);

            // 2. ORTA PANEL (TAB CONTROL & LIST VIEWLER)
            tabControl = new TabControl { Dock = DockStyle.Fill };
            layout.Controls.Add(tabControl, 0, 1);

            tabYT = new TabPage { Text = "YouTube", BackColor = Color.FromArgb(20, 20, 20) };
            tabSC = new TabPage { Text = "SoundCloud", BackColor = Color.FromArgb(20, 20, 20) };
            tabControl.TabPages.Add(tabYT);
            tabControl.TabPages.Add(tabSC);

            // ListView YouTube
            lstYT = CreateResultListView();
            tabYT.Controls.Add(lstYT);

            // ListView SoundCloud
            lstSC = CreateResultListView();
            tabSC.Controls.Add(lstSC);

            // 3. ALT KONTROLLER PANELİ
            Panel bottomPanel = new Panel { Dock = DockStyle.Fill };
            layout.Controls.Add(bottomPanel, 0, 2);

            btnPreview = new Button
            {
                Text = "🎵 Hızlı Dinle (Önizle)",
                Location = new Point(15, 12),
                Size = new Size(180, 35),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(31, 83, 141),
                ForeColor = Color.White,
                Font = new Font("Arial", 9, FontStyle.Bold)
            };
            btnPreview.FlatAppearance.BorderColor = Color.FromArgb(41, 110, 180);
            btnPreview.Click += (s, e) => StartPreview();
            bottomPanel.Controls.Add(btnPreview);

            btnDownload = new Button
            {
                Text = "📥 Klasöre İndir",
                Location = new Point(205, 12),
                Size = new Size(160, 35),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(16, 185, 129),
                ForeColor = Color.White,
                Font = new Font("Arial", 9, FontStyle.Bold)
            };
            btnDownload.FlatAppearance.BorderColor = Color.FromArgb(5, 150, 105);
            btnDownload.Click += (s, e) => StartDownload();
            bottomPanel.Controls.Add(btnDownload);

            lblStatus = new Label
            {
                Text = "",
                Location = new Point(380, 20),
                Size = new Size(240, 25),
                ForeColor = Color.FromArgb(10, 200, 10),
                Font = new Font("Arial", 9, FontStyle.Bold)
            };
            bottomPanel.Controls.Add(lblStatus);

            this.FormClosing += (s, e) => Program.MinimizeMemory();
        }

        private ListView CreateResultListView()
        {
            ListView lv = new ListView
            {
                Dock = DockStyle.Fill,
                View = View.Details,
                FullRowSelect = true,
                BackColor = Color.FromArgb(15, 15, 15),
                ForeColor = Color.FromArgb(200, 200, 200),
                BorderStyle = BorderStyle.None,
                HeaderStyle = ColumnHeaderStyle.Nonclickable,
                Font = new Font("Arial", 10)
            };
            lv.Columns.Add("Şarkı Adı / Video Başlığı", 340);
            lv.Columns.Add("Sanatçı / Kanal", 130);
            lv.Columns.Add("Süre", 80);
            return lv;
        }

        // --- ARAMA KODU ---

        private void StartSearch()
        {
            string query = txtSearch.Text.Trim();
            if (string.IsNullOrEmpty(query)) return;

            btnSearch.Enabled = false;
            btnSearch.Text = "Aranıyor...";
            lblStatus.Text = "Şarkılar aranıyor...";

            lstYT.Items.Clear();
            lstSC.Items.Clear();

            Thread t = new Thread(() =>
            {
                ytResults = SearchYTDLP(query, true);
                scResults = SearchYTDLP(query, false);

                this.Invoke((MethodInvoker)(() =>
                {
                    PopulateListView(lstYT, ytResults);
                    PopulateListView(lstSC, scResults);
                    
                    btnSearch.Enabled = true;
                    btnSearch.Text = "🔍 Şarkı Ara";
                    lblStatus.Text = "";
                    Program.MinimizeMemory();
                }));
            });
            t.Start();
        }

        private void PopulateListView(ListView lv, List<SearchResult> results)
        {
            foreach (var item in results)
            {
                ListViewItem lvi = new ListViewItem(item.Title);
                lvi.SubItems.Add(item.Uploader);
                
                int mins = item.Duration / 60;
                int secs = item.Duration % 60;
                lvi.SubItems.Add(string.Format("{0:02d}:{1:02d}", mins, secs));
                
                lvi.Tag = item;
                lv.Items.Add(lvi);
            }
        }

        private List<SearchResult> SearchYTDLP(string query, bool isYouTube)
        {
            var list = new List<SearchResult>();
            try
            {
                string prefix = isYouTube ? "ytsearch10:" : "scsearch10:";
                
                string[] browsers = { "chrome", "edge", "firefox", "brave", "opera" };
                bool searchSuccess = false;

                foreach (var browser in browsers)
                {
                    try
                    {
                        ProcessStartInfo psiCheck = new ProcessStartInfo
                        {
                            FileName = "yt-dlp.exe",
                            Arguments = string.Format("--quiet --extract-flat --skip-download --cookies-from-browser {0} --print \"%(id)s|||%(title)s|||%(duration)s|||%(uploader)s\" \"{1}{2}\"", browser, prefix, query.Replace("\"", "\\\"")),
                            UseShellExecute = false,
                            RedirectStandardOutput = true,
                            CreateNoWindow = true,
                            StandardOutputEncoding = System.Text.Encoding.UTF8
                        };
                        
                        using (Process p = Process.Start(psiCheck))
                        {
                            string line;
                            while ((line = p.StandardOutput.ReadLine()) != null)
                            {
                                var parts = line.Split(new[] { "|||" }, StringSplitOptions.None);
                                if (parts.Length >= 4)
                                {
                                    searchSuccess = true;
                                    AddPartsToList(list, parts, isYouTube);
                                }
                            }
                            p.WaitForExit();
                        }
                        if (searchSuccess) break;
                    }
                    catch { }
                }

                if (!searchSuccess)
                {
                    ProcessStartInfo psi = new ProcessStartInfo
                    {
                        FileName = "yt-dlp.exe",
                        Arguments = string.Format("--quiet --extract-flat --skip-download --print \"%(id)s|||%(title)s|||%(duration)s|||%(uploader)s\" \"{0}{1}\"", prefix, query.Replace("\"", "\\\"")),
                        UseShellExecute = false,
                        RedirectStandardOutput = true,
                        CreateNoWindow = true,
                        StandardOutputEncoding = System.Text.Encoding.UTF8
                    };

                    using (Process p = Process.Start(psi))
                    {
                        string line;
                        while ((line = p.StandardOutput.ReadLine()) != null)
                        {
                            var parts = line.Split(new[] { "|||" }, StringSplitOptions.None);
                            AddPartsToList(list, parts, isYouTube);
                        }
                        p.WaitForExit();
                    }
                }
            }
            catch { }
            return list;
        }

        private void AddPartsToList(List<SearchResult> list, string[] parts, bool isYouTube)
        {
            if (parts.Length >= 4)
            {
                string id = parts[0];
                string title = parts[1];
                string durStr = parts[2];
                string uploader = parts[3];

                int duration = 0;
                int.TryParse(durStr, out duration);

                string url = isYouTube ? "https://www.youtube.com/watch?v=" + id : id;

                list.Add(new SearchResult
                {
                    Id = id,
                    Title = title,
                    Duration = duration,
                    Uploader = uploader,
                    Url = url
                });
            }
        }

        // --- HIZLI ÖNİZLEME (STREAMING MANTIĞI) ---

        private void StartPreview()
        {
            ListView activeLV = tabControl.SelectedIndex == 0 ? lstYT : lstSC;
            if (activeLV.SelectedItems.Count == 0) return;

            SearchResult item = (SearchResult)activeLV.SelectedItems[0].Tag;
            btnPreview.Enabled = false;
            btnPreview.Text = "Bağlanıyor...";
            lblStatus.Text = "Yayın adresi alınıyor...";

            Thread t = new Thread(() =>
            {
                string streamUrl = GetStreamUrl(item.Url);
                
                this.Invoke((MethodInvoker)(() =>
                {
                    btnPreview.Enabled = true;
                    btnPreview.Text = "🎵 Hızlı Dinle (Önizle)";
                    lblStatus.Text = "";
                    
                    if (!string.IsNullOrEmpty(streamUrl))
                    {
                        parent.PlayStream(streamUrl, item.Title);
                    }
                    else
                    {
                        MessageBox.Show("Önizleme adresi alınamadı. İnternet bağlantısını kontrol edin.", "Hata", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    Program.MinimizeMemory();
                }));
            });
            t.Start();
        }

        private string GetStreamUrl(string url)
        {
            string[] browsers = { "chrome", "edge", "firefox", "brave", "opera" };
            foreach (var browser in browsers)
            {
                try
                {
                    ProcessStartInfo psi = new ProcessStartInfo
                    {
                        FileName = "yt-dlp.exe",
                        Arguments = string.Format("--format \"ba[ext=m4a]/ba\" --cookies-from-browser {0} --get-url \"{1}\"", browser, url),
                        UseShellExecute = false,
                        RedirectStandardOutput = true,
                        CreateNoWindow = true,
                        StandardOutputEncoding = System.Text.Encoding.UTF8
                    };
                    using (Process p = Process.Start(psi))
                    {
                        string output = p.StandardOutput.ReadToEnd().Trim();
                        p.WaitForExit();
                        if (p.ExitCode == 0 && !string.IsNullOrEmpty(output))
                            return output;
                    }
                }
                catch { }
            }

            try
            {
                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = "yt-dlp.exe",
                    Arguments = string.Format("--format \"ba[ext=m4a]/ba\" --get-url \"{0}\"", url),
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    CreateNoWindow = true,
                    StandardOutputEncoding = System.Text.Encoding.UTF8
                };
                using (Process p = Process.Start(psi))
                {
                    string output = p.StandardOutput.ReadToEnd().Trim();
                    p.WaitForExit();
                    if (p.ExitCode == 0)
                        return output;
                }
            }
            catch { }
            return null;
        }

        // --- İNDİRME ---

        private void StartDownload()
        {
            if (string.IsNullOrEmpty(activeFolder) || !Directory.Exists(activeFolder))
            {
                MessageBox.Show("İndirmeden önce ana ekrandan bir müzik klasörü seçmelisiniz!", "Hata", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            ListView activeLV = tabControl.SelectedIndex == 0 ? lstYT : lstSC;
            if (activeLV.SelectedItems.Count == 0) return;

            SearchResult item = (SearchResult)activeLV.SelectedItems[0].Tag;
            btnDownload.Enabled = false;
            lblStatus.Text = "İndirme başlatılıyor...";

            Thread t = new Thread(() =>
            {
                bool success = DownloadProcess(item.Url, activeFolder);
                this.Invoke((MethodInvoker)(() =>
                {
                    btnDownload.Enabled = true;
                    if (success)
                    {
                        lblStatus.Text = "Tamamlandı! Şarkı klasöre eklendi.";
                        parent.RefreshLocalFiles();
                    }
                    else
                    {
                        lblStatus.Text = "İndirme başarısız oldu!";
                    }
                    Program.MinimizeMemory();
                }));
            });
            t.Start();
        }

        private bool DownloadProcess(string url, string outputFolder)
        {
            string[] browsers = { "chrome", "edge", "firefox", "brave", "opera" };
            bool success = false;

            foreach (var browser in browsers)
            {
                try
                {
                    ProcessStartInfo psi = new ProcessStartInfo
                    {
                        FileName = "yt-dlp.exe",
                        Arguments = string.Format("--format \"ba[ext=m4a]/ba\" --cookies-from-browser {0} --outtmpl \"{1}\\%(title)s.\\%(ext)s\" \"{2}\"", browser, outputFolder, url),
                        UseShellExecute = false,
                        RedirectStandardOutput = true,
                        CreateNoWindow = true,
                        StandardOutputEncoding = System.Text.Encoding.UTF8
                    };

                    using (Process p = Process.Start(psi))
                    {
                        string line;
                        while ((line = p.StandardOutput.ReadLine()) != null)
                        {
                            ParseProgress(line);
                        }
                        p.WaitForExit();
                        if (p.ExitCode == 0)
                        {
                            success = true;
                            break;
                        }
                    }
                }
                catch { }
            }

            if (!success)
            {
                try
                {
                    ProcessStartInfo psi = new ProcessStartInfo
                    {
                        FileName = "yt-dlp.exe",
                        Arguments = string.Format("--format \"ba[ext=m4a]/ba\" --outtmpl \"{0}\\%(title)s.\\%(ext)s\" \"{1}\"", outputFolder, url),
                        UseShellExecute = false,
                        RedirectStandardOutput = true,
                        CreateNoWindow = true,
                        StandardOutputEncoding = System.Text.Encoding.UTF8
                    };

                    using (Process p = Process.Start(psi))
                    {
                        string line;
                        while ((line = p.StandardOutput.ReadLine()) != null)
                        {
                            ParseProgress(line);
                        }
                        p.WaitForExit();
                        success = (p.ExitCode == 0);
                    }
                }
                catch { }
            }

            return success;
        }

        private void ParseProgress(string line)
        {
            if (line.Contains("[download]") && line.Contains("%"))
            {
                try
                {
                    int idxPercent = line.IndexOf('%');
                    int idxStart = line.LastIndexOf(' ', idxPercent);
                    if (idxStart != -1)
                    {
                        string pctStr = line.Substring(idxStart + 1, idxPercent - idxStart - 1).Trim();
                        double pct = double.Parse(pctStr, System.Globalization.CultureInfo.InvariantCulture);
                        this.Invoke((MethodInvoker)(() =>
                        {
                            lblStatus.Text = string.Format("İndiriliyor... %{0}", (int)pct);
                        }));
                    }
                }
                catch { }
            }
        }
    }
}
