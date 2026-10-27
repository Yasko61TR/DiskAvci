# DiskAvcı Tasarım Dokümanı

Sürüm: taslak 0.1 · Tarih: 2026-10-26 · Yazar: Yasin Uzun

Bu belge kod içermez; tasarım kararlarını, şema taslaklarını ve planı tanımlar. SQL ve komut örnekleri tasarımın parçasıdır.

## İçindekiler

1. Hedefler ve kapsam
2. Gereksinimler
3. Modül mimarisi
4. Bağımlılıklar ve lisans notları
5. SQLite şeması
6. Örnek sorgular
7. Komut satırı arayüzü
8. Algoritmalar
9. Thread modeli
10. Test stratejisi
11. CI (GitHub Actions)
12. 6 haftalık plan
13. Doğrulama notları

---

## 1. Hedefler ve kapsam

**Amaç:** Bir dizin ağacını tara, sonucu SQLite'a *snapshot* olarak kaydet, sorgula, karşılaştır, kopya bul, rapor üret. Dosya silme yok.

**İkincil amaç (CV):** Gerçek veri modeli, eşzamanlılık, ölçüm ve çok platformlu CI gösteren, 6 haftada bitebilecek bir proje.

**Kapsam içi:** CLI, `std::filesystem` tabanlı tarama, SQLite, kopya bulma, snapshot farkı, tek dosyalık HTML treemap, temizlik önerisi listesi.

**Kapsam dışı (kesin):** GUI, silme/taşıma/yeniden adlandırma, sunucu/web uygulaması, bulut, NTFS MFT'yi doğrudan okuma, gerçek zamanlı izleme.

**Kapsam şişmesi freni:** Her yeni fikir `docs/FIKIRLER.md` benzeri bir listeye yazılır; mevcut aşama bitmeden hiçbiri başlatılmaz. Hafta 2 sonunda yayınlanabilir bir v0.1 şarttır (bkz. Bölüm 12).

## 2. Gereksinimler

### 2.1 İşlevsel

| No | Gereksinim | Aşama |
|---|---|---|
| F1 | Verilen kökten özyinelemeli tarama; dosya ve klasör sayısı, boyut | 1 |
| F2 | Hatalar (izin, kilit, bozuk link, döngü) taramayı durdurmaz; sayılıp nedenleriyle kaydedilir | 1 |
| F3 | Sonuç `snapshot` olarak SQLite'a yazılır | 1 |
| F4 | En büyük N dosya ve N klasör sorgusu | 1 |
| F5 | Uzantıya göre dağılım (adet, toplam boyut, yüzde) | 1 |
| F6 | Hard link'ler tek sayılır (aynı dosya kimliği) | 1 |
| F7 | Çok iş parçacıklı tarama; `--threads N` | 2 |
| F8 | Kopya dosya bulma (boyut → ilk 4 KiB hash → tam hash) | 2 |
| F9 | Yeniden üretilebilir klasör önerisi (yalnızca listeleme) | 2 |
| F10 | İki snapshot farkı ve `--since 7d` kısayolu | 3 |
| F11 | Tek dosyalık HTML treemap (gömülü JSON, sunucusuz) | 3 |

### 2.2 İşlevsel olmayan

| No | Gereksinim |
|---|---|
| N1 | Windows ve Linux'ta derlenir ve testler geçer (CI) |
| N2 | Türkçe karakterli yollar doğru taranır, saklanır, yazdırılır |
| N3 | Bellek kullanımı dizin boyutuyla doğrusal artmaz (akış) — hedef: ölçülüp README'ye yazılacak |
| N4 | Veritabanı yazma işlemi toplu (transaction batching) |
| N5 | Hiçbir kod yolu dosya sistemini değiştirmez (veritabanı dosyası hariç) |
| N6 | Aynı veriyle aynı sonuç (deterministik sıralama) |
| N7 | Her komut için `--help`; hatalarda anlaşılır Türkçe mesaj |

### 2.3 Başarı ölçütleri (ölçülebilir)

- Boş olmayan gerçek bir dizinde (kendi `home` klasörün) tarama hatasız bitiyor, atlanan yollar sayılıyor.
- Aynı dizin iki kez taranınca `diff` değişiklik yok diyor (değişen dosya yoksa).
- Benchmark tablosu gerçek ölçümle dolu ve yöntem tekrarlanabilir.
- CI iki işletim sisteminde yeşil.

## 3. Modül mimarisi

Dizin düzeni (taslak):

```text
DiskAvci/
  CMakeLists.txt
  src/
    core/      (tarama, platform katmanı)
    storage/   (SQLite sarmalayıcı, şema, sorgular)
    analysis/  (kopya, fark, temizlik önerisi)
    report/    (HTML + JSON)
    cli/       (argüman, komutlar, biçimlendirme)
    util/      (thread pool, kuyruk, zaman, log)
  tests/
  docs/
  third_party/ veya FetchContent
```

### 3.1 Sınıf listesi ve sorumluluklar

**core**

| Sınıf / yapı | Sorumluluk |
|---|---|
| `ScanOptions` | Kök yol, thread sayısı, `one_fs`, `follow_symlinks` (varsayılan kapalı), dışlama desenleri, hash kapsamı |
| `EntryInfo` | Bir dizin girdisinin ham bilgisi: ad (UTF-8), tür, boyut (görünür/diskteki), mtime, dosya kimliği, bağlantı sayısı, bayraklar |
| `ScanError` / `ErrorCollector` | Hata kategorisi (izin, kilit, döngü, uzun yol, diğer), yol, mesaj; sayaçlar; kayıt sınırı (milyonlarca hata belleği doldurmasın) |
| `IScanner` | Arayüz: `scan(options, sink)` |
| `SequentialScanner` | Aşama 1: tek iş parçacıklı DFS, `error_code` overload'ları ile |
| `ParallelScanner` | Aşama 2: klasör-görevi tabanlı havuz |
| `PlatformFs` | Platforma özgü her şey burada: dosya kimliği (dev+inode / volume serial + file index), diskteki boyut, bağlantı sayısı, reparse/junction tespiti, çevrimdışı (bulut) dosya bayrağı. `#ifdef` yalnızca bu dosyada |
| `PathCodec` | `std::filesystem::path` ↔ UTF-8 `std::string`; geçersiz dizileri güvenli işleme (bkz. Sorun 3) |

