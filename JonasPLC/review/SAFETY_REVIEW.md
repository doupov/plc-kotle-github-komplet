> Archiv první revize. Aktuální druhé kolo a změny nouzového režimu: [round2/README.md](round2/README.md).

# Reliability a safety review — JonasPLC + Display

Datum: 2026-09-12. Baseline: původní lokální soubory před úpravou, uložené v `review/baseline/`. Obě složky byly bez `.git`; nevznikly commity ani nový repozitář. Regulační logika čerpadel a mapování NT48 kanálů jsou zachovány. Firmware nebyl nahrán do zařízení.

## Hlavní závěr

Původní alarm bezpodmínečně přepisoval ventil na RUN=true, DIR=false po celou dobu přehřátí. Nebyl limit pohybu, ustálení ani vazba na výstupní teplotu. I běžná regulace mohla běžet souvisle a změnit DIR při RUN=true. Nově má každý povel pohybu limit 1 s, následuje STOP a nejméně 15 s ustálení. Alarm nejprve zastaví motor a potom reguluje topnou vodu k explicitnímu cíli 60 °C, s původním pásmem ±2 °C.

**Důležitá mez závěru:** odstraněn je nepřetržitý programový povel alarmu. Opakované omezené pulzy mohou za trvající teplotní odchylky postupně dosáhnout dorazu. Bez polohové zpětné vazby to nelze vyloučit. Software rovněž nezaručí rozepnutí vadného relé, správný fyzický směr, funkční koncové spínače ani STOP při zamrznutí CPU/I2C. Tyto vlastnosti host test neprokazuje.

## Nálezy P0/P1/P2

| Priorita | Nález | Výsledek |
|---|---|---|
| P0 | Alarm drží RUN nepřetržitě, nezávisle na teplotě topné vody | Odstraněno; STOP na hraně alarmu, regulované pulzy |
| P1 | Normální regulace nemá časový limit; DIR může změnit za chodu | Společný automat, STOP před DIR, dead-time, limit každého pulzu |
| P1 | Blokující Modbus může zpozdit softwarový STOP | Modbus, vykreslení a M5.update se během PULSE/DEADTIME nevolají; opakované I2C zápisy pump jsou během pohybu odloženy |
| P1 | Po ztrátě NT48 se neomezeně používají poslední teploty | 12 s od poslední úplné odpovědi → původní pump failsafe a STOP ventilu |
| P1 | Setpointy PLC nejsou omezené; při prvním výpadku jsou NAN | Přijetí jen 35–70 / 50–65 °C, poslední validní hodnota; počáteční 60 / 55 °C |
| P1 | Aplikace nekontroluje kompletnost osmi registrů | Atomické převzetí až po celé sadě; fail/partial nezmění teploty ani setpointy |
| P1 | Fyzický směr nejasný, komentář alarmu odporuje komentáři běžné regulace | Jedno mapování OPEN_HOT→HIGH, CLOSE_HOT→LOW zachovává původní zapojení; **fyzicky neověřeno** |
| P1 | Zamrzlá, ale plausibilní hodnota uvnitř NT48 při úspěšných odpovědích | **Nedetekovatelné dostupným protokolem**; freshness měří komunikaci, nikoli skutečnou obnovu konverze |
| P1 | Display může zobrazovat staré teploty/LED jako aktuální při výpadku komunikace | Coil synchronizace opravena; **stáří dat na panelu není protokolem předáváno**, PLC má vlastní stale ochranu |
| P2 | Tři coil zápisy každý průchod smyčkou | Změna stavu + 10 s heartbeat; nejvýše jeden coil zápis v loopu, retry nejvýše jednou za sekundu pro každý coil |
| P2 | Display coil registry nepřevádí do proměnných LED | Opraveno ihned po mb.task() |
| P2 | Přechod alarmu na Nádrž ponechává aktivní neviditelnou editaci | Editace se dokončí a změna jednou uloží před přepnutím |
| P2 | Stisk tlačítka mimo editaci vyvolával EEPROM commit | Jen ukončení skutečné editace, navíc kontrola změny a výsledku commit |
| P2 | Alarm bez hystereze | ENTER >85 °C, EXIT <=82 °C na PLC i Display |
| P2 | Signed rozdíl enkodéru může přetéct | Modulo uint32_t rozdíl, směr při vzdálenosti <2^31 kroků |
| P2 | Mrtvá vnitřní OFF→ON větev kotlového čerpadla | Popsána níže; regulační kód ponechán |

