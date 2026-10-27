# DiskAvcı: Sorunlar ve Çözümler

> **Not (yapay zekâ kullanımı):** Bu belge, yapay zekâ (Claude) yardımıyla hazırlanmış bir taslaktır. Proje ilerledikçe gerçek gelişmelere göre güncellenecektir. "Doğrulanmadı" ve "ÖRNEK" etiketli yerler henüz kontrol edilmemiştir.

Bu belge, proje sırasında karşılaşılması muhtemel sorunları ve çözüm planlarını listeler. Her kayıt: **Belirti**, **Neden**, **Çözüm**, **Test**. Kayıtlar "olabilir" türündendir; gerçekleştikçe "Gerçekte ne oldu" notu eklenmesi önerilir (CV ve mülakat için değerli).

Doğrulama etiketleri: **[D]** bu belge hazırlanırken kaynakla doğrulandı, **[?]** doğrulanmadı / kullanmadan önce kontrol et.

## Özet tablo

| # | Sorun | Önem | Aşama |
|---|---|---|---|
| 1 | Symlink / junction döngüsü | Yüksek | 1 |
| 2 | İzin hatası, kilitli ve sistem dosyaları | Yüksek | 1 |
| 3 | UTF-8 / UTF-16 yol farkları (Türkçe karakter) | Yüksek | 1 |
| 4 | Windows uzun yol sınırı | Orta | 1 |
| 5 | Hard link'lerin çift sayılması | Yüksek | 1 |
| 6 | Çok büyük dizinlerde bellek | Orta | 1-2 |
| 7 | SQLite yazma performansı / transaction batching | Yüksek | 1 |
| 8 | Thread yarış durumları ve SQLite erişimi | Yüksek | 2 |
| 9 | HDD'de paralelliğin yavaşlatması | Orta | 2 |
| 10 | Kapsam şişmesi (GUI'ye kayma) | Yüksek | Tümü |
| 11 | Yanlış silme riski | Kritik | Tümü |
| 12 | Kalabalık kategori ve "neden bu?" sorusu | Orta | Tümü |
| 13 | Görünür boyut ≠ diskteki boyut (sparse, sıkıştırma, küme) | Orta | 1 |
| 14 | Tarama sırasında değişen dosyalar | Orta | 1-2 |
| 15 | Sahte dosya sistemleri ve bağlama noktaları | Orta | 1 |
| 16 | Zaman damgası dönüşümleri ve saat dilimi | Orta | 3 |
| 17 | Kopya bulmada çakışma, boş dosya, bulut yer tutucuları | Orta | 2 |
| 18 | HTML rapor: enjeksiyon ve dosya boyutu | Yüksek | 3 |
| 19 | CI'da platform farkları (symlink, izin, root) | Orta | 1 |
| 20 | Adil olmayan benchmark | Orta | 2-3 |
| 21 | Büyük/küçük harf duyarlılığı ve snapshot eşleştirme | Düşük | 3 |
| 22 | Çıktı konsolunda Türkçe karakter bozulması | Düşük | 1 |

---

## 1. Symlink / junction döngüsü

- **Belirti:** Tarama bitmez, bellek/yığın büyür veya aynı dosyalar tekrar tekrar sayılır; toplam boyut gerçeğinden büyük çıkar.
- **Neden:** Bir link üst klasöre işaret ediyorsa ve link takip ediliyorsa sonsuz döngü oluşur. `std::filesystem` varsayılanında dizin symlink'leri takip **edilmez** [D: cppreference `directory_options::none`], ancak `follow_directory_symlink` açılırsa veya kendi yürüyüşünde `status()` (link'i izleyen) kullanırsan döngü doğar. Windows junction ve diğer reparse noktalarının `std::filesystem` altında nasıl göründüğü uygulamaya bağlıdır [?].
- **Çözüm:**
  1. Varsayılan: link'leri **takip etme**, `symlink_status` ile tür belirle, link'i "link" olarak say (0 bayt).
  2. `--follow-symlinks` isteğe bağlı olsun; açıkken ziyaret kümesi tut: anahtar = `PlatformFs` dosya kimliği (Linux: dev+inode; Windows: birim seri + dosya indeksi) veya en azından `canonical` yol. Kümede varsa girme, `scan_error('loop')` yaz.
  3. Windows'ta reparse noktası bayrağını kontrol et; takip kapalıyken hepsini atla.
  4. Derinlik üst sınırı (örn. çok yüksek bir güvenlik sınırı) emniyet kemeri olarak.