**storage**

| Sınıf | Sorumluluk |
|---|---|
| `Database` | `sqlite3*` için RAII; açma, PRAGMA ayarları, kapatma; hata → istisna veya `Result` |
| `Statement` | Hazırlanmış sorgu RAII; bind/step/reset |
| `Schema` | Şema oluşturma ve `PRAGMA user_version` ile sürümleme/migrasyon |
| `SnapshotWriter` | Toplu ekleme (N satırda bir COMMIT), `snapshot` yaşam döngüsü (`running` → `complete`/`failed`) |
| `Queries` | Hazır sorgular: top-N, uzantı dağılımı, kopyalar, fark |

**analysis**

| Sınıf | Sorumluluk |
|---|---|
| `IHasher` | `begin / update / finish` arayüzü |
| `XxHasher` / `Blake3Hasher` | Uygulamalar; seçim `--hash xxh3\|blake3` (bkz. Bölüm 4) |
| `DuplicateFinder` | Kopya hattının üç aşaması (Bölüm 8.2) |
| `DiffEngine` | İki snapshot'ın klasör ve dosya farkı (Bölüm 8.3) |
| `CleanupAdvisor` | Kural tabanlı öneri; yalnızca okur, listeler |

**report**

| Sınıf | Sorumluluk |
|---|---|
| `TreemapDataBuilder` | Snapshot'tan sınırlı derinlik/düğüm sayısı ile ağaç JSON'u üretir, küçükleri "diğer" altında toplar |
| `HtmlReport` | Şablona JSON'u güvenli gömer (kaçışlama), tek dosya yazar |
| `JsonWriter` | Küçük, bağımlılıksız JSON yazıcı (veya kütüphane; bkz. Bölüm 4) |

**cli**

| Sınıf | Sorumluluk |
|---|---|
| `ArgParser` | Alt komut + seçenek ayrıştırma |
| `Commands` | `scan`, `snapshots`, `top`, `ext`, `dups`, `diff`, `report`, `cleanup-suggest` |
| `TableFormatter` / `SizeFormatter` | Hizalı tablo, UTF-8 genişlik, "1,5 GB" biçimi |

**util**

| Sınıf | Sorumluluk |
|---|---|
| `ThreadPool` | Sabit sayıda işçi, görev kuyruğu, düzgün kapanış |
| `BoundedQueue<T>` | Üretici-tüketici kuyruğu; geri basınç (kuyruk doluysa üretici bekler) |
| `Timer`, `Log` | Süre ölçümü, seviyeli log |

### 3.2 Bağımlılık kuralları

- `cli` → her şeyi kullanabilir; `core` hiçbir üst katmanı bilmez.
- `storage` ve `core` birbirini bilmez; `SnapshotWriter` bir `ISink` arayüzünü uygular, tarayıcı yalnızca `ISink`'e yazar. Bu sayede tarayıcı veritabanı olmadan test edilir.
- Platforma özgü kod yalnızca `PlatformFs` içinde.

## 4. Bağımlılıklar ve lisans notları

| Bileşen | Kullanım | Lisans notu | Doğrulama |
|---|---|---|---|
| SQLite | Veritabanı | Public domain olarak bilinir | Bu oturumda doğrulanmadı |
| xxHash | Hızlı hash (XXH3, 64/128 bit) | Kütüphane BSD, `xxhsum` aracı GPL (yalnızca kütüphane dosyalarını kullan) | Eski bir sürüm README'sinden doğrulandı; güncel sürüm için depoya bak |
| BLAKE3 | Alternatif hash | CC0 1.0 veya Apache 2.0 (Apache 2.0 LLVM istisnalı seçenek de var); depoda C gerçekleştirimi `c` dizininde | Doğrulandı. CMake desteği sayfadan **doğrulanmadı** |
| GoogleTest veya Catch2 | Test | Lisansları bu oturumda doğrulanmadı | Doğrulanmadı |
| (isteğe bağlı) CLI ayrıştırıcı kütüphanesi | Argüman | Seçilecekse lisans ve sürüm kontrol edilmeli | Doğrulanmadı |

