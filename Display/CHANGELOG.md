# CHANGELOG — 2026-09-12

- Coil 0/1/2 po mb.task() aktualizují indikaci kotlové, topné a boiler pumpy.
- Teploty z holding registrů se čtou jako signed int16.
- Alarmová hystereze >85 / <=82 °C, shodná s PLC (BEHAVIOR CHANGE B2).
- Alarmové přepnutí na Nádrž dokončí editaci a jednou uloží změny (BEHAVIOR CHANGE B6).
- EEPROM se zapisuje při skutečném ukončení editace jen při změně nebo potřebě opravy; chyby begin/commit se logují. Formát a limity zůstávají stejné.
- Rozdíl enkodéru používá uint32_t aritmetiku; stávající millis timery zůstaly správně rollover-safe.

Společný audit, seznam rizik, failure modes a testy: [JonasPLC/review/SAFETY_REVIEW.md](../JonasPLC/review/SAFETY_REVIEW.md). Původní Display main.cpp je uložen v JonasPLC/review/baseline/Display-main.cpp.txt. Firmware nebyl nahrán.
