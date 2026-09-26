# Cílený patch: heat-dump priorita, Modbus init recovery, servo před Display

Produkční změna pouze v `src/main.cpp`. `control_safety.h` a `Display/src/main.cpp` jsou obsahově nezměněné (ověřeno SHA-256). Nezměněná je také Modbus mapa, tělo fyzického mapování DIR a původní rozhodovací logika tří čerpadel. Diff: `production.patch`, stav před patchem: `main-before.cpp.txt`.

## Pořadí rozhodování

1. Obsluha deadline ventilu; případný init retry s RUN OFF, poté NT48.
2. **Potvrzený alarm + fresh, validní tank-top a topení:** všechna čerpadla ON, případný starý emergency latch přejde přes `recover()` do STOP/settling, ventil reguluje na 60 °C. Vadné kotel/tank-mid/boiler čidlo tuto větev neblokuje.
3. Ostatní chyby senzorů/NT48: dosavadní generic failsafe, případně jednorázový emergency travel. Bez výstupní teploty topení se closed-loop heat-dump nepoužije.
4. Normální regulace.
5. Ve všech příslušných větvích se nejdříve obslouží ventil a teprve pak místní UI a Display. `busy()` brání synchronní Display komunikaci za chodu motoru i během dead-time.

Alarm latch nadále používá `fresh && valid_tankT` a prahy 85/82 °C. Proměnná `sensor_mode` může ukazovat SENSOR_FAIL kvůli vadnému boiler čidlu i při aktivním heat-dump; označuje stav senzorů, nikoli zákaz vyšší alarmové větve.

## Inicializace Modbusu

`setup()` nastaví RUN OFF, inicializuje boiler GPIO a čas počátku NT48 acquisition, pak zkusí `tryInitModbus()`. Selhání přejde do loopu bez nekonečného čekání. `modbus_ready` blokuje NT48 i Display transakce, dokud `begin()` neuspěje. Retry používá unsigned rozdíl času a interval 5 000 ms.

Prvních 12 s bez čtení: všechny pumpy failsafe ON, ventil STOP. Potom stejný emergency automat s jedním rozpočtem 60 000 ms, následně HOLD. Před init retry během pohybu se použije stávající `pauseEmergency()`; motorový rozpočet ani episode latch se nenulují. Úspěšná inicializace vynutí okamžitý pokus o nové NT48 čtení. Fail/partial frame neobnoví zdravý stav ani emergency latch.

Normální recovery vyžaduje fresh kompletní data a validní požadované sondy. Výslovně požadovanou výjimkou je potvrzený overheat s validním tank-top/topným feedbackem: může obnovit řízený heat-dump i při vadných ostatních sondách, vždy přes STOP a settling.

Opakované `begin()` je v instalované knihovně podpořeno úklidem přes `ModbusClient::end()`; při neúspěšném connect se kontext uvolní. Retry není nový retry framework ani nezávislé vlákno: samotné knihovní `begin()` zůstává synchronní. Během volání je motor OFF. Tento patch řeší návrat false, nikoli hypotetické interní zamrznutí knihovny/CPU.

## Ověření

Host testy: **PASS**, exit code 0, bez hlášení sanitizerů. Příkaz: `test/host/run.sh`. Výstup: `tests.log`. Embedded build JonasPLC (`pio run`): **SUCCESS**, 10,07 s, log `build.log`. C++17, ASan/UBSan, `-Wall -Wextra -Werror`. Zachovány předchozí testy rolloveru, complete frames, polarity, běžných pulzů, offline backoffu a hodinové ztráty NT48.

Nový `test/host/priority_test.cpp` spouští skutečný produkční main.cpp s mockovaným hardwarem:

- **A:** fresh tank-top 90 / topení 55 / vadné boiler, tank-mid a kotel → alarm ON, pumpy ON, skutečné OPEN_HOT pulzy a R4 HIGH.
- **B:** tank-top 90 / neplatné topení → alarm ON, žádné closed-loop pulzy; generic STOP.
- **C/D:** begin=false při bootu → loop běží, pumpy ON, retry 5 s; za hodinový fault právě 60 000 ms souhrnného RUN a HOLD bez opakování.
- **E:** begin uspěje → okamžitý NT48 read; fail/partial ponechá failsafe/latch; úplná validní data obnoví regulaci přes STOP/settling.
- **F:** splatná offline Display transakce neprovede čtení před způsobilým startem ventilu, jak v alarmové, tak v normální větvi. Chybějící zpětná vazba ukončí heat-dump.
- **G:** boot/init retry přes UINT32_MAX, zachované testy freshness, pulzu a emergency účetnictví přes rollover.
- Recovery z emergency do heat-dump s vadnými nesouvisejícími sondami; 15 s settling před pulzem, případná reverzace bez DIR změny pod RUN.

Mock potvrzuje při každém `begin()` a každé Modbus transakci RUN OFF, při DIR zápisu rovněž RUN OFF. Bez úspěšného `begin()` nesmí proběhnout žádná transakce.

Firmware se nenahrával; fyzické zkoušky neproběhly. ESBE ARA655, OPEN_HOT=R4 HIGH, 60 s emergency rozpočet, běžné 1 s/15 s/250 ms a čerpadlová policy zůstaly zachovány. Starší zprávy o bezpodmínečném STOP při nesouvisející vadné sondě jsou v případě validního heat-dump feedbacku nahrazeny tímto patchem.