Plausibilita teplot (-100, 300) °C a ignorování tank-down při souhrnné validaci zůstávají jako baseline. Rozsah je široký; plausibilní vadné měření není zachyceno. Komentáře deklarací CH04/CH06 v baseline nesouhlasily s reálným přiřazením: zachováno CH04=kotel, CH06=topení. Nutné ověřit zapojení, nikoli podle komentářů prohodit sondy.

## Všechny BEHAVIOR CHANGE

| ID | Původní chování | Nové chování | Důvod a přínos | Regresní riziko |
|---|---|---|---|---|
| B1 | Alarm trvale RUN=true, DIR=false | STOP při vstupu, pak regulace výstupní teploty na 60 °C pulzy | Odstranění známého failure mode a neomezeného pohybu | Omezená rychlost a výkon odvodu tepla; vhodnost 60 °C a fyzický směr nutno ověřit |
| B2 | Alarm ihned zaniká při <=85 °C | Drží do <=82 °C, při neplatných datech se latch uchová, ale přednost má failsafe | Potlačení přepínání poblíž prahu | Delší běh všech pump, více přeneseného tepla v pásmu 82–85 °C |
| B3 | Poslední validní NT48 hodnoty platí navždy | Po 12 s bez kompletní odpovědi všechna čerpadla ON, ventil STOP; návrat po úplné odpovědi s validními potřebnými sondami | Zabrání řízení podle starých dat | Výpadek delší než 12 s může rozběhnout pumpy, přenést nežádoucí teplo a zvýšit spotřebu; zachována původní filozofie failsafe |
| B4 | PLC bez prvního úspěšného Display read porovnává NAN | Do prvního validního čtení používá 60 / 55 °C, shodné s defaulty panelu | Určité a omezené rozhodování po bootu bez panelu | Dříve zadané jiné hodnoty PLC přes svůj restart neuchovává; bez panelu může začít regulovat na default |
| B5 | Libovolný kompletně přijatý setpoint se použije | Neplatná hodnota se odmítne jednotlivě, zachová se poslední validní | Panel není jediná bezpečnostní hranice | Úmyslný požadavek mimo rozsah nebude proveden; druhý validní setpoint stejné úplné sady se může přijmout |
| B6 | Při alarmovém přepnutí na Nádrž zůstává editace zapnutá | Editace se ukončí, aktuální omezená hodnota se uloží | Enkodér již nemění skrytý požadavek | Rozeditovaná hodnota se potvrdí i bez druhého stisku; v PLC byla již předtím dostupná průběžně |

Omezení normálního pohybu ventilu je podle zadání označeno **HARDENING / NO INTENDED CONTROL CHANGE**, ale mění dynamiku: stejný směr a pásmo ±2 °C, nižší pracovní cyklus a nejméně 15 s čekání i po předčasném zastavení/reverzaci. Toto může zpomalit reakci i mimo alarm. Zachováno: přidávat teplou pod set−2, ubírat nad set+2, STOP včetně hraničních hodnot. Ostatní hardening: atomické čtení, signed zobrazení záporných teplot na panelu, coil synchronizace, omezení provozu, korekce editace/flash zápisů a explicitní boot STOP.

## Ventil: automat a fyzické mapování

Implementace je v `include/control_safety.h`, použitá přímo produkčním programem i host testem. Jediné elektrické mapování je `ValveOutputs::direction()` v `src/main.cpp`. RUN = relé index 2 (R3, NO), DIR = relé index 3 (R4). OPEN_HOT → true/HIGH, CLOSE_HOT → false/LOW. STOP vypíná RUN a drží poslední DIR; regulační algoritmus nepracuje s elektrickou hodnotou DIR.

| Stav | RUN | Přechod |
|---|---|---|
| IDLE | OFF | Požadavek OPEN_HOT/CLOSE_HOT: RUN OFF → DIR → DEADTIME |
| DEADTIME | OFF | Po 250 ms při stále stejném požadavku RUN ON → PULSE; při změně požadavku STOP → SETTLING |
| PULSE | ON | Po 1 000 ms nebo změně/odvolání požadavku RUN OFF → SETTLING |
| SETTLING | OFF | Nejméně 15 000 ms od STOP, potom nové vyhodnocení požadavku přes IDLE |