**Hash seçimi:** Kopya bulma için `xxh3-128` yeterli hız ve düşük çakışma ihtimali sağlar, ancak kriptografik değildir (bu bilgi kullanılan README sayfasında geçmiyor; genel kabul gören bilgidir, doğrulanmadı). Basitlik için tek hash seçilebilir. Önerilen karar: varsayılan **BLAKE3** (tek bağımlılık, güçlü çakışma direnci, çok çekirdekli SIMD'den yararlanır — bu performans iddiası doğrulanmadı, benchmark ile ölç), `--hash xxh3` isteğe bağlı. Her iki durumda "kopya" iddiası kullanıcıya *"aynı hash"* olarak sunulur; ihtiyatlı mod `--verify` ile bayt bayt karşılaştırma yapar (Aşama 2 sonu, isteğe bağlı).

Sürüm sabitleme: CMake `FetchContent` kullanılacaksa her bağımlılık **sürüm etiketi veya commit hash** ile sabitlenir ("latest" yok). Güncel sürüm numaralarını proje başında depodan kontrol et; bu belgede sürüm numarası verilmedi çünkü doğrulanmadı.

## 5. SQLite şeması

Tasarım ilkeleri:

- Ağaç `parent_id` ile (adjacency list). Klasör toplamları recursive CTE ile hesaplanır ve tarama sonunda `directory.total_size` sütununa **önbellek** olarak yazılır (okuma hızlı, doğruluk CTE ile test edilir).
- Dosya içeriği (hash) ayrı tabloda: aynı içerik farklı dosyalardan tek satıra bağlanır. Kopya bulma bir `GROUP BY ... HAVING` olur.
- Bütün tablolar `snapshot_id` ile bağlıdır; bir snapshot silinince hepsi `ON DELETE CASCADE` ile gider (bu yalnızca *veritabanı satırlarını* siler, dosya sistemine dokunmaz).
- Yollar UTF-8 `TEXT`; geçersiz UTF-8 için bayrak (bkz. Sorun 3).

> Taslak. `PRAGMA foreign_keys = ON` her bağlantıda açılmalıdır (SQLite'ta varsayılan kapalıdır; bu davranış bilinen bir noktadır, oturumda yeniden doğrulanmadı).

```sql
-- Sürümleme: PRAGMA user_version = 1;

CREATE TABLE snapshot (
  id            INTEGER PRIMARY KEY,
  root_path     TEXT    NOT NULL,           -- taranan kök (UTF-8)
  started_at    INTEGER NOT NULL,           -- unix saniye (UTC)
  finished_at   INTEGER,                    -- NULL = bitmedi / çöktü
  status        TEXT    NOT NULL DEFAULT 'running'
                CHECK (status IN ('running','complete','failed')),
  tool_version  TEXT    NOT NULL,
  platform      TEXT    NOT NULL,           -- 'windows' | 'linux'
  threads       INTEGER NOT NULL DEFAULT 1,
  total_files   INTEGER NOT NULL DEFAULT 0,
  total_dirs    INTEGER NOT NULL DEFAULT 0,
  total_size    INTEGER NOT NULL DEFAULT 0, -- benzersiz dosya kimliklerine göre
  skipped_count INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE directory (
  id          INTEGER PRIMARY KEY,
  snapshot_id INTEGER NOT NULL REFERENCES snapshot(id) ON DELETE CASCADE,
  parent_id   INTEGER REFERENCES directory(id),   -- kök için NULL
  name        TEXT    NOT NULL,
  rel_path    TEXT    NOT NULL,                   -- kökten göreli, '/' ayraçlı
  depth       INTEGER NOT NULL,
  name_is_raw INTEGER NOT NULL DEFAULT 0,         -- 1: ad geçerli UTF-8 değildi (bkz. SORUNLAR #3)
  total_size  INTEGER,                            -- önbellek, tarama sonunda doldurulur
  total_files INTEGER,
  UNIQUE (snapshot_id, rel_path)
);

CREATE TABLE content_hash (
  id        INTEGER PRIMARY KEY,
  algo      TEXT    NOT NULL,                    -- 'blake3' | 'xxh3-128'
  size      INTEGER NOT NULL,
  full_hash BLOB    NOT NULL,
  UNIQUE (algo, size, full_hash)
);

CREATE TABLE file (
  id              INTEGER PRIMARY KEY,
  snapshot_id     INTEGER NOT NULL REFERENCES snapshot(id) ON DELETE CASCADE,
  dir_id          INTEGER NOT NULL REFERENCES directory(id) ON DELETE CASCADE,
  name            TEXT    NOT NULL,
  ext             TEXT,                           -- küçük harf, noktasız; yoksa NULL
  size_apparent   INTEGER NOT NULL,               -- dosyanın görünür boyutu
  size_on_disk    INTEGER,                        -- biliniyorsa; yoksa NULL
  mtime           INTEGER NOT NULL,               -- unix saniye (UTC)
  file_key        TEXT,                           -- 'dev:inode' veya 'volserial:fileindex'
  nlink           INTEGER NOT NULL DEFAULT 1,
  is_dup_counted  INTEGER NOT NULL DEFAULT 1,     -- 0: aynı file_key daha önce sayıldı (hard link)
  name_is_raw     INTEGER NOT NULL DEFAULT 0,
  head_hash       BLOB,                           -- ilk 4 KiB hash'i (aşama 2)
  content_hash_id INTEGER REFERENCES content_hash(id)  -- tam hash (aşama 3 hattı)
);

CREATE TABLE scan_error (
  id          INTEGER PRIMARY KEY,
  snapshot_id INTEGER NOT NULL REFERENCES snapshot(id) ON DELETE CASCADE,
  category    TEXT    NOT NULL,   -- 'permission' | 'locked' | 'loop' | 'long_path' | 'io' | 'other'
  rel_path    TEXT    NOT NULL,
  message     TEXT
);

-- İndeksler (ölçerek karar ver; EXPLAIN QUERY PLAN çıktısı docs/ölçüm notlarına eklenecek)
CREATE INDEX idx_dir_parent     ON directory(snapshot_id, parent_id);
CREATE INDEX idx_file_dir       ON file(dir_id);
CREATE INDEX idx_file_snap_size ON file(snapshot_id, size_apparent DESC);
CREATE INDEX idx_file_snap_ext  ON file(snapshot_id, ext);
CREATE INDEX idx_file_size_head ON file(snapshot_id, size_apparent, head_hash)
  WHERE head_hash IS NOT NULL;
CREATE INDEX idx_file_chash     ON file(content_hash_id)
  WHERE content_hash_id IS NOT NULL;
CREATE INDEX idx_err_snap_cat   ON scan_error(snapshot_id, category);
```

**Tasarım gerekçeleri (README/CV'de anlatılabilir):**

- `rel_path` depolama maliyeti getirir ama snapshot'lar arası karşılaştırmayı (diff) basit bir eşleştirmeye indirir. Alternatif (yalnızca `parent_id`) daha az yer tutar fakat diff için yol yeniden kurmayı gerektirir. Karar ölçümle doğrulanacak: veritabanı boyutu ve diff süresi iki seçenekte ölçülüp belgelenir.
- `content_hash` ayrı olduğu için aynı içeriğin tekrar saklanması engellenir ve kopya sorgusu tek tabloya yaslanır.
- `is_dup_counted` (hard link) toplamları şişirmez; toplam sorguları `WHERE is_dup_counted = 1` kullanır.
- Partial index'ler (`WHERE ... IS NOT NULL`) hash'i olmayan çoğunluk satırı indeksten uzak tutar.

## 6. Örnek sorgular

**6.1 Bir klasörün alt ağaç toplamı (recursive CTE)**

```sql
WITH RECURSIVE sub(id) AS (
  SELECT :dir_id
  UNION ALL
  SELECT d.id FROM directory d JOIN sub s ON d.parent_id = s.id
)
SELECT COUNT(*) AS dosya, SUM(f.size_apparent) AS toplam
FROM file f
WHERE f.dir_id IN (SELECT id FROM sub) AND f.is_dup_counted = 1;
```

**6.2 Tüm klasör toplamlarını tarama sonunda doldurma (alt-yukarı)**

Yaklaşım: önce her klasörün *doğrudan* dosya toplamı hesaplanır; sonra derinlikten köke doğru, `depth` büyükten küçüğe, her seviye için çocuk toplamları ebeveyne eklenir. (Her seviye için tek `UPDATE` ... `FROM` sorgusu; en derin seviyeden başlayarak.) Doğruluk testi: 6.1'deki CTE sonucu ile `total_size` karşılaştırılır.

**6.3 Yol kurma (rel_path olmasa da)**

```sql
WITH RECURSIVE up(id, parent_id, path) AS (
  SELECT id, parent_id, name FROM directory WHERE id = :dir_id
  UNION ALL
  SELECT d.id, d.parent_id, d.name || '/' || up.path
  FROM directory d JOIN up ON d.id = up.parent_id
)
SELECT path FROM up WHERE parent_id IS NULL;
```

**6.4 En büyük 20 dosya**

```sql
SELECT d.rel_path, f.name, f.size_apparent
FROM file f JOIN directory d ON d.id = f.dir_id
WHERE f.snapshot_id = :snap AND f.is_dup_counted = 1
ORDER BY f.size_apparent DESC
LIMIT 20;
```

**6.5 Uzantı dağılımı**

```sql
SELECT COALESCE(ext, '(uzantısız)') AS uzanti,
       COUNT(*)                     AS adet,
       SUM(size_apparent)           AS toplam,
       ROUND(100.0 * SUM(size_apparent) /
             (SELECT SUM(size_apparent) FROM file
              WHERE snapshot_id = :snap AND is_dup_counted = 1), 1) AS yuzde
FROM file
WHERE snapshot_id = :snap AND is_dup_counted = 1
GROUP BY ext
ORDER BY toplam DESC
LIMIT 15;
```

**6.6 Kopya adayları: aşama 1 (boyut)**

```sql
SELECT size_apparent, COUNT(*) AS n
FROM file
WHERE snapshot_id = :snap AND is_dup_counted = 1 AND size_apparent > 0
GROUP BY size_apparent
HAVING COUNT(*) > 1;
```

**6.7 Kopya adayları: aşama 2 (boyut + ilk 4 KiB hash)**

```sql
SELECT size_apparent, head_hash, COUNT(*) AS n
FROM file
WHERE snapshot_id = :snap AND head_hash IS NOT NULL AND is_dup_counted = 1
GROUP BY size_apparent, head_hash
HAVING COUNT(*) > 1;
```

**6.8 Kesin kopya grupları ve "boşa giden alan"**

```sql
SELECT ch.id, ch.size, COUNT(*) AS kopya_sayisi,
       (COUNT(*) - 1) * ch.size AS bosa_giden
FROM file f JOIN content_hash ch ON ch.id = f.content_hash_id
WHERE f.snapshot_id = :snap AND f.is_dup_counted = 1
GROUP BY ch.id
HAVING COUNT(*) > 1
ORDER BY bosa_giden DESC
LIMIT 50;
```

**6.9 Snapshot farkı: klasör bazında (A → B)**

```sql
SELECT COALESCE(b.rel_path, a.rel_path) AS klasor,
       COALESCE(b.total_size, 0) - COALESCE(a.total_size, 0) AS degisim
FROM (SELECT * FROM directory WHERE snapshot_id = :a) a
FULL OUTER JOIN (SELECT * FROM directory WHERE snapshot_id = :b) b
  ON a.rel_path = b.rel_path
ORDER BY degisim DESC
LIMIT 20;
```

> `FULL OUTER JOIN` SQLite'ta ancak yeni sürümlerde desteklenir (bu desteğin hangi sürümden itibaren olduğu **doğrulanmadı**). Taşınabilir alternatif: `LEFT JOIN` ile `UNION ALL` + ters yönlü `LEFT JOIN` (yalnızca B'de olmayanlar için).

**6.10 Ölçüm alışkanlığı**

Her ağır sorgu için:

```sql
EXPLAIN QUERY PLAN <sorgu>;
```

çıktısı indeks eklemeden önce/sonra kaydedilir. Hedef: `SCAN` yerine `SEARCH ... USING INDEX` görmek ve süreyi ölçmek. Gerçek ölçümler `docs/` altında tablo olarak tutulur; bu belgede uydurma sayı yoktur.

## 7. Komut satırı arayüzü

Çalıştırılabilir ad: `diskavci`. Ortak seçenekler: `--db <dosya>` (varsayılan: çalışma dizininde `diskavci.db`), `--help`, `--version`, `--quiet`, `--verbose`.

| Komut | Seçenekler | Açıklama |
|---|---|---|
| `scan <yol>` | `--threads N` (varsayılan 1), `--one-fs`, `--follow-symlinks` (varsayılan kapalı), `--exclude <desen>` (tekrarlanabilir), `--hash` (kopya için hash'leme de yap), `--no-save` (yalnızca benchmark; veritabanına yazmaz) | Tarar ve snapshot yazar |
| `snapshots` | — | Snapshot listesi (id, kök, tarih, durum, boyut) |
| `top` | `--snapshot ID` (varsayılan: son tamamlanan), `--files` / `--dirs`, `-n` | En büyükler |
| `ext` | `--snapshot`, `-n` | Uzantı dağılımı |
| `dups` | `--snapshot`, `--min-size 1MB`, `--verify` | Kopya grupları |
| `diff` | `--from ID --to ID` veya `--since 7d`, `-n` | Zaman farkı |
| `report` | `--snapshot`, `--out rapor.html`, `--max-nodes N` | HTML treemap |
| `cleanup-suggest` | `--snapshot` | Yalnızca listeler |

**`--since 7d` anlamı:** Son snapshot (B) ile, B'den en az 7 gün önce alınmış *en yakın* aynı-kök snapshot (A) karşılaştırılır; yoksa açıkça "karşılaştırılacak eski snapshot yok" denir (sessizce başka bir şey yapma).

**Çıkış kodları:** `0` başarılı · `1` kullanım hatası · `2` tamamlandı ama bazı yollar atlandı · `3` ölümcül hata.

**Çıktı ilkesi:** İnsan okunur tablo varsayılan; makine okunur çıktı (`--format json`) yol haritasında isteğe bağlı.

**Güvenlik ilkesi:** Hiçbir komut yazma amaçlı dosya sistemi çağrısı yapmaz; yalnızca veritabanı ve `--out` ile verilen rapor dosyası yazılır. Rapor çıktı yolu, taranan kök içinde ise uyarı verilir (kendi çıktısını sonraki taramada saymasın).

## 8. Algoritmalar

### 8.1 Tarama

Girdi: kök yol, `ScanOptions`. Çıktı: `ISink`'e akan kayıtlar.

1. Kökü doğrula (`status(ec)`): yoksa/dizin değilse kullanım hatası.
2. `snapshot` satırı `running` olarak açılır.
3. DFS (Aşama 1) — `recursive_directory_iterator` yerine **elle yönetilen yığın** önerilir; gerekçe: hata anında tek klasörü atlayıp devam etmek, `one_fs` ve döngü denetimini açıkça yönetmek kolaylaşır. (`recursive_directory_iterator` ile de `skip_permission_denied`, `disable_recursion_pending()` ve `error_code` overload'ları kullanılarak yapılabilir; seçim Hafta 1'de küçük bir deneyle verilir.)
4. Her klasör için: `directory_iterator(path, skip_permission_denied, ec)`; her girdide `symlink_status(ec)` ile **symlink'i takip etmeden** tür belirle.
5. Türe göre:
   - Normal dosya: boyut, mtime, dosya kimliği, `nlink` → `PlatformFs`. Aynı `file_key` daha önce görüldüyse `is_dup_counted = 0`.
   - Klasör: `one_fs` açıksa cihaz kimliği kökünkiyle aynı mı? Değilse atla (kaydet: "başka dosya sistemi").
   - Symlink / Windows reparse (junction vb.): **takip etme**, `file` olarak 0 boyutla "link" türünde say. `--follow-symlinks` açıksa: hedefin `canonical` yolunu / dosya kimliğini ziyaret kümesine bak; ziyaret edilmişse `loop` hatası olarak kaydet ve gir(me).
   - Diğer (soket, aygıt, FIFO): say, boyutuna bakma.
6. Her `ec` dolu ise `ErrorCollector`'a (kategori, yol) ekle ve devam et.
7. Satırlar `ISink`'e verilir; `SnapshotWriter` N satırda bir commit eder.
8. Bitiş: klasör toplamları (6.2), `snapshot` güncelle (`complete`, süre, sayaçlar).
9. Çökme/iptal: snapshot `running` kalır; sonraki açılışta `failed` olarak işaretlenir (temizlik kullanıcıya sorulmadan veritabanından silinmez, `snapshots` listesinde görünür).

Karmaşıklık: O(n) girdi; bellek: yığın derinliği × bir klasörün girdileri.

### 8.2 Kopya bulma hattı

Girdi: snapshot, `--min-size` (varsayılan örn. 1 MiB; 0 baytlık dosyalar hiç dahil edilmez).

1. **Boyut gruplama** (SQL 6.6): benzersiz boyutlu dosyalar elenir. En ucuz ve çoğunu eler.
2. **Hard link eleme:** Aynı `file_key` olanlar zaten tek sayılmıştır (`is_dup_counted = 1` filtresi); bunlar "kopya" değil aynı dosyadır.
3. **İlk 4 KiB hash** (6.7): adaylar için dosyanın ilk 4096 baytı okunur ve hash'lenir; `head_hash` yazılır. 4 KiB'tan küçük dosyalarda bu zaten tam hash'tir (tam hash'e kısayol).
4. **Tam hash:** Hâlâ aynı (boyut, head_hash) grubundaki dosyalar için tüm içerik, büyük tamponla (örn. 1 MiB) akış olarak hash'lenir; `content_hash` tablosuna `INSERT OR IGNORE` + `id` alınır; `file.content_hash_id` güncellenir.
5. **Raporlama** (6.8): `GROUP BY content_hash_id HAVING COUNT(*) > 1`.
6. **İsteğe bağlı doğrulama** (`--verify`): hash eşit gruplarda bayt bayt karşılaştırma.

Bütünlük notları:

- Hash sırasında dosya değişebilir: hash öncesi ve sonrası `size`/`mtime` karşılaştırılır; değiştiyse o dosya "kararsız" sayılıp atlanır ve `scan_error('io')`'ya yazılır.
- Okunamayan dosya (izin/kilit) gruptan çıkar, hata sayılır.
- "Yalnızca çevrimiçi" bulut yer tutucuları hash'lenmez (içerik okumak indirmeyi tetikleyebilir; bu davranış doğrulanmadı, ihtiyatlı davranılır).

### 8.3 Snapshot farkı

Girdi: iki snapshot A (eski), B (yeni) aynı kök için.

1. Kökler farklıysa uyar ve çık (farklı kökler karşılaştırılmaz).
2. Klasör farkı: `rel_path` üzerinden eşleştir (6.9). Her klasör için `degisim = B.total_size - A.total_size`.
3. Raporlama kuralı: *yaprak olmayan klasörlerde değişim üst klasörlere de yansır*; kullanıcıya anlamlı sonuç için "en derin anlamlı klasör" önerilir: bir klasörün değişiminin ≥ %X'i (örn. %80) tek bir çocuktan geliyorsa o çocuğa inilir. (Bu eşik bir tasarım önerisidir, gerçek veride ayarlanacak.)
4. Dosya farkı (isteğe bağlı detay): (`dir.rel_path`, `name`) ile eşleştir → yeni, silinen, boyutu değişen.
5. Zaman: `finished_at` farkı gün olarak başlıkta gösterilir. Karşılaştırmada dosya `mtime` değerleri **yerel saat**e değil UTC saniyeye göre tutulur.
6. Büyük farkta sonuç sınırı (`-n`).

Eşleştirme kısıtları: Windows'ta yol büyük/küçük harf duyarsız olabilir; aynı makinede aynı kök tarandığında yollar dosya sisteminin döndürdüğü biçimle kaydedildiği için bire bir eşleşir. Dosya yeniden adlandırma "sil+ekle" görünür (kabul edilen sınırlama).

### 8.4 Temizlik önerisi (`CleanupAdvisor`)

Kural tablosu (ad → gerekçe işareti):

| Klasör adı | Önerilmesi için gereken yan işaret |
|---|---|
| `node_modules` | Aynı klasörde `package.json` |
| `build`, `cmake-build-*` | Aynı klasörde `CMakeLists.txt` |
| `target` | `pom.xml` veya `Cargo.toml` |
| `.gradle`, `build` (Gradle) | `build.gradle` / `build.gradle.kts` / `settings.gradle` |
| `__pycache__` | Yan işaret gerekmez (Python önbelleği) |
| `.venv`, `venv` | `pyvenv.cfg` içinde |
| `dist`, `out` | Belirsiz: ek işaret yoksa **önerme**, yalnızca "düşük güven" etiketiyle listele |

Çıktı: yol, boyut, güven (yüksek/düşük), gerekçe. Hiçbir komut çalıştırılmaz. Kod tabanında `remove`, `remove_all`, `DeleteFile`, `unlink`, `rmdir` çağrısı bulunmadığı CI'da bir betikle denetlenir (Bölüm 10).

## 9. Thread modeli

### 9.1 İlke

**Aşama 1 tamamen tek iş parçacıklıdır ve ayrıca bitirilir.** Paralellik, çalışan ve test edilmiş bir tek iş parçacıklı sürüme eklenir; `SequentialScanner` referans doğruluk kaynağı olarak kalır (paralel sonuç = sıralı sonuç testi).

### 9.2 Tarama (Aşama 2)

- **Birim iş:** "Bir klasörü listele". İşçi klasörü açar, dosyaları işler, alt klasörleri yeni görev olarak havuza koyar.
- **İşçi sayısı:** `--threads N`; varsayılan **1**. Otomatik seçim yok (HDD riski). Gelecekte otomatik, ölçüme dayanarak düşünülebilir.
- **Bitiş tespiti:** Atomik "bekleyen görev sayısı" sayacı; görev eklerken artar, bitince azalır; 0 olunca bitti.
- **Kimlik üretimi:** Klasör `id`'leri atomik sayaçtan tarama iş parçacığında verilir; böylece çocuk, ebeveyn `id`'sini veritabanına sormadan bilir.
- **Paylaşılan durum:** Ziyaret kümesi (`follow_symlinks` açıkken) ve `file_key` kümesi (hard link) tek bir `mutex` ile korunur veya parçalı (sharded) küme kullanılır. Başlangıçta basit `mutex`; profil ağır kilit çekişmesi gösterirse parçalanır (ölç, sonra karar ver).
- **Hata toplama:** İşçi yerel listede toplar, sınırlı kuyruğa gönderir.

### 9.3 Veritabanı yazma

- **Tek yazıcı iş parçacığı**; tüm `INSERT`'ler orada. İşçiler `BoundedQueue<Batch>`'e koyar. Kuyruk doluysa işçi bekler (bellek sınırı + geri basınç).
- SQLite bağlantısı yalnızca yazıcı tarafından kullanılır; okuma sorguları ayrı bağlantı/iş parçacığında yapılabilir. (SQLite'ın iş parçacığı kipleri derleme/çalışma ayarına bağlıdır; "bir bağlantıyı iş parçacıkları arasında paylaşma" kuralı korunur.)
- WAL kipi ve `synchronous` ayarı ölçülerek belirlenir (Sorun 7); `snapshot` satırı `complete` yapılmadan veri tutarlı sayılmaz.

### 9.4 Hash hattı

- Ayrı havuz; birim iş: bir dosyanın (veya dosya grubunun) hash'lenmesi.
- Her işçinin kendi okuma tamponu vardır (paylaşım yok).
- I/O yoğun olduğundan HDD'de iş parçacığı sayısı tarama sayısından bağımsız ayarlanır: `--hash-threads`.

### 9.5 Doğrulama

- Linux CI'da ThreadSanitizer ile yapı (ayrı iş akışı veya matris girdisi).
- Stres testi: yüzlerce küçük klasörlü sentetik ağaçta 1, 2, 8 iş parçacığı; sonuçlar sıralı tarama ile birebir eşit.
- Benchmark: Bölüm 12 Hafta 3; SSD ve HDD ayrı; sonuç "N iş parçacığı yavaşlattı" çıksa bile README'de yayınlanır (dürüst ölçüm değerlidir).

## 10. Test stratejisi

**Çerçeve:** GoogleTest veya Catch2 (biri seçilir, sürüm sabitlenir). Seçim kriteri: CMake entegrasyonunun kolaylığı; ikisinin de çalışacağı beklenir, ayrıntılar doğrulanmadı.

| Katman | Ne test edilir | Nasıl |
|---|---|---|
| Birim | `PathCodec`, `SizeFormatter`, `ArgParser`, `CleanupAdvisor` kuralları, hash'ler (bilinen test vektörleri; vektörleri resmi depolardan al) | Saf fonksiyon testleri |
| Tarayıcı | Geçici dizinde (`temp_directory_path`) sentetik ağaç kur: iç içe klasörler, boş klasör, uzantısız dosya, Türkçe adlar (`ğüşıöçİ`), boşluklu adlar | Her test kendi geçici dizinini kurar ve temizler (test kodu *kendi* dizinini siler; ürün kodu silmez) |
| Hata toleransı | Okunamayan klasör, kırık symlink, symlink döngüsü | Linux: `chmod 000` ile (**root olarak çalışırken izin kontrolü atlanır**; CI'da root değilsek geçerli, yerelde dikkat). Windows: symlink oluşturma ayrıcalık gerektirebilir; yapılamıyorsa test `SKIP` edilir (gerekçesiyle) |
| Depolama | Şema oluşturma, `:memory:` veritabanı, CTE doğruluğu: `total_size` önbelleği = CTE sonucu | Bellek içi SQLite |
| Kopya | Elle kurulmuş kopya/benzer dosyalar: aynı boyut farklı içerik, aynı ilk 4 KiB farklı sonu, hard link (kopya sayılmamalı) | Beklenen grup listesiyle karşılaştır |
| Fark | İki sentetik snapshot (dosya ekle/büyüt/sil) | Beklenen fark tablosu |
| Eşzamanlılık | Paralel = sıralı eşitlik; TSan | Stres + sanitizer |
| Rapor | HTML'e gömülen JSON geçerli; `</script>` içeren dosya adı güvenle kaçışlanmış | Üretilen dosyayı ayrıştır |
| Uçtan uca | `scan` → `top` → `diff` küçük ağaçta; çıkış kodları | Alt süreç çağıran test |
| Güvenlik | Kaynak ağacında silme API'leri yok | Basit bir `grep` tabanlı CTest girdisi (`remove`, `remove_all`, `unlink`, `rmdir`, `DeleteFile`…); test kodu hariç tutulur |

Ek: Linux'ta AddressSanitizer/UBSan ile derlenmiş bir CI işi. Windows için MSVC sanitizer desteği sürüme bağlı; **doğrulanmadı**, gerekirse yalnızca Linux'ta çalıştır.

Kapsam hedefi sayısal değil niteliksel: her sorun-çözüm kaydının (SORUNLAR belgesi) bir testi olmalı.

## 11. CI (GitHub Actions)

İş akışı (kod yerine adımlar):

1. Tetik: `push` ve `pull_request`.
2. Matris: Linux ve Windows runner'ları. Etiketler (`ubuntu-latest`, `windows-latest`) projeyi yazarken GitHub belgelerinden kontrol edilmeli; bu oturumda belgelerdeki güncel etiket listesi **doğrulanamadı** (sayfa etiket listesi içermiyordu). `latest` etiketleri zamanla değişebilir; tekrarlanabilirlik için belirli bir sürüm etiketi seçmek düşünülmelidir.
3. Adımlar: checkout → CMake yapılandır (Release) → derle → `ctest --output-on-failure`.
4. Ek iş (yalnızca Linux): sanitizer'lı derleme + test.
5. Ek iş: "silme API'si yok" denetimi (testin parçası).
6. Önbellek: bağımlılık indirme süresini kısaltmak için isteğe bağlı.
7. Rozet: README'deki CI rozeti gerçek iş akışı adına bağlanır.
8. Benchmark CI'da **koşturulmaz** (paylaşımlı runner'da ölçüm güvenilmez); yalnızca yerelde.

## 12. 6 haftalık plan

Haftada tahmini 8-10 saat varsayımı (ders yoğunluğuna göre ayarla). **Her hafta sonunda `main` derlenir ve testler geçer.**

| Hafta | İş | Bitiş kriteri (kontrol edilebilir) |
|---|---|---|
| **1** | Repo, lisans, CMake iskeleti, test çerçevesi, CI (Linux+Windows) yeşil. `PathCodec`, `PlatformFs` ilk hali. `SequentialScanner`: özyinelemeli tarama, hata toplama, symlink'i takip etmeme. Komut: `scan` (şimdilik ekrana özet) | CI iki OS'te yeşil; kendi `home`/proje klasöründe tarama hatasız biter ve atlananları sayar; Türkçe adlı test klasörü doğru görünür; en az 10 birim/entegrasyon testi |
| **2** | SQLite sarmalayıcı, şema, `SnapshotWriter` (toplu transaction), klasör toplamları (CTE), `top`, `ext`, `snapshots`; hard link'i çift saymama. **v0.1 etiketi** | Aynı dizin iki kez taranır, iki snapshot da veritabanında; `top` ve `ext` doğru (test ağacında elle hesaplanmış değerlerle eşit); `EXPLAIN QUERY PLAN` kayıtlı; toplu/toplusuz yazma süresi ölçülüp not edildi; README ilk sürümü |
| **3** | `ThreadPool`, `BoundedQueue`, `ParallelScanner`, tek yazıcı. **Benchmark** yöntemi + 1/2/4/8 iş parçacığı ölçümü (varsa SSD ve HDD) | Paralel sonuç = sıralı sonuç (stres testi geçer); TSan temiz; tablo README'de gerçek ölçümle dolu (kötü sonuç da yayınlanır); `--no-save` çalışır |
| **4** | Hash hattı: boyut → 4 KiB → tam hash; `content_hash`; `dups`; `cleanup-suggest`. **v0.2 etiketi** | Test ağacında bilinen kopyalar bulunur, hard link kopya sayılmaz, benzer ama farklı dosyalar elenir; değişen dosya güvenle atlanır; `cleanup-suggest` yalnızca listeler ve silme denetimi CI'da var |
| **5** | `DiffEngine`, `diff` ve `--since 7d`; en derin anlamlı klasör mantığı; zaman dönüşümleri (UTC) | İki sentetik snapshot'ta beklenen fark üretilir; gerçek veride iki gün arayla alınan snapshot farkı mantıklı çıkar (elle doğrula); `--since` eski snapshot yoksa açık mesaj verir |
| **6** | `HtmlReport` + treemap (gömülü JSON, sunucusuz); güvenli kaçışlama; büyük ağaçta düğüm sınırı; README cilası, ekran görüntüleri/GIF, `ncdu`/`dust` karşılaştırma ölçümü, **v0.3 etiketi** | HTML rapor çevrimdışı tarayıcıda açılıp gezilir; `</script>` içeren ad ile test geçer; README'deki yer tutucular gerçek içerikle değişti; "Neden farklı?" bölümü gerçek deneyime göre gözden geçirildi |

**Esneme kuralları (zaman yetmezse kesme sırası):** (1) treemap'te süsleme ve etkileşim, (2) `--verify`, (3) `--hash-threads`, (4) dosya bazlı fark detayı. **Asla kesilmeyecekler:** Aşama 1, testler, CI, README'nin dürüst bölümleri.

**Kayma uyarısı:** Hafta 2 sonunda v0.1 çıkmadıysa Hafta 3'e (paralellik) geçilmez; önce v0.1 bitirilir.

## 13. Doğrulama notları

Bu belge hazırlanırken yapılan kontroller:

**Doğrulandı (kaynakla):**

- `std::filesystem::directory_options` değerleri `none`, `follow_directory_symlink`, `skip_permission_denied`; varsayılanda dizin symlink'leri takip edilmez ve izin hatası hata olarak bildirilir. Kaynak: <https://cppreference.com/cpp/filesystem/directory_options>
- Windows varsayılan `MAX_PATH` = 260; `\\?\` önekiyle yaklaşık 32.767 karakter; Windows 10 sürüm 1607 ve sonrasında uzun yol desteği için hem `LongPathsEnabled` kayıt değeri hem uygulama bildiriminde `longPathAware` gerekir; bu, `W` (Unicode) işlevleri için geçerlidir. Kaynak: <https://learn.microsoft.com/en-ie/Windows/Win32/fileio/maximum-file-path-limitation>
- BLAKE3 deposu: `c` dizininde C gerçekleştirimi, Rust crate'i; lisans CC0 1.0 veya Apache 2.0. Kaynak: <https://github.com/BLAKE3-team/BLAKE3>
- xxHash: kütüphane dosyaları BSD, `xxhsum` GPL; XXH3 64 ve 128 bit üretir; `XXH_INLINE_ALL` ile başlık-içi kullanım. Kaynak: depo yansısındaki eski bir sürümün (v0.8.0) README'si, <https://chromium.googlesource.com/external/github.com/Cyan4973/xxHash/+/refs/tags/v0.8.0/>. Güncel sürüm için ana depoya bak.

**Doğrulanmadı (kullanmadan önce kontrol et):**

- SQLite: `WITH RECURSIVE` söz dizimi, WAL kipi ayrıntıları, `FULL OUTER JOIN` desteğinin başladığı sürüm, `foreign_keys` varsayılanı, lisans metni (resmi belge sayfalarına bu oturumda erişilemedi).
- BLAKE3 C gerçekleştiriminin CMake desteği ve `blake3.h` arayüzü.
- xxHash'in kriptografik olmadığı bilgisinin güncel README'deki ifadesi.
- GoogleTest ve Catch2 güncel sürümleri, lisansları, CMake entegrasyon şekli.
- `ncdu` ve `dust` özellikleri/platformları (bu oturumda sayfalarına erişilemedi); README'deki karşılaştırma ifadeleri kasıtlı olarak genel tutuldu.
- GitHub Actions güncel runner etiketleri ve genel depolar için ücretsiz olma durumu.
- Windows'ta symlink oluşturmanın ayrıcalık gereksinimi, bulut yer tutucu dosyalarda içerik okumanın indirme tetiklemesi, MSVC sanitizer desteği.
- Linux'ta disk türünü (`rotational`) ve Windows'ta arama cezasını (seek penalty) algılama yöntemleri; otomatik iş parçacığı seçimi için kullanılacaksa ayrıca araştırılmalı.
