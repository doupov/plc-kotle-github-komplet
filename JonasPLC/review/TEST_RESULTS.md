> Archiv první revize. Aktuální druhé kolo a změny nouzového režimu: [round2/README.md](round2/README.md).

# Výsledky ověření — 2026-09-12

## Host testy

Příkaz z JonasPLC:

```sh
test/host/run.sh
```

Clang/C++17, `-Wall -Wextra -Werror`, AddressSanitizer a UndefinedBehaviorSanitizer. Výsledek: PASS, exit code 0, bez hlášení sanitizerů. Úplný výstup: `host-tests.log`.

| Test | Výsledek |
|---|---|
| Timer start 0xfffffff0, kontrola na 0xffffffff a 0, timeout na 0x10 | PASS |
| remaining po rolloveru =16, po timeoutu =0 | PASS |
| Perioda 32 ms přes rollover, 4 tick události za 128 ms | PASS |
| Freshness před/na 12 s přes rollover, latch proti oživení po dalším oběhu | PASS |
| Studená→OPEN_HOT, teplá→CLOSE_HOT, hranice ±2 a střed→STOP | PASS |
| Neplatná teplota nebo vypnuté topení→STOP | PASS |
| Dead-time 250 ms, pulz končí při 1 000 ms, settling 15 000 ms | PASS |
| Reverzace zastaví RUN, zachová DIR během settling, DIR se píše jen při OFF | PASS |
| Alarm aktivovaný během pulzu bezpečně přeruší pohyb | PASS |
| Desetiminutový nepřetržitý alarm v produkčním automatu | PASS: 37 pulzů včetně vstupního přerušeného, maximum RUN 1 000 ms |
| Skutečný PLC setup/loop: boot, NT48 fail a partial, stale → tři pumpy ON / RUN OFF | PASS |
| Display fail/partial a SET 0 / 65535 nepoškodí poslední hodnoty | PASS |
| Recovery NT48 i Display, nové validní setpointy a výstup z alarmu | PASS |
| Skutečný loop: 10 min studená voda + 5 min teplá voda v alarmu, offline Display, rollover | PASS: maximum skutečného simulovaného relay RUN 1 000 ms, 37 otevíracích pulzů v prvních 10 min |
| Každá mock Modbus transakce, M5.update a vykreslení požaduje RUN OFF | PASS; jediný pokus za RUN ON by ukončil test assertion |

`integration_test.cpp` přímo zahrnuje produkční `src/main.cpp`; relay mock měří interval mezi skutečnými voláními zapnutí a vypnutí. Transakce posouvají simulovaný čas o 100 ms, smyčka o produkční delay(5). Nejde o emulaci parseru libmodbus ani analogového systému. Test maximálního pulzu má platnost při pravidelném vykonávání programu a funkčních výstupech; nevytváří hardwarovou záruku při zamrznutí CPU/I2C.

## Embedded buildy

```sh
cd JonasPLC
/Users/doupov/.platformio/penv/bin/pio run
cd ../Display
/Users/doupov/.platformio/penv/bin/pio run
```

| Projekt | Prostředí | Výsledek | RAM | Flash |
|---|---|---|---|---|
| JonasPLC | m5stack-stamp-s3, espressif32 6.10.0 | SUCCESS | 23 200 / 327 680 B (7,1 %) | 535 517 / 3 342 336 B (16,0 %) |
| Display | m5stack-stamps3, espressif32 6.10.0 | SUCCESS | 88 932 / 327 680 B (27,1 %) | 824 081 / 3 342 336 B (24,7 %) |

Závěrečný PLC log je `jonasplc-build.log`; úspěšný Display build byl ověřen ve výstupu nástroje (71,10 s). Při prvním pokusu omezení sandboxu blokovalo globální PlatformIO lock/cache, následná autorizovaná kompilace proběhla standardně. Žádné změny závislostí ani konfigurace nebyly potřeba.

Instalované hlavní závislosti: PLC M5StamPLC 1.1.0+7c33fdf, M5Unified 0.2.9+e126f90, ArduinoModbus 1.0.6+f903d0a, ArduinoRS485 1.0.2+2dd70d2. Display M5Dial 1.0.3, LVGL 9.3.0, modbus-esp8266 4.1.0. Arduino ESP32 framework 2.0.17.

## Meze a zbývající zkoušky

- Nebyl proveden upload, fyzický test serva, relé, RS-485 ani topného systému.
- Display EEPROM a LED změny byly zkontrolovány zdrojově a embedded buildem; host harness neprovádí LVGL/EEPROM emulaci.
- Skutečný timeout/truncation/CRC přes RS-485 a restart panelu ověřit na zařízení. Host simuluje návraty API.
- Nezměněná plausibilní odpověď NT48 nemůže dokázat čerstvou fyzickou konverzi.
- Normální pump hysterese byla porovnána zdrojovým diffem; testy neprocházejí všechny kombinace hydraulických teplot a historie pump.