Hrana alarmu vždy volá STOP. Při stale/neplatných měřeních má STOP přednost před alarmem. Po návratu dat může pohyb začít, pokud uplynula doba ustálení. Přepnutí směru tedy obsahuje i ustálení, potom OFF → DIR → dead-time → ON. Vše bez delay čekání uvnitř automatu.

Konstanty jsou výchozí pro uvedení do provozu, nikoli odvozené z identifikovaného serva: 1 s by znamenala asi 1/120 zdvihu serva s přejezdem 120 s; 15 s umožňuje reakci teplotní sondy/soustavy; 250 ms je výchozí rezerva pro změnu relé. Skutečná doba přejezdu, sepnutí/rozepnutí relé a hydraulická odezva nejsou z projektu známé. Pulz končí v prvním obslouženém průchodu po limitu; v host simulaci přesně 1 000 ms, na zařízení navíc plánovací/IO latence. Absolutní hardwarový časový limit při poruše CPU nebo I2C vyžaduje nezávislé časové odpojení pohonu.

Před blokujícími operacemi se RUN OFF znovu zapisuje. Nezavádí se trvalá cache úspěchu I2C: API `writePlcRelay()` vrací void, neumí potvrdit úspěšný zápis. Zachovány opakované zápisy čerpadel mimo aktivní pohyb. Čerpadla během pulzu nemají nová měření ani setpointy; stale větev nejdříve zastaví ventil a poté zapíše pumpy.

## Failure modes — požadované výstupy po úpravě

ON/OFF níže znamená funkční čerpadlo při předpokládaném zapojení, nikoli elektrický stav relé. Kotel a Topení jsou NC: ON→relé false; Boiler je GPIO1 NO: ON→HIGH. **Bez ověření kontaktů a skutečné cirkulace nelze fyzické výstupy garantovat.**

K = původní kotlová logika popsaná níže. T = původní hystereze tank-top vůči setpointu topení ±2. B = původní boiler logika: zapnout při boiler<set−2 a tank-mid>boiler, vypnout při boiler>=set+2 nebo tank-mid<=boiler. V ostatních případech drží předchozí stav. Tyto symboly označují jednoznačný algoritmus závislý na teplotách a paměti, nikoli neurčený bezpečnostní režim.

| Událost | Kotel pump | Topení pump | Boiler pump | Valve RUN | Valve DIR / požadavek |
|---|---|---|---|---|---|
| NT48 nedostupný od bootu, po prvním loop | ON | ON | ON | OFF | STOP, drží boot DIR |
| NT48 výpadek/partial po validním čtení, stáří <12 s | K, při alarmu ON | T, při alarmu ON | B, při alarmu ON | Omezené pulzy nebo OFF | Normální teplotní požadavek, při alarmu cíl 60 |
| NT48 bez úplné odpovědi >=12 s | ON | ON | ON | OFF | STOP, poslední DIR |
| Úplná odpověď s neplatnou potřebnou sondou | ON | ON | ON | OFF | STOP, poslední DIR |
| **NT48 vrací stále stejnou plausibilní hodnotu v úplných odpovědích** | K/alarm | T/alarm | B/alarm | Pulzy/OFF podle zdánlivé teploty | **Skutečná potřebná akce není bezpečně určitelná; nedetekováno** |
| Display fail/partial za běhu | K/alarm | T/alarm s posledním SET | B/alarm s posledním SET | Pulzy/OFF | Poslední validní SET; žádná nula z chybějícího registru |
| Display kompletní odpověď s neplatným SET | K/alarm | T/alarm s posledním validním SET | B/alarm s posledním validním SET | Pulzy/OFF | Neplatný SET ignorován |
| Restart JonasPLC — inicializace | Baseline NC ON po inicializaci relé | Baseline NC ON po inicializaci relé | OFF po GPIO init | Explicitně OFF ihned po PLC.begin | Před prvním pohybem DIR nastaví automat |
| Restart JonasPLC — validní NT48, Display offline | K/alarm | T s defaultem 60 / alarm ON | B s defaultem 55 / alarm ON | Pulzy/OFF | Běžně cíl 60, v alarmu 60; **starý SET přes restart PLC není uchován** |
| Restart Display | K/alarm | T/alarm | B/alarm | Pulzy/OFF | Během výpadku poslední SET, po návratu EEPROM/default v platném rozsahu; může nastat legitimní změna SET |
| Vstup overheat (>85) | ON | ON | ON | Ihned povel OFF, potom čekání | STOP → regulace na 60 |
| Trvající overheat | ON | ON | ON | Max 1 s pulzy s pauzami | OPEN_HOT pod 58, CLOSE_HOT nad 62, jinak STOP |
| Návrat z overheat (<=82, validní data) | K | T | B | Pulz může pokračovat jen do limitu, pokud zůstává stejný požadavek; jinak STOP | Znovu lokální validní SET |
| Návrat NT48 komunikace | K/alarm | T/alarm | B/alarm | Nové pulzy až po ustálení | Nová validní výstupní teplota; alarm latch se znovu vyhodnotí |
| Návrat Display komunikace | K/alarm | T/alarm | B/alarm | Pulzy/OFF | Přijme platný SET; coily se opraví retry/heartbeatem |
| Modbus klient se nepodaří inicializovat | Baseline dle inicializace relé | Baseline dle inicializace relé | OFF | OFF | STOP; původní nekonečné čekání zachováno; **řízení neběží** |
| **Porucha I2C, svařené relé, zatuhlý pohon/CPU** | **Fyzicky neurčitelné** | **Fyzicky neurčitelné** | **Fyzicky neurčitelné** | **Povel není důkaz rozepnutí** | **Nezávislá ochrana není tímto refaktorem nahrazena** |

