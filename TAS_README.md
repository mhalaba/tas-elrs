# TAS-ELRS v2 — utwardzone łącze C2 na bazie ExpressLRS 4.1.0

> Dokument pisany głosem zespołu inżynierskiego działającego przy dronach i EW
> na Ukrainie od 2022 r.: RF/firmware, kryptografia, szkolenia operatorów,
> naprawy w warunkach polowych. Wszystkie decyzje projektowe w tym repo
> wywodzą się z konkretnej lekcji z pola — każdą znajdziesz w sekcji 1 z
> odnośnikiem do kodu, który ją realizuje.

**Zakres projektu: wyłącznie łącze komunikacyjne C2 + telemetria.**
Żadnych ładunków, uzbrojenia, naprowadzania, namierzania celów. Tyle.
Zgodność z regulaminami radiowymi (ETSI EN 300 220/328, FCC Part 15) jest
odziedziczona po stockowym ELRS i obowiązuje; wybór regionu jest trwały.

---

## 1. Lekcje polowe 2022–2026 → wymagania → kod

### L1. Wykrycie zabija szybciej niż zagłuszenie
Najstraszniejszy nie jest jammer. Jest detektor emisji połączony z kierunkową
i artylerią/dronem-lancją. Operator, który siedzi zbyt długo na jednym miejscu
z aktywnym nadajnikiem, sam staje się cel. **Każda sekunda emisji to koszt.**

Realizacja w projekcie:
- profil STEALTH (sekcja 5): telemetria OFF, minimalna moc utrzymująca link,
  krótkie preambuły;
- brak stałych pól plaintext w powietrzu (L2);
- zweryfikowano upstream: TX nie emituje bez aparatury (`tx_main.cpp:
  UARTdisconnected() → hwTimer::stop()`) — zapomniany właczony moduł bez
  radia nie nadaje.

### L2. Popularność = wpisana sygnatura
ELRS jest najpopularniejszym protokołem hobbystycznym świata. Jego sync-word,
preambuła LoRa i struktura ramek są od dawna w tabelach każdego skanera.
Stockowy ELRS na współczesnym teatrze **krzyczy „jestem tu"** zanim cokolwiek
zrobi. Dlatego samo „mamy FHSS" to za mało — przeciwnik zagłusuje selektywnie
protokoły, które rozpoznaje.

Realizacja: keyed MAC zamiast statycznego inicjalizatora CRC z UID;
szyfrowanie całego payloadu RC; brak identyfikowalnych stałych bajtów;
domena `"wake"` oddzielona od domeny danych (`TasWake.cpp`, `TasOta.cpp`).

### L3. Zagłuszanie jest selektywne albo szerokie — planuj na oba
Na froncie spotykasz (publicznie opisane klasy): barrages szerokopasmowe na
całe pasmo, jamery skanujące/sweep, oraz jamery protokołowe celowane w znane
systemy. Pasma sub-GHz bywają czystsze, ale mają mniej kanałów; 2.4 GHz jest
tłoczne (własne drony + Wi-Fi).

Realizacja: klasyfikacja CLEAN/SUSPECT/WIDEBAND/SWEEP raportowana do FC
(`TasAfh.cpp`, ramka CRSF 0x34); deterministyczny per-epoch pruning kanałów
(anti-follow: przeciwnik, który nauczył się Twojej sekwencji, traci ją co
epochę); dual-band failover LR1121 — Faza B (wymaga sterowania pasmem w locie).

### L4. Sprzęt jest zużywalny. Logistyka wygrywa wydajność
Drony nie wracają. RX kupowane hurtowo przez wolontariuszy. Jeśli Twój BOM
wymaga części „z AliExpress 6 tygodni", projekt jest martwy.

