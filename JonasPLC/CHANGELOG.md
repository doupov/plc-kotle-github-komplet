# Cílený patch — priorita heat-dump / Modbus init / servo

- **SAFETY FIX:** Confirmed overheat with valid tank-top and heating-supply feedback now takes priority over unrelated sensor failures.
- **RELIABILITY:** Modbus initialization failure no longer halts JonasPLC permanently. Controller stays in failsafe mode and retries initialization periodically (5 s).
- **SAFETY PRIORITY:** Valve actuator servicing and safety decisions now have priority over Display communication.
- **NO BEHAVIOR CHANGE:** Normal valve polarity remains `OPEN_HOT = R4 HIGH`, `CLOSE_HOT = R4 LOW`.
- **NO BEHAVIOR CHANGE:** Existing pump failsafe and ESBE ARA655 emergency travel policy remain unchanged. Rozpočet zůstává 60 s motorového ON času; retry inicializace jej při STOP pauze nevynuluje.
- Produkční změna pouze v JonasPLC/src/main.cpp; Display a control_safety.h beze změny. Výsledky a přesný rozsah: [review/priority/README.md](review/priority/README.md).

# Upřesnění serva — ESBE ARA655

- Uživatel potvrdil dobu přejezdu 60 s / 90°. `VALVE_EMERGENCY_TRAVEL_MS` upraveno z předpokládaných 120 000 na 60 000 ms.
- Nouzový rozpočet je stále jednorázový součet ON času, bez pauz čtení NT48. Po vyčerpání následuje HOLD.
- Směr OPEN_HOT = R4 HIGH, normální pulz 1 s, ustálení 15 s a dead-time 250 ms beze změny. Jeden normální pulz odpovídá nominálně 1,5°.

# CHANGELOG — druhé kolo, 2026-09-12

- **SAFETY FIX:** Overheat alarm validity now depends on tank-top validity rather than validity of every temperature sensor.
- **SAFETY FIX:** Complete NT48 loss moves mixing valve once to a bounded configured emergency heat-dump position and then stops the actuator. `VALVE_EMERGENCY_DIRECTION = OPEN_HOT`; rozpočet motorového času 60 s pro uživatelem uvedený ESBE ARA655 (60 s / 90°). Pauzy pro NT48 zachovávají jeden rozpočet; po dojezdu HOLD až do plné obnovy dat.
- **RELIABILITY:** Display communication uses offline backoff and cannot starve safety-critical NT48 polling. Tři chyby → OFFLINE, pouze čtení po 10 s; úplná odpověď → běžná 1 s. Nejvýše jedna Display transakce po řízení na loop; všechny zápisy respektují backoff.
- **NO BEHAVIOR CHANGE:** Normal servo direction mapping remains `OPEN_HOT = R4 HIGH`, `CLOSE_HOT = R4 LOW`. Běžná regulace 1 s / 15 s, alarm 85/82 °C, cíl 60 °C a pump logika zachovány. Display se nemění.
- Rozšířené host testy: A–H, hodinový fault, obnova za pohybu, boot bez NT48, skutečný výstup DIR a rollover. Podrobnosti: [review/round2/README.md](review/round2/README.md).
- **REVIEW LATER:** Evaluate whether boiler pump should remain ON during complete sensor loss, because boiler temperature is unknown.
- **COMMISSIONING:** Verify that 60 °C radiator supply provides sufficient emergency heat-dump capacity for the actual boiler/system.

---

# CHANGELOG — 2026-09-12

## JonasPLC

- HARDENING / NO INTENDED CONTROL CHANGE: společný pulzní automat ventilu, STOP před DIR, dead-time, ustálení, limit pulzu; stejné normální teplotní pásmo a směr.
- BEHAVIOR CHANGE B1/B2: alarmový cíl topné vody 60 °C místo trvalého motorového povelu; ENTER >85, EXIT <=82 °C; STOP na hraně alarmu.
- BEHAVIOR CHANGE B3: 12 s timeout NT48 → dosavadní pump failsafe a STOP; automatický návrat po validních datech.
- BEHAVIOR CHANGE B4/B5: počáteční bezpečně omezené SET 60/55 °C; přejímání pouze 35–70 a 50–65 °C, zachování posledních platných hodnot.
- HARDENING: celé osmi-registrové transakce před převzetím, žádná nulová náhrada chybějícího registru.
- HARDENING: blokující komunikace/UI jen mimo pohyb ventilu; opakovaný STOP před komunikací, pump coil změny/retry/heartbeat, konec temperature batche po prvním failu.
- HARDENING: explicitní boot RUN OFF, stale diagnostika i při výpadku obou Modbus zařízení.
- Kotlové prahy a hystereze čerpadel zachovány. Watchdog/knihovny/piny se nemění.

## Display

- HARDENING: skutečný obsah tří Modbus coilů promítnut do indikace čerpadel.
- HARDENING: signed čtení teplot, overflow-safe rozdíl enkodéru, omezené EEPROM zápisy a logování selhání.
- BEHAVIOR CHANGE B2/B6: alarmová hystereze 85/82 °C a dokončení editace při přepnutí na Nádrž.

## Testy a audit

- Host testy produkčního automatu a skutečného PLC setup/loop pod AddressSanitizer/UndefinedBehaviorSanitizer.
- Rollover, remaining, periodicita, pulse maximum, settling, reverzace, alarm, fail/partial/stale/recovery a setpointy.
- Původní soubory zachovány v review/baseline; podrobnosti změn, rizik a failure modes v review/SAFETY_REVIEW.md.
- Složky nemají Git: místo neexistujících commitů je k dispozici baseline, changelog a auditní diff. Logické balíčky odpovídají testům, valve safety, Modbus/stale, Display a signalizaci.