Původní failsafe se třemi pumpami ON může ohřívat TUV nebo odvádět teplo nevhodnou cestou. Bez schématu hydrauliky nelze označit za univerzálně bezpečný. Podle zadání je zachován. Alarm se stejně jako dříve vyhodnocuje jen při validitě všech potřebných sond; při jejich chybě vítězí failsafe.

## Kotlové čerpadlo — skutečný baseline

1. Kotel >=75 °C: ON bez ohledu na delta vůči nádrži.
2. Kotel <=55 °C: OFF.
3. 55<kotel<75: pokud je OFF, zůstává OFF. Vnitřní podmínka `kotel>=kotel_start` s `kotel_start=75` je zde nedosažitelná.
4. V témže pásmu již zapnuté čerpadlo vypne při `kotel<=tank_top−2`, jinak zůstane ON. Dílčí test `kotel<=kotel_stop` s `kotel_stop=50` je také nedosažitelný.
5. Failsafe a alarm mají vyšší prioritu a zapínají pumpu.

Minimální možné zjednodušení by při pevně shodných prazích odstranilo vnitřní OFF→ON větev a z ON→OFF odstranilo test <=50. Není provedeno: hodnoty jsou nastavitelné proměnné a po budoucí změně prahů mohou být větve opět relevantní. Opraven je pouze mylný komentář rozsahu 55–80 na 55–75.

## Modbus a recovery

Adresy/baud/piny/knihovny nezměněny. `readRegisters()` kontroluje návrat 8, available 8 a úspěšné odebrání všech osmi slov do dočasného bufferu. Teprve pak se aktualizuje cíl. Fail ani partial neobnoví freshness. Úplný rámec obnoví čas transportu; chybové hodnoty jednotlivých sond jsou jako dříve převedeny na NAN a aktivují failsafe.

Kontrola instalované ArduinoModbus 1.0.6+f903d0a: `ModbusClient::requestFrom()` sám vrací požadovaný počet, ale nižší `libmodbus/modbus.c:check_confirmation()` kontroluje délku a počet hodnot, RTU vrstva integritu. Proto nález nulového doplnění nebyl prokázán jako průchozí wire exploit této verze; aplikace byla zbytečně závislá na této záruce. Krátký fyzický rámec knihovna typicky vrátí jako FAIL (timeout/length/CRC), nikoli jako dostupných 7 registrů. Diagnostika rozlišuje OK, FAIL/frame error a PARTIAL na úrovni API; **počet skutečně přijatých bajtů při timeoutu API neposkytuje**, nelze tedy spolehlivě rozlišit ticho od všech druhů wire truncation bez změny knihovny. Oba případy jsou bezpečně odmítnuty.

NT48 polling zůstává 3 s, Display 1 s; během pohybu se mohou zpozdit přibližně o dead-time+pulz. Sedm teplotních zápisů skončí při prvním neúspěchu, zbytek se zkusí při další periodě. Signalizace pump je omezená samostatně, úspěšný zápis se pamatuje, neúspěšný se opakuje. Heartbeat opraví i restart panelu bez změny stavu pump. Při nedostupném panelu a dlouhém timeoutu může první coil dočasně předbíhat další; po obnovení potvrzených zápisů se obslouží i ostatní. Jde jen o signalizaci.

