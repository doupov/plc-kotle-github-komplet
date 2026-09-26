> Následující cílený patch mění prioritu heat-dump při nesouvisejících vadách senzorů a přidává Modbus init recovery: [../priority/README.md](../priority/README.md).

# Druhé kolo safety/reliability — 2026-09-12

Tento dokument doplňuje a v níže uvedených bodech nahrazuje audit první revize. Upravené produkční soubory jsou pouze `JonasPLC/src/main.cpp` a `JonasPLC/include/control_safety.h`. Display se nemění. Zachováno zapojení, Modbus mapa/ID/piny, EEPROM, setpoint rozsahy, běžné čerpadlové algoritmy, normální pulzy 1 s / ustálení 15 s, dead-time 250 ms a alarm 85/82 °C s cílem 60 °C.

## Změny

- **SAFETY FIX:** Overheat alarm validity now depends on tank-top validity rather than validity of every temperature sensor. Vstup je `fresh && valid_tankT`. Vadný boiler nebo jiná sonda neblokuje zpracování validní tank-top teploty. Obecný failsafe zůstává nezávislý.
- **SAFETY FIX:** Complete NT48 loss moves mixing valve once to a bounded configured emergency heat-dump position and then stops the actuator. Směr je konfigurační `VALVE_EMERGENCY_DIRECTION = Direction::OPEN_HOT`, tedy **R4 HIGH**. Rozpočet motorového času je 60 000 ms pro ESBE ARA655, jehož dobu přejezdu 60 s / 90° uvedl uživatel. Nahrazuje původní předpoklad 120 s.
- **RELIABILITY:** Display communication uses offline backoff and cannot starve safety-critical NT48 polling. Po třech chybách od posledního kompletního čtení se vypnou zápisy teplot i coilů a zbude pouze čtecí probe po 10 s. Úspěšná kompletní osmi-registrová odpověď obnoví interval 1 s a vynuluje chyby. Neplatné setpointy se nadále jednotlivě odmítají.
- **NO BEHAVIOR CHANGE:** Normal servo direction mapping remains `OPEN_HOT = R4 HIGH`, `CLOSE_HOT = R4 LOW`. Produkční tělo `ValveOutputs::direction()` ani `demand()` se nezměnilo.

První dva safety fixy záměrně mění rozhodnutí oproti první revizi: alarm nyní zpracovává tank-top i při chybě jiné sondy a úplný výpadek již neznamená pouze STOP v náhodné poloze. Jednotlivá vadná sonda nadále znamená pumpy ON a ventil STOP. Riziko nouzového otevírání bez znalosti teplot je přijato podle zadání; skutečný odvod tepla a koncovou polohu nelze potvrdit bez hydraulického/servo testu.

## Fault režimy a episode latch

| Stav dat | Režim | Pumpy Kotel/Topení/Boiler | Ventil |
|---|---|---|---|
| Fresh a všechny požadované sondy validní | NORMAL | Původní algoritmy / alarm ON | Původní demand + pulzy |
| Fresh, vadná jednotlivá požadovaná sonda | SENSOR_FAIL | ON / ON / ON | STOP; tank-top alarm se nezávisle vyhodnocuje |
| Fresh, neplatné všechny požadované sondy | SENSOR_FAIL | ON / ON / ON | Jeden emergency travel |
| Bez kompletní odpovědi >=12 s od poslední přijaté sady | NT48_LOST | ON / ON / ON | Jeden emergency travel |
| Start bez jediného kompletního čtení | NT48_LOST | ON / ON / ON | Prvních 12 s od spuštění klienta STOP, pak emergency |
| Pokračující úplná ztráta po dojezdu | NT48_LOST / SENSOR_FAIL | ON / ON / ON | EMERGENCY_HOLD, RUN OFF |
| Pouze částečný návrat sond během nouzového pohybu | SENSOR_FAIL | ON / ON / ON | Přerušení do EMERGENCY_HOLD, episode latch se neruší |
| Fresh návrat všech požadovaných sond | NORMAL | Původní algoritmy / alarm ON | STOP, 15 s ustálení, pak normální regulace; rearm další fault episode |

Požadované sondy jsou původní kotel, tank-top, tank-mid, topení, boiler; tank-down se stejně jako dříve neúčastní souhrnné validace. Opakovaný úplný výpadek po pouze částečné obnově nespustí další přejezd. Kompletní obnova jej znovu povolí. Čerstvé odpovědi se zamrzlými, ale plausibilními hodnotami zůstávají bez další diagnostiky nerozpoznatelné.

## Emergency state machine a souběh s NT48

Normální stavy `IDLE`, `DEADTIME`, `PULSE`, `SETTLING` zůstaly zachovány. Nové stavy:

1. `startEmergencyTravel()`: RUN OFF → DIR OPEN_HOT → `EMERGENCY_DEADTIME`.
2. Po 250 ms → RUN ON → `EMERGENCY_TRAVEL`.
3. Před splatným synchronním čtením NT48: RUN OFF → `EMERGENCY_PAUSED`. Uloží se délka dokončeného ON segmentu jako unsigned elapsed time.
4. Pokud úplný fault přetrvává, následuje nový dead-time a pokračování **téhož přejezdu se zbývajícím rozpočtem**. Neprovádí se normální 1 s / 15 s regulace.
5. Po celkem 60 000 ms povelu ON: RUN OFF → `EMERGENCY_HOLD`. Ani opakované volání startu během stejné epizody pohyb neobnoví.
6. Při úplné obnově dat se nejprve provede STOP a normální settling. DIR se nikdy nezmění při RUN ON.