Realizacja: cele BOM (RX < 12 USD, TX < 20 USD @ 100 szt.), minimum 2
niezależne źródła każdego układu; build na targetach istniejących płytek
ELRS (zero nowego hardware'u dla v1).

### L5. Zima jest dłuższa niż lato kalendarzowe
−20°C na wietrze, LiPo traci 30–40% pojemności, kondensacja przy wniesieniu
do ciepła. Brownout na 3,0 V/celę musi być bezpieczny, nie losowy.

Realizacja: wymagania temperaturowe −25…+55 °C w kryteriach akceptacji (sekcja
7), brownout-safe restart radia (watchdog), testy komorowe jako warunek Fazy B.

### L6. Operator ma dwa dni szkolenia i nie czyta manuala w okopie
Konfiguracja musi być: preset z QR-kodu/pliku, offline, deterministyczna,
po polsku i ukraińsku. Żadnego „zaawansowanych ustawień" w drodze.

Realizacja: presety operacyjne (sekcja 5) jako specyfikacja dla portalu
konfiguracyjnego; diagnostyka LED/kody (naklejka na obudowę) — Faza B GUI.

### L7. Nad Tobą lata dwadzieścia dronów. Wszystkie z ELRS-em
Planowanie widma i izolacja sesji to nie luksus, to wymóg: cross-talk między
parami = utrata obu.

Realizacja: unikalny klucz sesyjny z materiału bindującego pary (HKDF z UID),
keyed MAC wiążący ramkę z kanałem — ramki cudzego TX-u nie przechodzą walidacji
(test `tas: cross-slot injection rejected`). Pełny plan widma jednostki — Faza B.

### L8. Najgroźniejszy moment misji to utrata linku przy aktywnym FPV
Zamrożone komendy gazu w okopie przeciwnika to dar. RX nie może „pamiętać"
ostatnich wartości dłużej niż 150 ms, a decyzję failsafe podejmuje FC.

Realizacja: zachowany semantycznie stockowy path failsafe + antyreplay
(odrzucone duplikaty nie odświeżają stanu komend) + watchdog radia
(`TasFailsafe.cpp`: cisza >500 ms przy żywym linku → reinit krzemu, licznik
widoczny w telemetrii FC).

### L9. Internetu nie ma. Aktualizacje idą pendrive'em i BLE z telefonu
Realizacja: natywne binarki PIO per-target (stock pipeline) + wymóg podpisanych
artefaktów — Faza B (ed25519 manifesty). v1: proces dystrybucji offline
(sekcja 10).

### L10. Dron może czekać tygodniami, zanim będzie potrzebny
Bateria czekająca dron umiera szybciej od elektroniki. Opcja „sen do miesiąca
z wybudzeniem wyraźnym sygnałem sparowanego TX" to realna potrzeba magazynowa.

Realizacja: deep-sleep RX + duty-cycled CAD + token budzenia kluczowany
kluczem domeny `"wake"` (`TasWake.cpp`). Budżety: 3000 mAh → ~44 dni,
5000 mAh → ~73 dni (poll 3 s). HAL per-płyta — Faza B (świadomie `#error`
bez hooków, żeby nikt nie wystawił „działającego" feature'u na papierze).

---

## 2. Co jest zaimplementowane (mapowanie na kod)

| Funkcja | Stan | Kod |
|---|---|---|
| ChaCha20 (RFC 8439) na payload RC | ✅ v1 | `TasCrypto.cpp`, `TasOta.cpp` |
| Keyed per-frame MAC (kanał+nonce+epoka-materiał) | ✅ v1 | `TasSession.cpp` |
| HKDF-SHA256 z materiału bindującego | ✅ v1 | `TasSession.cpp` |
| Antyreplay (monotoniczne okno nonce) | ✅ v1 | `TasSession.cpp`, `rx_main.cpp` |
| Klasyfikacja zagłuszeń → FC | ✅ v1 | `TasAfh.cpp`, `TasTelemetry.cpp` |
| Deterministyczny pruning FHSS (anti-follow) | ✅ rdzeń | `TasAfh.cpp` |
| Watchdog radia (reinit >500 ms ciszy) | ✅ v1 | `TasFailsafe.cpp` |
| Telemetria TAS_STATUS 0x34 | ✅ v1 | `TasTelemetry.cpp` |
| Deep sleep + wake-token | ✅ logika / ⚠ HAL | `TasWake.cpp` |
| Rotacja kluczy w locie | 🔜 B (licznik epoki w 4 wolnych bajtach SYNC OTA8) | — |
| AFH sterowany pomiarem TX↔RX, dual-band failover | 🔜 B | — |
| Podpisane OTA (ed25519), secure boot | 🔜 B | — |
| Relay PHY-layer | 🔜 C | — |

## 3. Model zagrożeń

| # | Zagrożenie | Skutek bez mitigacji | Mitigacja v1 | Resztkowe ryzyko |
|---|---|---|---|---|
| T1 | Skaner sygnatur → DF → ogień na pozycję operatora | rany/personel | L1 profile, brak stałych pól, keyed wszystko | emisja RF zawsze wykrywalna fizycznie |
| T2 | Jammer protokołowy ELRS | utrata linku | pruning per epoch, watchdog, failsafe FC | total barrage = utrata niezależnie od protokołu |
| T3 | Sweep/wideband | utrata linku | klasyfikacja + raport (operator zmienia pasmo/pozycję) | j.w. |
| T4 | Replay/wstrzyknięcie ramki | przejęcie komend | keyed MAC slot-bound + okno nonce | — |
| T5 | FałszowanieSYNC | desynchronizacja | SYNC bez szyfrowania ale treść niskoczuła; dane i tak walidowane | możliwy brief lock-loss |
| T6 | Przejęcie fizyczne RX | ekstrakcja klucza | rotacja Faza B; procedura rebind po utracie sprzętu | v1: klucz statyczny w urządzeniu |

## 4. Właściwości krypto v1 — uczciwie, bez marketingu

1. Payload RCDATA szyfrowany ChaCha20; typ i CRC w clear (framing); SYNC
   nieszyfrowany (warunek resynchronizacji po total loss).
2. MAC = SHA-256 z klucza epoki powiązanego z kanałem FHSS i nonce, obcięty do
   pola CRC. **Nie jest to Poly1305** — budżet bajtów OTA4/8 nie mieści pełnego
   taga. Fałszerstwo offline praktycznie wykluczone; formalna dowodliwość słabsza
   niż AEAD — świadomy trade-off, poprawiany w Fazie B.
3. Antyreplay: każdy krok wstecz/duplikat odrzucony; utrata linku obsłużona
   ścieżką SYNC, nie akceptacją starych ramek.
4. Klucz statyczny w obrębie wersji firmware (brak pola epoki w powietrzu).
   Powtarzalność pary (kanał,nonce) ≤ co ~768 ramek (najgorsze domeny 3-ch):
   wyciek = XOR dwóch payloadów odległych o wielokrotność okresu. **Przed użyciem
   tam, gdzie przeciwnik archiwizuje emisje masowo, wymagana Faza B.**
5. Token budzenia uwierzytelnia nadajnik (const-time compare), nie świeżość;
   wybudzenie nie omija żadnej autoryzacji lotu.

## 5. Presety operacyjne (specyfikacja dla portalu/Lua, v0)

| Preset | Packet rate | TLM | Moc | Pruning | Zastosowanie |
|---|---|---|---|---|---|
| STANDARD | 250–500 Hz | 1:16 | auto | on | praca domyślna |
| EW_HEAVY | 125–250 Hz | 1:32 | max legalna | on, agresywny | silne zakłócenia |
| STEALTH | 100–150 Hz | **off** | minimalna utrzymująca LQ | on | blisko frontu, L1 |
| MAX_RANGE | 25–50 Hz | 1:64 | max legalna | off | zwiad daleki |
| LONG_WATCH | 50 Hz | 1:128 | niska | off | czat + deep sleep |
| TRAINING/BENCH | dowolny | 1:8 | ≤25 mW | off | praca legalna na ziemi |

## 6. Wybudzenie na sygnał — szczegóły

Cykl: MCU deep sleep → co `TAS_WAKE_POLL_MS` radio robi jeden skan CAD →
aktywność → odbiór tokenu → const-time porównanie → pełny boot linku.
TX-beacon nadaje token ≥1 okres poll na kanale sync.

Budżety (`TasWakeEstimateDays`, prąd skanu 80 mA / 100 ms):

| Bateria | Poll | Średni prąd | Czuwanie |
|---|---|---|---|
| 3000 mAh | 3 s | ~2,9 mA | ~44 dni |
| 5000 mAh | 3 s | ~2,9 mA | ~73 dni |
| 3000 mAh | 1 s | ~8,2 mA | ~15 dni |

Procedura magazynowa: wgraj preset LONG_WATCH → wprowadź sen → wybudzenie
wyłącznie beaconem własnego TX. Utrata RX-a z polem = procedura rebind
(klucz per-para, więc skompromitowany RX nie otwiera innych par — Faza B:
rotacja zamyka temat definitywnie).

HAL (ESP32 ext0/timer + DIO1-CAD; STM32 standby/WKUP) — Faza B per-płyta;
flaga `-DTAS_WAKE_ON_SIGNAL` bez hooków kończy build `#error`.

## 7. Kryteria akceptacji (co musi przejść przed uznaniem za gotowe)

Gotowe (natywne, 60/60 PASS — `src/test/run_mil_tests.sh`):
wektory RFC (ChaCha20/HKDF/HMAC/SHA-256), determinizm i granice pruning
(włącznie z domenami 13-kanałowymi EU868 i ochroną slotów SYNC), roundtrip
TAS na prawdziwym `OTA.cpp` (szyfrogram ≠ plaintext, tamper/cross-slot/replay
odrzucone, SYNC przechodzi, OTA8 działa), regresja stock bez flagi,
budżety snu >30 dni @3000 mAh, werdykty watchdog.

Do zrobienia na sprzęcie (blokery przed deploymentem):
1. HIL z SDR-jammerem (sweep+wideband): krzywe LQ vs moc, czas re-lock
   <2 s (STANDARD) / <5 s (low-signal) po 30 s zasłony.
2. Range-test wg protokołu stockowego + margines −3 dB na krypto/FEC.
3. Soak 24 h: zero utraty synchronizacji, trend free-heap płaski.
4. Komora −25/+55 °C: boot i brownout przy 3,0 V/celę.
5. Pomiary rzeczywistego poboru deep-sleep (nasze liczby to model, nie miernik).
6. Build PIO per-target (macierz płyt stockowych) — nasze testy są natywne
   i nie zastępują kompilacji firmware!

## 8. Proces

- Każda zmiana ścieżek krypto: PR + aktualizacja wektorów w tym README.
- Audyt 2 osoby dla `TasCrypto/TasSession/TasOta`.
- Bez metadanych lokalizacji w commitach; neutralne nazewnictwo.
- Faza B (kolejność): rotacja kluczy przez SYNC OTA8 → AFH pomiarowy →
  dual-band failover → signed OTA → HAL wake per-płyta.
- Faza C: relay PHY-layer (bez deszyfrowania na przekaźniku).

## 9. Granice i zgodność

Projekt nie zawiera funkcji uzbrojenia, naprowadzania ani namierzania —
i takie PR-y będą odrzucane. Urządzenia muszą pracować w ramach regulaminów
radiowych danego kraju; eksport i transfer mogą podlegać kontroli
dwustronnej — weryfikacja prawna przed dystrybucją poza projekt.

## 10. Szybki start budowniczego-wolontariusza

1. Firmware: stock pipeline PlatformIO, w `user_defines.txt` odkomentuj
   `-DTAS_HARDENING` (TX **i** RX zawsze razem! Stock i TAS nie gadają ze sobą).
2. Zbuduj i uruchom suite: `bash src/test/run_mil_tests.sh` — musi być PASS.
3. Bind klasycznym frazą bindującą; sprawdź w portalu, że `TAS_STATUS`
   dochodzi do FC (Betaflight: sensor „TASS").
4. Bench test failsafe: zgaś TX przy uzbrojonym łapie na stole — gaz 0 w
   ≤150 ms, akcja wg konfiguracji FC.
5. Sen/wake: dopiero po dostarczeniu hooków Twojej płyty (Faza B).

---

## Licencja i uznanie autorstwa

- **Cały fork pozostaje na licencji GPL-3.0** (plik `LICENSE`) — to wymóg prawny
  wynikający z dziedziczenia po ExpressLRS. Zmiany w plikach upstream oraz dzieła
  zależne są wyłącznie GPL-3.0.
- **Nasze pliki w pełni autorskie** — `src/lib/TAS/*`, `docs/TAS_README_*`,
  `src/test/test_tas_*`, `src/test/run_tas_tests.sh` — udostępniamy **dodatkowo
  na licencji MIT** (Copyright © 2026 TAS-ELRS Team), do niezależnego wykorzystania.
- **Uznanie autorstwa:** zespół i społeczność [ExpressLRS](https://github.com/ExpressLRS/ExpressLRS)
  (GPL-3.0) za bazę kodu, architekturę FHSS/OTA i lata otwartej pracy.

## Wsparcie projektu ☕

Jeśli projekt Ci pomógł: [**postaw nam kawę → buycoffee.to/maha**](https://buycoffee.to/maha)