Limit stale se vyhodnocuje po návratu blokujících volání a za chodu pulzu v každém loopu. Nejde o real-time deadline všech pump: porouchaná komunikace může reakci pump zpozdit o dobu právě probíhajícího volání/batche. Motor během těchto volání má povel OFF.

## Timery, dlouhodobý čas a watchdog

Prověřeny `src` a vlastní `lib` obou projektů. Původní periodické poll timery PLC, LVGL tick a navigační dead-time Display již používají unsigned rozdíl a byly ponechány. Nenalezen countdown typu `remaining -= millis()` ani signed millis timer. Nové pulse/settle/dead-time/freshness/coil retry/heartbeat/UI timery používají uint32_t odečítání. `remaining()` je čistý výpočet z elapsed, testovaný přes rollover. Expirace freshness se zapamatuje, takže se prastarý údaj neobnoví při dalším celém oběhu millis. Předpoklad všech těchto krátkých timerů je průběžné vykonávání programu; ne pauza CPU na celý 49,7denní oběh.

Nebylo nalezeno počítadlo servisních hodin, dlouhodobý interval ani uptime statistika, proto se nepřidává 64bit čas bez použití. Pokud přibudou, použít monotónní 64bit zdroj typu esp_timer_get_time().

Watchdog nepřidán. Lokální ModbusRTUClient má response timeout 1 000 ms, libmodbus byte timeout 500 ms a protocol recovery může přidat čekání/flush. U přicházejících bajtů s velkými rozestupy nelze celou transakci považovat za tvrdě omezenou na 1 s; batch a UI přidávají další čas. Nahodilý krátký task watchdog by mohl restartovat provozně fungující systém. Navíc MCU reset nezaručuje rozepnutí externího expanderu před jeho inicializací. Espressif popisuje nutnost správně dimenzovat timeout a sledovat konkrétní tasky v [Task Watchdog dokumentaci](https://docs.espressif.com/projects/esp-idf/en/v4.4.5/esp32/api-reference/system/wdts.html). Prioritou je nyní motor OFF před blokující komunikací. Změny defaultních watchdogů platformy se nedělaly.

## Display a EEPROM

Po mb.task() se všechny tři coil hodnoty kopírují do LED proměnných. Teplotní registry se interpretují jako signed 16bit, takže záporné hodnoty nejsou 655xx °C. UI alarm má stejnou hysterezi 85/82 jako PLC; je ovšem jen indikace z přijaté zaokrouhlené teploty, může se od PLC u prahu o zaokrouhlení lišit.

EEPROM formát (magic 0xBEEF, dvě uint16 hodnoty) zachován. Platné hodnoty se načtou jednotlivě; neplatná data nahradí stávající default. Chybné magic způsobí stejnou inicializaci jako dříve. Validní magic s vadnými daty se opraví při příštím ukončení editace. begin/commit fail je logován; po selhání commit se hodnoty neoznačí za uložené a další ukončení editace zápis zkusí znovu. Žádné periodické ani krokové flash zápisy. Platná editace bez změny necommitne. Přerušení alarmem je nyní ukončení editace.

Formát nemá aplikační CRC ani dvouslotový transakční záznam. Korupci vedoucí na jinou hodnotu uvnitř povoleného rozsahu nerozpozná. Na panelu není timeout poslední master komunikace; po restartu proto krátce ukazuje defaultní teploty/LED, při dlouhém výpadku poslední přijaté údaje. Tento nedostatek signalizace neřídí PLC, ale obsluha jej nesmí pokládat za důkaz aktuálního měření.

## Ověření a uvedení do provozu

Automatické výsledky a příkazy jsou v `TEST_RESULTS.md`. Testy používají produkční safety header a skutečný JonasPLC main.cpp s mockovaným hardwarem; nejsou simulací hydrauliky ani RTU elektrické vrstvy. Oba embedded buildy prošly s instalovanými knihovnami, bez změn platformio.ini.

Před nasazením ověřit na zařízení: fyzický OPEN_HOT/CLOSE_HOT, RUN OFF při bootu a přepnutí směru, skutečný čas pulzu a dead-time, dobu přejezdu a koncové spínače, reakci na 60 °C alarmový cíl a skutečnou cirkulaci všech pump. Zkoušku odpojení NT48 a Display provést v kontrolovaných podmínkách a sledovat také recovery a restart panelu během editace. Potřebné HW/hydraulické parametry v projektu nejsou; **fyzická bezpečnost systému není uzavřena host testy**.