Přerušování kvůli NT48 je nutné s existující synchronní knihovnou: neponechává motor zapnutý během timeoutů a umožňuje zastavit nouzový pohyb při obnově senzorů. **60 s je celkový motorový ON čas, nikoli wall-clock doba celé epizody.** Ta zahrnuje pauzy čtení a dead-time, proto je delší. Čítač `emergency_run_ms` sčítá jen dokončené intervaly `elapsed(now, started)`; nejde o odpočítávání absolutního millis. Limit se nezvětšuje ani při opakovaném přerušení.

V každém loopu má pořadí prioritu: servis motorového limitu → NT48 → rozhodnutí a výstupy → Display. Display má nejvýše **jednu** transakci na loop; po transakci se normální pulz zahájí až po novém vyhodnocení v dalším loopu. Teplotní batch se posílá po jednom registru. Za aktivního emergency pohybu/jeho pauzy se Display neobsluhuje; v HOLD opět může. NT48 se nadále čte, perioda 3 s se počítá od dokončení předchozího čtení.

V offline Display testu s reálným modelovaným 1s timeoutem zůstává mezera čtení NT48 pod 6 s, tedy pod stale limitem 12 s. Žádné Display retry/coil/temperature batche nedokážou obsluhu NT48 donekonečna odsunovat. To není tvrzení o libovolně dlouhém interním zablokování knihovny: jednotlivá synchronní transakce musí skončit; výjimečně pomalý/chybný byte stream či porucha CPU/I2C vyžadují další řešení. Motor má během transakcí povel OFF.

## Testy

Spuštění: `test/host/run.sh` z JonasPLC. Výstup: `review/round2/host-tests.log`. Produkční header a skutečný main.cpp s hardware mocky, C++17, `-Wall -Wextra -Werror`, ASan a UBSan.

| Scénář | Výsledek |
|---|---|
| A: demand(40,60)→OPEN_HOT a produkční výstup R4 HIGH; demand(70,60)→CLOSE_HOT a R4 LOW | PASS |
| B: tank-top 90 °C, boiler -273,1 °C→NAN, fresh → alarm true a SENSOR_FAIL | PASS; při tank-top 82 alarm správně zanikne i s vadnou boiler sondou |
| C/D: hodinová ztráta NT48 | PASS; jedna fault episode, přesně 60 000 ms součtu povelů RUN, potom HOLD bez opakování |
| E: částečná/úplná obnova, obnova během pohybu, nový fault po úplné obnově | PASS; STOP/settling, bez DIR změny za RUN |
| F: Display offline od bootu 10 min, 1s timeouty, NT48 fresh, regulace běží | PASS; defaults 60/55, normální RUN max 1 000 ms |
| F: opětovný výpadek po přijetí SET 45/60 | PASS; poslední setpointy zachovány |
| G: partial frame zůstává OFFLINE, complete obnoví 1s polling a přijme nové validní SET | PASS |
| H: rollover freshness, emergency travel/dead-time, běžný pulz/settling a offline retry | PASS; emergency a offline start také na 0xffffff00 |
| Start s nedostupným NT48 a fresh rámec s úplně vadnými sondami | PASS; 12s boot prodleva, obnova, emergency při úplně vadné sadě |
| Původní timer, rámce, setpointy a 15min alarm při validních datech | PASS; běžný pulz zůstává nejvýše 1 000 ms |

Ověření DIR/RUN probíhá při každém volání mock relé; jakýkoli DIR zápis pod RUN nebo Modbus transakce pod RUN ukončí test assertion. Hodinový integrační test rozlišuje jednotlivé krátké pauzy pro polling od nové fault episode a měří součet skutečných relay povelů, ne pouze interní čítač automatu.

Embedded build druhého kola před upřesněním serva: **SUCCESS**, 10,37 s, RAM 23 224 B (7,1 %), flash 536 465 B (16,1 %). Log: `review/round2/build.log`. Display nebyl změněn ani znovu kompilován. Zdrojové porovnání s uloženou první revizí potvrdilo doslovně stejné tělo mapování serva, `demand()`, algoritmy všech tří čerpadel a mapu Modbus registrů. Produkční diff: `review/round2/production.patch`; původní soubory tohoto kola: `review/round2/baseline/`. Složka nemá Git, commity nevznikly.

## Commissioning / TODO

- ESBE ARA655: uživatelem uvedený přejezd 60 s / 90°. Při uvedení do provozu ověřit koncové spínače a odvod tepla v plně otevřené horké větvi. Směr R4 HIGH je podle uživatele správný a beze změny.
- **REVIEW LATER:** Evaluate whether boiler pump should remain ON during complete sensor loss, because boiler temperature is unknown. V tomto patchi zůstává ON.
- Verify that 60 °C radiator supply provides sufficient emergency heat-dump capacity for the actual boiler/system. Cíl v tomto patchi zůstává 60 °C.
- Softwarové termíny platí při obsluhovaném loopu, s plánovací/IO latencí; stav relé a dosažení fyzické polohy nejsou měřeny. Firmware se nenahrával a fyzické zkoušky neproběhly.

Upřesnění serva na 60 s: aktuální host výsledky jsou v `servo-60s-tests.log`, embedded build v `servo-60s-build.log`. `production.patch` zachycuje původní druhé kolo před tímto upřesněním.
