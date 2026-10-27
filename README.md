# DiskAvcı

> **Not (yapay zekâ kullanımı):** Bu belge, yapay zekâ (Claude) yardımıyla hazırlanmış bir taslaktır. Proje ilerledikçe gerçek gelişmelere göre güncellenecektir. "Doğrulanmadı" ve "ÖRNEK" etiketli yerler henüz kontrol edilmemiştir.

> C++17 ile yazılmış, tarama sonuçlarını **SQLite'ta zaman içinde saklayan** komut satırı disk analiz aracı.
> *English: a C++17 command-line disk analyzer that stores every scan as a queryable SQLite snapshot, so you can ask "what grew in the last 7 days?" and find duplicate files, without a GUI or a server.*

![CI](https://github.com/Yasko61TR/DiskAvci/actions/workflows/ci.yml/badge.svg)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Durum](https://img.shields.io/badge/durum-geli%C5%9Ftirme%20a%C5%9Famas%C4%B1nda-orange)

> **Durum notu:** Bu README projenin **hedef tasarımını** anlatır. Aşağıdaki kutucuklar (`[ ]` / `[x]`) gerçek durumu gösterir; işaretlenmeyen bir özellik henüz yoktur. Rozetlerdeki CI bağlantısı repo kurulunca güncellenecek.

---

## İçindekiler

1. [Neden DiskAvcı?](#neden-diskavcı)
2. [Neden farklı? (dürüst karşılaştırma)](#neden-farklı-dürüst-karşılaştırma)
3. [Özellikler ve durum](#özellikler-ve-durum)
4. [Derleme ve kurulum](#derleme-ve-kurulum)
5. [Kullanım](#kullanım)
6. [Mimari özeti](#mimari-özeti)
7. [Benchmark yöntemi](#benchmark-yöntemi)
8. [Güvenlik: dosya silmez](#güvenlik-dosya-silmez)
9. [Bilinen sınırlamalar](#bilinen-sınırlamalar)
10. [Yol haritası](#yol-haritası)
11. [Ekran görüntüleri](#ekran-görüntüleri)
12. [Katkı](#katkı)
13. [Lisans](#lisans)

---

## Neden DiskAvcı?

Disk dolduğunda çoğu araç şu soruya cevap verir: **"Şu an ne kadar yer kaplıyor?"**
Asıl can sıkıcı soru genelde şudur: **"Geçen haftaya göre ne şişti?"**

DiskAvcı her taramayı bir *snapshot* olarak SQLite veritabanına yazar. Böylece:

- aynı dizini farklı günlerde tarayıp iki snapshot'ı karşılaştırabilir,
- sonuçları kendi SQL sorgularınla inceleyebilir,
- tek dosyalık, sunucusuz bir HTML treemap raporu üretebilirsin.

Proje aynı zamanda bir **öğrenme ve portföy projesidir**: gerçek bir veri modeli (ağaç yapısı + recursive CTE), eşzamanlılık (thread pool, tek yazıcı), ölçüm (benchmark, `EXPLAIN QUERY PLAN`) ve çok platformlu CI içerir.

## Neden farklı? (dürüst karşılaştırma)

Disk analizi kategorisi kalabalıktır (ör. WinDirStat, WizTree, ncdu, dust). DiskAvcı bu araçlardan **daha hızlı olduğunu iddia etmez**; en iyi hız için ilgili aracın kendi sayfasındaki bilgilere bak. Farkı şu üç noktadadır:

| Konu | Tipik "anlık görüntü" araçları | DiskAvcı |
|---|---|---|
| Geçmiş | Genelde tek seferlik tarama gösterir (araca göre değişir, kendi belgelerine bak) | Her tarama bir snapshot; iki snapshot arası fark birinci sınıf özellik |
| Veri erişimi | Arayüz içinde gezinme | Veri bir SQLite dosyası: `sqlite3` ile istediğin sorguyu yazabilirsin |
| Kopya bulma | Çoğu araçta ayrı bir iş | Aynı veritabanı üzerinde boyut → ilk 4 KB → tam hash hattı |
| Arayüz | GUI veya TUI | Sade CLI + tek dosyalık HTML rapor (sunucu yok) |

Ne olmadığı da açık olsun:

- Grafik arayüzlü bir uygulama **değildir** ve olmayacaktır (kapsam dışı).
- Windows'ta MFT'yi doğrudan okuyan araçlar kadar hızlı olması **beklenmez**; DiskAvcı `std::filesystem` üzerinden taşınabilir bir yol seçer.
- Dosya silmez, taşımaz, değiştirmez.

> Not: Diğer araçların özellikleri hakkındaki bu tablo genel ifadelerle yazıldı. Yayınlamadan önce her aracın güncel belgesine bakıp tabloyu kontrol et (bu repo yazılırken tüm araçların ayrıntıları tek tek doğrulanmadı).

## Özellikler ve durum

### Aşama 1: Çekirdek (tek iş parçacıklı)
- [x] Özyinelemeli tarama (`std::filesystem`, hata kodlu arayüzler)
- [x] Hata toleransı: izin hatası ve symlink döngüsü atlanır ve raporlanır (kilitli dosya senaryosu henüz test edilmedi)
- [ ] SQLite'a snapshot kaydı (toplu transaction)
- [ ] En büyük N dosya / klasör _(dosyalar tamam, klasörler henüz yok)_
- [ ] Uzantıya göre dağılım
- [ ] Hard link'leri çift saymama
- [ ] Windows ve Linux'ta derleme + test (GitHub Actions) _(şu an yalnızca Linux)_

### Aşama 2: Performans ve kopyalar
- [ ] Çok iş parçacıklı tarama (thread pool, tek veritabanı yazıcısı)
- [ ] Ölçülmüş kazanç tablosu: 1 vs N iş parçacığı (SSD ve HDD ayrı)
- [ ] Kopya dosya bulma: boyut → ilk 4 KB hash → tam hash
- [ ] Temizlik önerisi (yalnızca listeler: `node_modules`, `build`, `.gradle` vb.)

### Aşama 3: Zaman ve rapor
- [ ] İki snapshot arası fark ("son 7 günde +X GB: klasör Y")
- [ ] Tek dosyalık etkileşimli HTML treemap (gömülü JSON, sunucu gerekmez)
- [ ] Benchmark bölümü gerçek ölçümlerle dolduruldu

> Kutucukları yalnızca özellik çalışıp testi geçtiğinde işaretle.

## Derleme ve kurulum

**Gereksinimler**

- C++17 destekleyen derleyici (Linux: GCC veya Clang; Windows: MSVC / Visual Studio Build Tools)
- CMake (sürüm alt sınırı `CMakeLists.txt` içinde belirtilecek)
- Git
- SQLite, CMake ile projeye dahil edilir (ayrıntı: `docs/TASARIM.md`)

**Linux**

```bash
git clone https://github.com/Yasko61TR/DiskAvci.git
cd DiskAvci
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

**Windows (Visual Studio Developer Prompt veya PowerShell)**

```powershell
git clone https://github.com/Yasko61TR/DiskAvci.git
cd DiskAvci
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

**Windows'ta Türkçe karakterler:** Terminalde `ğ ş ı İ ö ü ç` düzgün görünmüyorsa konsolu UTF-8'e al (`chcp 65001`) ve Windows Terminal kullan. DiskAvcı yolları dahili olarak UTF-8 saklar (ayrıntı: `docs/SORUNLAR_VE_COZUMLER.md`).

## Kullanım

> **Önemli:** Aşağıdaki çıktılar yalnızca **biçimi göstermek için yazılmış ÖRNEKLERDİR**. Sayılar uydurmadır, gerçek bir ölçüm değildir. Gerçek çıktılar araç çalıştıktan sonra bu bölüme eklenecek.

Komut yüzeyi (taslak, kesin hali `docs/TASARIM.md` içinde):

```text
diskavci scan <yol> [--db dosya.db] [--threads N] [--one-fs] [--exclude desen]
diskavci snapshots [--db dosya.db]
diskavci top    [--snapshot ID] [--files|--dirs] [-n 20]
diskavci ext    [--snapshot ID] [-n 15]
diskavci dups   [--snapshot ID] [--min-size 1MB]
diskavci diff   --from ID --to ID      (veya --since 7d)
diskavci report [--snapshot ID] --out rapor.html
diskavci cleanup-suggest [--snapshot ID]     (yalnızca listeler)
```

**Tarama (ÖRNEK çıktı biçimi, değerler uydurma)**

```text
$ diskavci scan ~/projeler --db diskavci.db
Tarama: /home/kullanici/projeler
Snapshot #<ID> kaydedildi
  Dosya sayısı : <N>
  Klasör sayısı: <N>
  Toplam boyut : <X GB>
  Atlanan      : <N> (izin: <a>, döngü: <b>, diğer: <c>)
  Süre         : <T> sn
```

**En büyükler (ÖRNEK biçim)**

```text
$ diskavci top --dirs -n 3
 #  BOYUT     YOL
 1  <X GB>    /home/kullanici/projeler/ornek-a
 2  <X GB>    /home/kullanici/projeler/ornek-b
 3  <X MB>    /home/kullanici/projeler/ornek-c
```

**Zaman farkı (ÖRNEK biçim)**

```text
$ diskavci diff --since 7d
Değişim (snapshot #<A> -> #<B>): <+X GB>
 KLASÖR                              DEĞİŞİM
 /home/kullanici/projeler/ornek-a    <+X GB>
 /home/kullanici/.cache              <+Y GB>
Yeni dosya: <N>   Silinen dosya: <N>   Büyüyen dosya: <N>
```

**Kendi SQL'ini yaz**

```bash
sqlite3 diskavci.db "SELECT id, root_path, started_at FROM snapshot ORDER BY id DESC LIMIT 5;"
```

**Çıkış kodları (taslak):** `0` başarı, `1` kullanım hatası, `2` tarama tamamlandı ama bazı yollar atlandı, `3` ölümcül hata (ör. veritabanı açılamadı).

## Mimari özeti

```text
 CLI  ──►  Scanner (Sequential / Parallel)  ──►  yazıcı kuyruğu  ──►  SnapshotWriter ──► SQLite
                │                                                          ▲
                └── PlatformFs (dosya kimliği, disk boyutu, reparse/symlink)│
 Analiz: DuplicateFinder · DiffEngine · CleanupAdvisor  ◄── SQL sorguları ───┘
 Çıktı : TableFormatter (terminal) · HtmlReport (tek dosya treemap)
```

Temel kararlar:

- **Taşınabilirlik:** `std::filesystem`; platforma özgü kod yalnızca `PlatformFs` içinde.
- **Tek yazıcı:** Tarama iş parçacıkları veritabanına doğrudan yazmaz, tek bir yazıcı iş parçacığı toplu transaction ile yazar.
- **Bellek:** Ağaç belleğe yüklenmez; satırlar akıtılır.
- **Güvenlik:** Silme kodu yoktur (CI içinde otomatik kontrol edilir).

Tam ayrıntı: [`docs/TASARIM.md`](docs/TASARIM.md). Karşılaşılan/karşılaşılabilecek sorunlar: [`docs/SORUNLAR_VE_COZUMLER.md`](docs/SORUNLAR_VE_COZUMLER.md).

## Benchmark yöntemi

Sonuç tabloları ölçüm yapıldıktan sonra eklenecek. Şimdilik yalnızca yöntem:

1. Makine bilgisini yaz: CPU, RAM, disk türü (SSD/HDD), işletim sistemi, dosya sayısı.
2. **Soğuk** ve **sıcak** önbellek ayrı ölçülür; her koşul en az 5 kez çalıştırılır, medyan raporlanır.
3. Karşılaştırmayı adil tut: DiskAvcı veritabanına yazar, diğer araçlar yazmayabilir. Bu yüzden iki sayı verilir: `--no-save` (yalnızca tarama) ve normal (kayıtlı) mod.
4. `ncdu` ve `dust` ile karşılaştırma yalnızca kurulabildiği platformlarda ve aynı dizinde yapılır; her aracın sürümü yazılır.
5. İş parçacığı tablosu (örnek iskelet, **boş**):

| İş parçacığı | SSD (medyan sn) | HDD (medyan sn) |
|---|---|---|
| 1 | _ölçülecek_ | _ölçülecek_ |
| 2 | _ölçülecek_ | _ölçülecek_ |
| 4 | _ölçülecek_ | _ölçülecek_ |
| 8 | _ölçülecek_ | _ölçülecek_ |

Not: Paralel taramanın HDD'de **yavaşlatabileceği** bilinen bir risktir; tablo bunu gizlemeden gösterecek.

## Güvenlik: dosya silmez

- Hiçbir komut dosya silmez, taşımaz, yeniden adlandırmaz.
- `cleanup-suggest` yalnızca *yeniden üretilebilir görünen* klasörleri **listeler**. Listelenen bir klasörü silmek tamamen senin kararındır; silmeden önce içeriğine bak.
- Önerilerde yanlış pozitif olabilir (ör. gerçekten kaynak kod barındıran bir `build` klasörü). Bu yüzden öneriler yan işaret dosyalarıyla desteklenir (ör. `package.json` yanındaki `node_modules`).

## Bilinen sınırlamalar

- Hedef: yerel diskler. Ağ sürücüleri ve bulut eşzamanlama klasörleri (ör. "yalnızca çevrimiçi" dosyalar) özel davranabilir.
- Çalışırken değişen dosyalar için sonuç "yaklaşık"tır.
- Yönetici/root yetkisi gerektiren yollar atlanır ve sayısı raporlanır.
- macOS resmi hedef değildir (denenmedi).

## Yol haritası

| Sürüm | Kapsam |
|---|---|
| v0.1 | Aşama 1: tarama, SQLite snapshot, top/ext, CI |
| v0.2 | Aşama 2: thread pool, benchmark tablosu, kopya bulma, cleanup-suggest |
| v0.3 | Aşama 3: diff, HTML treemap |
| Sonra (isteğe bağlı) | Artımlı tarama (değişmeyen dosyaları yeniden hash'lememe), JSON çıktı |

Kapsam dışı: GUI, silme/taşıma, bulut, sunucu.

## Ekran görüntüleri

> Yer tutucular; araç çalışınca gerçek görseller eklenecek.

| Terminal çıktısı | HTML treemap | Diff çıktısı |
|---|---|---|
| `docs/img/terminal.png` _(yer tutucu)_ | `docs/img/treemap.gif` _(yer tutucu)_ | `docs/img/diff.png` _(yer tutucu)_ |

## Katkı

1. Önce bir issue aç, ne yapmak istediğini yaz.
2. Dalı `main`'den oluştur, küçük ve odaklı PR gönder.
3. Yeni davranış için test ekle; `ctest` yerelde ve CI'da geçmeli.
4. Silme/taşıma özelliği ekleyen PR'lar kabul edilmez (bkz. [Güvenlik](#güvenlik-dosya-silmez)).

## Lisans

[MIT](LICENSE). Üçüncü taraf bileşenler kendi lisanslarıyla gelir (ayrıntı `docs/TASARIM.md` içindeki bağımlılık listesinde; yayından önce her birinin lisansını kendi deposundan teyit et).
