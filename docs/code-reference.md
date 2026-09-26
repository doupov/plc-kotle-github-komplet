# Technický popis projektu

Tento dokument shrnuje hlavní vazby v kódu a význam jednotlivých vstupů a výstupů.

## 1. Komunikační architektura

Systém používá **jednu společnou sběrnici RS485 / Modbus RTU**:
- **STAM PLC M5** = Modbus master
- **M5Dial** = slave **ID 1**
- **NT48A08** = slave **ID 2**

Komunikace běží na **9600 Bd, 8N1**.

## 2. Mapování teplotních čidel

Ve firmware je rozhodující mapování v `readNT48()`, tedy:

| Kanál NT48 | Proměnná ve firmware | Význam |
|---|---|---|
| CH01 | `t_nt_tank_top` | teplota nádrže nahoře |
| CH02 | `t_nt_tank_mid` | teplota nádrže uprostřed |
| CH03 | `t_nt_tank_down` | teplota nádrže dole |
| CH04 | `t_nt_kotel` | teplota kotle |
| CH05 | `t_nt_boiler` | teplota boileru / TUV |
| CH06 | `t_nt_topeni` | teplota topení |

> Pozor: v deklaracích jsou starší komentáře, které zaměňují CH04 a CH06. Pro dokumentaci je správné držet se skutečného mapování při čtení registrů.

## 3. Modbus registry pro M5Dial

### Holding registry

| Adresa | Název | Význam |
|---|---|---|
| HR0 | `HR_KOTEL_TEMP` | teplota kotle |
| HR1 | `HR_TANK_TOP` | nádrž nahoře |
| HR2 | `HR_TANK_MID` | nádrž střed |
| HR3 | `HR_TANK_DOWN` | nádrž dole |
| HR4 | `HR_TOPENI_ACT` | aktuální teplota topení |
| HR5 | `HR_TOPENI_SET` | nastavení teploty topení |
| HR6 | `HR_BOILER_ACT` | aktuální teplota boileru |
| HR7 | `HR_BOILER_SET` | nastavení teploty boileru |
| HR8 | `HR_KOTEL_TEMP2` | zrcadlená teplota kotle |

### Coil registry

| Adresa | Název | Význam |
|---|---|---|
| Coil 0 | `CO_KOTEL_PUMP` | stav čerpadla kotle |
| Coil 1 | `CO_TOPENI_PUMP` | stav čerpadla topení |
| Coil 2 | `CO_BOILER_PUMP` | stav čerpadla boileru |

## 4. Výstupy PLC

| Výstup | Funkce | Poznámka |
|---|---|---|
| Relay 1 | čerpadlo kotle | NC / invertovaná logika |
| Relay 2 | čerpadlo topení | NC / invertovaná logika |
| Relay 3 | běh pohonu ventilu | `RUN` |
| Relay 4 | směr pohonu ventilu | `OPEN_HOT` při aktivním směru |
| Port A G1 / GPIO1 | čerpadlo boileru / TUV | přímo přes GPIO |

## 5. Chování řízení

### Kotel
Kotel používá:
- absolutní zapínací práh,
- absolutní vypínací práh,
- a hysterézi vůči horní části nádrže.

### Topení
Topení používá hysterézi podle:
- `d_topeni_set` (nastavení z M5Dial),
- a teploty horní části nádrže.

### Boiler / TUV
Boiler se řídí podle:
- `d_boiler_set` (nastavení z M5Dial),
- teploty boileru,
- a dostupnosti tepla ve středu nádrže.

## 6. Ruční režim

Tlačítka A/B/C na PLC přepínají pro jednotlivá čerpadla režim:
- `AUT`
- nebo `FORCE ON`

Ruční volba je pouze **vynucené zapnutí**. Další stisk vrací dané čerpadlo do automatiky.

## 7. Bezpečnostní chování

Systém obsahuje několik bezpečnostních větví:

### Neplatná nebo stará data čidel
Pokud nejsou dostupná platná data, přechází systém do failsafe režimu a čerpadla se zapínají.

### Úplná ztráta čidel / komunikace
Při úplné ztrátě čidel je spuštěn omezený nouzový přejezd směšovacího ventilu do směru **OPEN_HOT**.

### Přehřátí
Při přehřátí se:
- zapnou čerpadla,
- ventil přejde do režimu heat dump,
- a systém se snaží bezpečně odvést teplo.

### Výpadek M5Dial
M5Dial je z pohledu bezpečnosti **neprioritní**. Výpadek displeje nesmí zastavit samotné řízení kotle.

## 8. Doporučení pro další dokumentaci

Pokud budeš chtít dokumentaci později rozšířit, hodí se doplnit:
- seznam použitých knihoven,
- krátký wiring pinout PLC,
- build/postup nahrání firmware,
- a případně separátní elektrické schéma vytvořené ručně podle reálného zapojení svorek.