- **Test:** Geçici dizinde `a/b/link -> ../..` döngüsü kur; tarama sonlanır, döngü hatası bir kez raporlanır, toplam boyut link'siz ağaçla aynıdır. Kırık link (hedefi yok) kurulur: çökme yok, "kırık link" sayılır. Windows CI'da link oluşturulamıyorsa test gerekçeli `SKIP` (bkz. #19).

## 2. İzin hatası, kilitli ve sistem dosyaları

- **Belirti:** İstisna fırlar ve program çöker; ya da sessizce eksik sayar ve kullanıcı bunu bilmez.
- **Neden:** Erişim reddedilen klasörler; Windows'ta sistem tarafından kilitli/korumalı yollar (örn. sayfalama dosyası, `System Volume Information` gibi — örnekler genel bilgidir [?]); Linux'ta okuma izni olmayan dizinler.
- **Çözüm:**
  1. Bütün dosya sistemi çağrılarında `std::error_code` overload'ları kullan; istisnaya güvenme.
  2. Dizin açarken `skip_permission_denied` [D] veya hata kodunu yakala, `scan_error('permission')` yaz, devam et.
  3. Kilitli dosyada yalnızca meta veri (boyut) iste; içerik açmayı yalnızca hash aşamasında dene, başarısızsa `locked` say.
  4. Taramanın sonunda "N yol atlandı (izin: a, kilit: b, ...)" özetini her zaman göster; çıkış kodu `2`.
  5. Hata kaydını sınırla (örn. ilk binler detaylı, kalanı sayaç) ki milyonlarca hata belleği doldurmasın.
- **Test:** Linux: `chmod 000` klasör (**root olarak koşuyorsan izinler atlanır**; testi root olmayan kullanıcıyla çalıştır veya root ise `SKIP`). Windows: erişim kuralı değiştirilmiş test klasörü veya `SKIP`. Beklenen: tarama biter, hata sayısı = beklenen, çıkış kodu 2.

## 3. UTF-8 / UTF-16 yol farkları (Türkçe karakter)

- **Belirti:** `ğ ş ı İ ö ü ç` içeren adlar `?` veya yanlış karakter olarak görünür; veritabanında bozuk metin; Windows'ta bazı klasörler "bulunamadı" hatası verir; farklı makinelerde aynı yol farklı saklanır.
- **Neden:**
  - Windows yolları dahili olarak UTF-16'dır; `std::filesystem::path::string()` Windows'ta **yerel kod sayfasına** çevirir (Türkçe sistemde genellikle 1254) ve temsil edemediği karakterlerde kayıp/hata olabilir [?]. `u8string()` ile UTF-8 alınır (C++17'de dönüş türü `std::string`; C++20'de `char8_t` olarak değişir, bu yüzden projeyi C++17'de tut).
  - `main(int argc, char** argv)` Windows'ta komut satırını ANSI kod sayfasında verir; Türkçe/Unicode argüman bozulabilir [?].
  - Linux dosya adları bayt dizisidir, **geçerli UTF-8 olmak zorunda değildir**; Windows adlarında eşleşmemiş vekil (surrogate) çiftleri bulunabilir.
  - "İ/ı" büyük-küçük harf dönüşümü yerel ayara bağlıdır (Türkçe `i` sorunu); kendi `tolower` yazma.
- **Çözüm:**
  1. **Tek iç temsil: UTF-8 `std::string`.** Tüm veritabanı metni UTF-8.
  2. `PathCodec` sınıfı: `path → UTF-8` (`u8string()`), `UTF-8 → path` (`u8path` C++17 / eşdeğeri). Başka yerde dönüşüm yapma.
  3. Windows'ta komut satırı: geniş karakterli giriş noktası (`wmain`) veya `CommandLineToArgvW` ile al, UTF-8'e çevir [?: tam API ayrıntıları Microsoft belgesinden kontrol edilmeli].
  4. Geçersiz dizilerde: adı kayıp-güvenli sakla (hex veya BLOB), `name_is_raw = 1`, ekranda `�` ile göster; asla çökme.
  5. Uzantı çıkarımı: yalnızca ASCII büyük/küçük harf indirgemesi (`A-Z`); Unicode uzantılar olduğu gibi kalır.
  6. Dosya adı karşılaştırması/diff'inde eşleştirme **bayt eşitliği** ile yapılır (yerel ayar kullanılmaz).
  7. MSVC'de kaynak/çalışma karakter kümesi için `/utf-8` derleyici seçeneği düşünülebilir [?]; bu yalnızca kaynak kodundaki literalleri etkiler, çalışma anındaki argv'yi çözmez.
- **Test:** Fixture klasörleri: `Çalışma_ığüşöç`, `İstanbul`, `ısı`, emoji içeren ad, çok uzun Unicode ad. Beklenen: tarama → DB → `top` çıktısında adlar bire bir aynı (bayt karşılaştırması). Linux'ta geçersiz UTF-8 bayt dizili dosya adı oluştur: çökme yok, `name_is_raw=1`. Windows CI'da aynı fixture'lar.

## 4. Windows uzun yol sınırı

- **Belirti:** Derin `node_modules` ağaçlarında "yol bulunamadı" / erişim hataları; tarama ağacın bir kısmını atlar.
- **Neden:** Windows'ta varsayılan `MAX_PATH` 260 karakterdir; `\\?\` önekiyle yaklaşık 32.767'ye çıkabilir; Windows 10 1607+ için hem `LongPathsEnabled` kayıt değeri hem uygulama bildiriminde `longPathAware` gerekir ve bu `W` işlevleri için geçerlidir [D: Microsoft Learn].
- **Çözüm:**
  1. Windows derlemesine bildirim (manifest) ile `longPathAware` ekle (CMake ile kaynak/manifest gömme).
  2. Yine de çalışmayabilir (kayıt değeri kapalıysa) → hata `scan_error('long_path')` olarak sınıflandırılır, çökme yok.
  3. Seçenek: yolları `\\?\` önekli mutlak yola çevir (önek normalizasyonu atlar; eğik çizgi ve `..` kullanılamaz [D]). Bu `PlatformFs` içinde yapılır.
  4. README'de bu gereksinimi ve `LongPathsEnabled` ayarını belirt.
- **Test:** Windows CI'da 300+ karakterlik yol oluşturan fixture. Önek/manifest ile taranır mı? Taranamıyorsa beklenen davranış "long_path hatası + devam". Her iki sonuç da test edilir ve belgelenir.

## 5. Hard link'lerin çift sayılması

- **Belirti:** Toplam boyut diskin gerçek kullanımından büyük; kopya bulma aynı dosyayı "kopya" olarak raporlar.
- **Neden:** Aynı dosya içeriğine giden birden fazla dizin girdisi (hard link) her biri ayrı dosya sanılır.
- **Çözüm:**
  1. `PlatformFs`: dosya kimliği (Linux: `st_dev`+`st_ino`; Windows: birim seri numarası + dosya indeksi) ve bağlantı sayısı. (`std::filesystem::hard_link_count` bağlantı *sayısını* verir, kimliği vermez; kimlik için platform API gerekir [?: ayrıntılar].)
  2. `nlink > 1` olan dosyaları kimlik kümesine koy; ilki sayılır (`is_dup_counted=1`), sonrakiler `0`.
  3. Kopya bulma hattında yalnızca `is_dup_counted = 1` işlenir.
  4. Dokümanda: toplam, "benzersiz dosya kimliğine göre"dir.
- **Test:** Geçici dizinde bir dosya + `create_hard_link` ile 2 link: toplam boyut tek dosya kadar; `dups` bunları kopya saymaz. Gerçek kopya (ayrı içerik, aynı bayt) ise kopya sayılır. (Hard link oluşturma Windows'ta da NTFS'te mümkündür; CI sürücüsünün dosya sistemine bağlı olduğundan başarısızlık `SKIP` olabilir [?].)

## 6. Çok büyük dizinlerde bellek

- **Belirti:** Milyonlarca dosyada bellek GB'larca şişer; tarama sonunda OOM veya kayda değer yavaşlama.
- **Neden:** Bütün ağacı bellekte tutmak, hata listesinin sınırsız büyümesi, yol dizilerinin her girdide tam kopyalanması.
- **Çözüm:**
  1. **Akış:** Girdiler okundukça `ISink`'e verilir ve toplu yazılır; bellekte yalnızca DFS yığını ve yazma tamponu.
  2. Yolları saklama: `parent_id` + `name`; tam yol (`rel_path`) yalnızca klasörler için üretilir (dosyalar için değil).
  3. Hata kaydı sınırlı (#2).
  4. Hard link kimlik kümesi yalnızca `nlink > 1` olanlar için.
  5. Paralel modda `BoundedQueue` ile geri basınç (#8).
  6. Hash hattında (Aşama 2) adayları SQL'den sayfa sayfa çek; hepsini belleğe alma.
  7. Bellek ölçümü: yerel araçlarla zirve bellek raporu (Linux `/usr/bin/time -v` çıktısındaki en yüksek RSS; Windows görev yöneticisi/performans sayaçları) benchmark notlarına yazılır [?: araç ayrıntıları kullanıcı sistemine bağlı].
- **Test:** 100 binlerce küçük dosyalı sentetik ağaç (test sırasında üretilir). Zirve belleğin dosya sayısıyla **doğrusal artmadığı** gözlemlenir (iki boyutta ölçüp kıyasla). Tek klasörde çok sayıda dosya (örn. 100 bin) senaryosu ayrıca.

## 7. SQLite yazma performansı / transaction batching

- **Belirti:** Tarama dosya sistemi hızından çok daha yavaş; her ekleme ayrı diske yazma gibi davranır; veritabanı dosyası çok büyür.
- **Neden:** Her `INSERT` kendi transaction'ında (otomatik commit) çalışırsa her biri için dayanıklılık maliyeti öder; hazırlanmış sorgu kullanılmaması; gereksiz indeksler yazmayı yavaşlatır.
- **Çözüm:**
  1. Tek yazıcı iş parçacığı; **N satırda bir** (örn. on binlerce; N ölçülerek seçilir) `BEGIN ... COMMIT`.
  2. Hazırlanmış sorgular (`Statement`) yeniden kullanılır (`reset` + `bind`).
  3. PRAGMA ayarları ölçülerek seçilir: `journal_mode` (WAL denenebilir) ve `synchronous` seviyeleri; daha gevşek `synchronous` çökme durumunda veri kaybı riski getirir → snapshot `running` durumunda kalır ve `failed` işaretlenir (veritabanı bozulmaz ama kayıt eksik olabilir). Bu takas README'de açıkça yazılır. [?: PRAGMA ayrıntıları SQLite belgesinden kontrol edilmeli.]
  4. İkincil indeksleri **tarama bittikten sonra** oluşturmayı dene (ölç: insert sırasında indeksli vs sonradan indeks).
  5. `INSERT ... RETURNING` yerine klasör `id`'lerini tarayıcıda üret (bkz. TASARIM 9.2) ki ebeveyn `id`'si için geri okuma gerekmesin.
  6. Veritabanı yolunu taranan diskte tutma (kendi dosyasını saymasın, yazma G/Ç'si taramayı bozmasın).
- **Test/ölçüm:** Aynı ağaç: (a) satır başına commit, (b) N=1000, (c) N=10.000, (d) tek dev transaction. Süre ve veritabanı boyutu tablosu (gerçek ölçüm). Çökme testi: tarama ortasında süreci öldür → veritabanı açılır, `snapshot.status='running'`, `PRAGMA integrity_check` ok.

## 8. Thread yarış durumları ve SQLite erişimi

- **Belirti:** Ara sıra yanlış toplam, bazen çökme veya kilitlenme (deadlock); sonuçlar çalıştırmadan çalıştırmaya değişir; "database is locked" hataları.
- **Neden:** Paylaşılan sayaçlar/kümeler kilitsiz güncellenir; birden çok iş parçacığı aynı SQLite bağlantısını kullanır; bitiş tespiti hatalı (iş bitmeden çıkma ya da hiç bitmeme); kuyruğun kapanışında işçilerin uyandırılmaması.
- **Çözüm:**
  1. **Veriyi paylaşma, mesaj gönder:** işçiler tarama sonuçlarını `BoundedQueue<Batch>` ile tek yazıcıya iletir; DB yalnızca yazıcıda.
  2. İşçi başına yerel sayaç, sonda birleştir (paylaşımlı sayaca her adımda yazma).
  3. Ortak yapılar (ziyaret kümesi): önce tek `mutex`; ölç; gerekirse parçala.
  4. Bitiş: atomik "bekleyen görev" sayacı; sıfır olunca kuyruğu kapat ve işçileri uyandır (`condition_variable` doğru koşulla, "spurious wakeup" için döngü).
  5. Hata/iptal durumunda tüm iş parçacıkları `join` edilir (RAII).
  6. Paralel sonuç = sıralı sonuç eşitlik testi her değişiklikte çalışır.
  7. ThreadSanitizer (Linux CI).
- **Test:** Sentetik ağaç (çok sayıda küçük klasör) 1/2/4/8 iş parçacığı × 100 tekrar: dosya/klasör sayısı ve toplam boyut her zaman eşit. TSan temiz. Kapanış testi: taramayı ortasında iptal et, süreç sorunsuz sonlanır (takılma yok; test zaman aşımı koy).

## 9. HDD'de paralelliğin yavaşlatması

- **Belirti:** `--threads 8` SSD'de hızlıyken HDD'de 1 iş parçacığından yavaş.
- **Neden:** Dönen disklerde eşzamanlı rastgele okumalar kafa hareketini artırır (seek); tek iş parçacıklı sıralı gezinti daha uygun olabilir. Bu genel bir bilgidir; sizin diskinizde ölçmeden kesin sayı iddia etme.
- **Çözüm:**
  1. Varsayılan `--threads 1`; paralellik isteğe bağlı.
  2. Benchmark tablosunu SSD ve HDD için ayrı yayınla; kötü sonuç da gösterilir.
  3. README'de açıklama: "HDD'de önerilen: 1". İleride otomatik seçim düşünülürse disk türü algılama yöntemleri ayrıca araştırılmalı [?].
  4. Hash aşamasında ayrı `--hash-threads`.
- **Test:** Elinde HDD yoksa: bunu açıkça yaz ("HDD ölçümü yapılamadı") ve uydurma. Harici disk/USB bellek de ayrı bir ölçüm hâli olarak eklenebilir (özelliği not et).

## 10. Kapsam şişmesi (GUI'ye kayma)

- **Belirti:** Terminal çıktısı yerine pencere/grafik üzerinde saatler harcamak; Aşama 1 haftalarca sürer; "bir de şunu ekleyeyim" listesi uzar.
- **Neden:** Görsel sonuç motive edicidir; disk analizi araçlarında GUI beklenir.
- **Çözüm:**
  1. Kapsam dışı listesi README'de: GUI, silme, sunucu.
  2. Görsel ihtiyacı **tek HTML dosyası** karşılar (Aşama 3, Hafta 6).
  3. Fikir otoparkı dosyası: fikir yaz, uygulama.
  4. Hafta bitiş kriterleri (TASARIM 12) ve kesme sırası: süsleme önce kesilir.
  5. v0.1 (Hafta 2) çıkmadan Aşama 2'ye geçilmez.
- **Test (süreç):** Her hafta sonu kontrol: bitiş kriteri tamam mı? Değilse yeni özellik yok. İki hafta üst üste kaydıysa kapsamı küçült (örn. kopya bulmayı yalnızca boyut+hash ile bırak).

## 11. Yanlış silme riski

- **Belirti:** Kullanıcı veya katkıcı "temizle" komutu ister; öneri listesi bir `build` klasörünü yanlışlıkla gerçek kaynak sanıp listeler ve biri silerse veri kaybı olur.
- **Neden:** Heuristikler hatalıdır (`build`, `dist`, `out` her yerde "yeniden üretilebilir" değildir).
- **Çözüm:**
  1. **Ürün kodunda silme/taşıma/yeniden adlandırma yok** (Tasarım ilkesi). Bu bir kural, bir hedef değil.
  2. Otomatik denetim: CTest girdisi kaynakta yasaklı API'leri arar (`remove`, `remove_all`, `unlink`, `rmdir`, `DeleteFile`, `SHFileOperation` vb.; test kodu istisna).
  3. Öneri güveni: yan işaret dosyası varsa "yüksek", yoksa "düşük" etiket ya da hiç önerme.
  4. Rapor ve README'de uyarı: "Listeyi silme komutuna körü körüne vermeyin."
  5. Çıktı biçimi kasıtlı olarak `rm` satırı **üretmez**.
  6. Veritabanı dosyası yalnızca kendi `.db` dosyasıdır; `snapshot` silme komutu (gerekirse) yalnızca DB satırlarını siler ve bunu açıkça belirtir.
- **Test:** Yasaklı API denetimi yeşil. `cleanup-suggest` sonrası fixture ağacının karması (dosya listesi + boyutlar) önceki ile birebir aynı. `build` klasörü yan işaretsiz fixture: "düşük güven" veya listelenmez.

## 12. Kalabalık kategori ve "neden bu?" sorusu

- **Belirti:** Mülakatta "WinDirStat / WizTree / ncdu / dust varken neden?" sorusu; README'nin sıradan görünmesi.
- **Neden:** Kategori gerçekten kalabalık; bu proje onlarla hız yarışında kazanmak için tasarlanmadı.
- **Çözüm:**
  1. Dürüst cevap: **öğrenme ve portföy projesi**; ayırt edici tasarım: *snapshot tabanlı zaman farkı + SQL ile sorgulanabilir veri + tek dosyalık rapor*.
  2. "Neden farklı?" bölümü iddia değil, **sınır belirten** bölüm (README'de yazıldı): ne olmadığını da söyle.
  3. Hız iddiası yok; yalnızca ölçülmüş ve yöntemi verilmiş sonuçlar.
  4. Mülakat konuşma noktaları (gerçek deneyimden): şema tasarım kararı (`rel_path` takası), paralellik ölçümünün şaşırtıcı sonucu, UTF-8 hatası, hard link buluşu.
  5. Rakip özelliklerini yazmadan önce güncel belgelerinden doğrula; yanlış iddia güveni zedeler. (Bu belgede `ncdu`/`dust` ayrıntıları doğrulanamadı [?].)
- **Test (gözden geçirme):** README'yi tanımadığın birine okut: 30 saniyede "ne yapar, neden var, ne yapmaz" cevaplanabiliyor mu?

## 13. Görünür boyut ≠ diskteki boyut

- **Belirti:** Araç "50 GB" der, disk yönetimi "30 GB" der (veya tersi); sparse dosyalar (VM görüntüleri, veritabanları) ya da sıkıştırılmış klasörler farklı görünür.
- **Neden:** `file_size()` görünür boyuttur; küme (blok) yuvarlaması, seyrek (sparse) dosyalar ve sıkıştırma gerçek disk kullanımını değiştirir.
- **Çözüm:**
  1. İki sütun: `size_apparent` (her yerde) ve `size_on_disk` (platform API ile elde edilebiliyorsa; Linux'ta blok sayısına dayalı, Windows'ta sıkıştırılmış boyut API'si [?: tam API'ler doğrulanmadı]).
  2. Varsayılan sıralama ölçütünü açıkça belirt ve `--by apparent|disk` seçeneği düşün.
  3. README'de fark nedenini açıkla.
- **Test:** Sparse dosya oluştur (Linux: dosyayı büyük boyuta uzat, az veri yaz): `size_apparent` büyük, `size_on_disk` küçük. Windows'ta aynısı yapılamıyorsa `SKIP`.

## 14. Tarama sırasında değişen dosyalar

- **Belirti:** Dosya listelendi ama stat sırasında silinmiş ("bulunamadı"); hash sırasında boyut değişmiş; toplamlar tutarsız.
- **Neden:** Tarama sırasında dosya sistemi canlıdır (TOCTOU).
- **Çözüm:**
  1. `NotFound` hatası "atlandı: kayboldu" olarak kaydedilir, hata değil uyarıdır.
  2. Hash öncesi/sonrası `size` ve `mtime`; farklıysa dosya atlanır (#17).
  3. Snapshot "anlık görüntü" değildir; README'de "tarama sırasında değişim olabilir" notu.
  4. Kendi çıktı dosyasını (DB, rapor) tarama dışında tut.
- **Test:** Tarama sırasında dosya silen yardımcı iş parçacığı olan entegrasyon testi: çökme yok, kayıp dosyalar uyarı olarak sayılır.

## 15. Sahte dosya sistemleri ve bağlama noktaları

- **Belirti:** Linux'ta `/proc`, `/sys`, `/dev` taranınca anlamsız dev boyutlar veya sonsuz/okunamayan girdiler; ağ sürücüleri çok yavaş; bağlama noktaları nedeniyle aynı veri iki kez sayılır.
- **Neden:** Bu yollar gerçek disk içeriği değildir; farklı cihaz kimlikleri taşır.
- **Çözüm:**
  1. `--one-fs`: kök cihaz kimliğinden farklı klasörlere girme (cihaz kimliği `PlatformFs`'ten).
  2. Varsayılan dışlama önerisi: Linux'ta `/proc`, `/sys`, `/dev` kök taranırken uyarı veya otomatik dışla (davranış belgelenir).
  3. Ağ yollarında uyarı.
- **Test:** Linux CI'da `/proc` taraması bilerek denenmez; birim testte cihaz kimliği farkı simüle edilemiyorsa `PlatformFs` için sahte (fake) uygulama arayüzü kullan.

## 16. Zaman damgası dönüşümleri ve saat dilimi

- **Belirti:** "Son 7 gün" yanlış hesaplanır; Windows ve Linux'ta aynı dosya farklı yıl gösterir; gün ışığı saati kayması.
- **Neden:** `std::filesystem::file_time_type` saati C++17'de uygulama tanımlı bir epoch kullanır; `system_clock`'a taşınabilir dönüştürme C++20 ile gelir. C++17'de güvenilir bir dönüşüm için "şimdi" ile `file_time_type::clock::now()` farkı yöntemi ya da platform API kullanılır [?: ayrıntılar].
- **Çözüm:**
  1. Tüm zamanlar **UTC unix saniye** (`INTEGER`).
  2. Dönüşümü tek yerde (`PlatformFs` veya `TimeUtil`) yap ve test et.
  3. `--since 7d` hesabı saniye bazlı; yerel saat sadece gösterimde.
  4. Snapshot `started_at`/`finished_at` doğrudan `system_clock`'tan.
- **Test:** Bilinen `mtime` ayarlanmış dosya (`last_write_time` ile) → DB'de beklenen unix saniyesi. İki platformda aynı test.

## 17. Kopya bulmada çakışma, boş dosya, bulut yer tutucuları

- **Belirti:** Farklı dosyalar "kopya" görünür (nadir); sıfır baytlık dosyalar dev kopya grubu yapar; kopya bulma çok yavaş; bulut klasörleri taranırken dosyalar indirilmeye başlar.
- **Neden:** Hash çakışması teorik olarak mümkündür (özellikle kriptografik olmayan hash'lerde); boş dosyaların hepsi aynı; tam hash çok büyük dosyaları baştan sona okur; "yalnızca çevrimiçi" dosyaların içeriğini okumak indirmeyi tetikleyebilir [?].
- **Çözüm:**
  1. 0 baytlık dosyaları hattan çıkar (ayrı sayaç).
  2. Boyut → 4 KiB → tam hash hattı (okuma miktarını azaltır).
  3. `--min-size` ile küçük dosyaları dışla.
  4. Çakışma: BLAKE3 varsayılanı; xxHash seçilirse `--verify` ile bayt karşılaştırma; sonuç dilinde "aynı hash".
  5. Çevrimdışı/bulut yer tutucusu bayrağı (`PlatformFs`) varsa hash'leme, `skipped:cloud` say.
  6. Büyük tampon (örn. 1 MiB) ve `ifstream` yerine verimli okuma; ölç.
- **Test:** Aynı boyutta, ilk 4 KiB aynı ama sonu farklı iki dosya → kopya **değil**. Aynı içerikli iki dosya → kopya. Boş dosyalar kopya değil. 4 KiB'tan küçük iki aynı dosya → kopya.

## 18. HTML rapor: enjeksiyon ve dosya boyutu

- **Belirti:** `</script>` içeren dosya adı raporu bozar; adda `<img onerror=...>` gibi içerik çalışır (XSS); büyük taramada HTML yüzlerce MB olur, tarayıcı donar.
- **Neden:** Dosya adları kullanıcı denetimindeki güvenilmeyen veridir; JSON'u HTML'e olduğu gibi gömmek ve `innerHTML` kullanmak tehlikelidir.
- **Çözüm:**
  1. JSON gömerken `<`, `>`, `&`, U+2028/2029 karakterlerini kaçışla (en azından `</` → `<\/`).
  2. Rapor JavaScript'inde adları **`textContent`** ile yaz, `innerHTML` ile değil.
  3. Düğüm sınırı (`--max-nodes`, derinlik sınırı); küçükler "diğer (N öğe)" altında toplanır.
  4. Dış kaynak/CDN kullanma (çevrimdışı çalışsın, izlenme/güvenlik riski yok). Kendi treemap algoritmanı yaz veya küçük bir kütüphaneyi lisansını kontrol ederek satır içi göm [?].
  5. İçerik güvenlik ilkesi (CSP meta etiketi) ile dış betik yüklemeyi kapat.
- **Test:** Adı `</script><script>alert(1)</script>` olan dosya: rapor üretilir, tarayıcıda açılınca betik çalışmaz, ad ekranda metin olarak görünür. 100 bin düğümlü sentetik veride dosya boyutu hedef sınırın altında (sınır README'ye yazılır).

## 19. CI'da platform farkları (symlink, izin, root)

- **Belirti:** Test yerelde geçer, CI'da düşer (ya da tersi); Windows'ta symlink testi "izin yok" der; konteynerde root olduğundan izin testi geçmez.
- **Neden:** Windows'ta symlink oluşturma ayrıcalık gerektirebilir [?]; root kullanıcı dosya izinlerini aşar; dosya sistemi türü hard link/sparse desteğini etkiler; runner etiketleri zamanla değişebilir [?].
- **Çözüm:**
  1. Test yardımcılarında "yetenek denemesi": symlink/hard link oluşturulamıyorsa gerekçeyle `SKIP` (sessizce geçme).
  2. `SKIP` sayısı CI özetinde görünür (hep skip olan test sahte güvence verir).
  3. Linux'ta izin testini root değilse çalıştır.
  4. CI'da derleyici/runner sürümünü günlüğe yaz.
  5. Yerelde iki platformda da çalıştırma imkânı yoksa, hatayı CI günlüğünden yakala; ikinci platforma erişimi olan bir arkadaşla doğrulamayı düşün.
- **Test:** CI'a bir "ortam bilgisi" adımı ekle (OS, derleyici, dosya sistemi, root mu); beklenmeyen `SKIP`'ler fark edilir.

## 20. Adil olmayan benchmark

- **Belirti:** DiskAvcı "ncdu'dan hızlı/yavaş" çıkar ama sonuç tekrarlanamaz; ikinci çalıştırma çok hızlıdır.
- **Neden:** Önbellek (sıcak/soğuk), farklı iş yükü (DiskAvcı DB'ye yazar), farklı özellikler (bazı araçlar boyut hesaplamaz, bazıları hard link ayırt eder), arka plan işlemleri, ısınma, tek ölçüm.
- **Çözüm:**
  1. Yöntemi README'de yaz (bkz. README "Benchmark yöntemi").
  2. Soğuk/sıcak ayrı; en az 5 tekrar, **medyan**; aralık da yaz.
  3. `--no-save` modu ile "yalnızca tarama" karşılaştırması.
  4. Makine bilgisi, dosya sayısı, araç sürümleri, komut satırları.
  5. Soğuk önbellek için yöntem sisteme bağlı (Linux'ta yönetici yetkisi gerekebilir; Windows için yeniden başlatma veya başka yöntem) [?: kesin komutlar doğrulanmadı, kendi sisteminde dene].
  6. Zamanlama aracı: komut satırı benchmark araçları var (örn. `hyperfine`), ancak varlığı/sürümü bu oturumda doğrulanmadı [?]; yoksa kendi betiğinle döngü.
  7. İddia dili: "bu makinede, bu dizinde".
- **Test:** Aynı ölçümü iki gün sonra tekrarla; medyanlar makul aralıkta mı?

## 21. Büyük/küçük harf duyarlılığı ve snapshot eşleştirme

- **Belirti:** `diff` aynı klasörü "silindi + eklendi" gösterir; Windows'ta `Foo` ve `foo` birleşmez.
- **Neden:** Windows genelde büyük/küçük harf duyarsız (ama koruyucu), Linux duyarlıdır; yeniden adlandırma/taşıma "sil+ekle" olur.
- **Çözüm:**
  1. Aynı kök aynı makinede taranırken yol, dosya sisteminin döndürdüğü biçimde saklanır → bayt eşitliği çoğu durumda yeterli.
  2. Farklı platform veritabanlarını karşılaştırma desteklenmez (belgelenir).
  3. Yeniden adlandırma tespiti kapsam dışı; kullanıcıya sınırlama olarak yaz.
- **Test:** Aynı klasörü iki kez tara, hiçbir şey değiştirme: `diff` = 0 (kritik "kendi kendine fark" testi).

## 22. Konsolda Türkçe karakter bozulması

- **Belirti:** Kod doğru olsa bile terminalde `ÄŸ`, `?` veya kare kutular görünür.
- **Neden:** Konsol yazı kodlaması UTF-8 değildir veya yazı tipi karakteri içermez; Windows'ta eski konsolda kod sayfası farklıdır.
- **Çözüm:**
  1. Başlangıçta Windows'ta konsol çıkış kod sayfasını UTF-8'e ayarlamayı dene (başarısızsa uyarı ver) [?: API ayrıntıları kontrol edilmeli].
  2. README'de Windows Terminal ve `chcp 65001` önerisi.
  3. `--ascii` çıktı seçeneği (isteğe bağlı): geçersiz/özel karakterleri `?` ile göster.
  4. Tablo hizalamasında UTF-8 görünür genişliği (bayt değil karakter) kullan.
- **Test:** Çıktıyı dosyaya yönlendir ve baytlarını doğrula (konsol görünümünden bağımsız): beklenen UTF-8 dizisi. Görsel kontrol elle yapılır ve README ekran görüntüsüyle belgelenir.

---

## Kayıt tutma önerisi

Her sorun için gerçekleştiğinde bir satır ekle:

```text
#N | tarih | gerçekte ne oldu | ne denedim | ne işe yaradı | bağlantılı commit/test
```

Bu kayıt hem README'deki "öğrenilenler" bölümüne hem mülakat hikâyelerine kaynaktır.
