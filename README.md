# PLC kotle — STAM PLC + NT48A08 + M5Dial

Kompletní firmware a dokumentace domácího systému řízení kotle.

Tento repozitář popisuje domácí systém řízení kotle postavený na:
- **STAM PLC M5** jako hlavním řídicím prvku,
- **NT48A08** jako Modbus teplotním modulu,
- **M5Dial** jako lokálním displeji a panelu nastavení,
- a výkonových výstupech pro **čerpadla** a **pohon směšovacího ventilu**.

> Dokumentace je sestavená podle dodaného firmware a přiložených fotografií instalace.
> Není to svorkové ani revizní schéma 230 V. U výkonové části záměrně popisuje jen to, co jde z kódu a fotek rozumně doložit.

## Obsah dokumentace

- [Uživatelské ovládání](#uživatelské-ovládání)
- [Nouzové chlazení a bezpečnost](#nouzové-chlazení-a-bezpečnost)
- [Technický popis](#technický-popis)
- [Schéma systému (Mermaid)](docs/diagram.md)
- [Galerie fotografií](docs/gallery.md)
- [Snapshot firmware](docs/source/firmware_snapshot.md)

## Firmware

- [`JonasPLC/`](JonasPLC/) — firmware hlavní řídicí jednotky STAM PLC M5
- [`Display/`](Display/) — firmware lokálního panelu M5Dial

Oba projekty jsou připravené pro PlatformIO. Otevřete příslušnou složku jako samostatný PlatformIO projekt.

## Co systém dělá

- čte teploty přes **NT48A08**,
- řídí **čerpadlo kotle**, **čerpadlo topení** a **čerpadlo boileru / TUV**,
- ovládá **tříbodový pohon směšovacího ventilu**,
- posílá aktuální data a stav čerpadel do **M5Dial**,
- čte z **M5Dial** nastavené teploty pro topení a boiler,
- a má bezpečnostní chování pro výpadek komunikace, neplatná čidla a přehřátí.

## Uživatelské ovládání

Běžné uživatelské nastavení se provádí na otočném displeji **M5Dial**. Displej má čtyři obrazovky: **Kotel**, **Nádrž**, **Topení** a **Boiler**.

### Pohyb mezi obrazovkami

- Otáčením voliče se přechází mezi jednotlivými obrazovkami.
- Obrazovky zobrazují aktuální teploty a stav příslušného čerpadla.
- Zelená kontrolka znamená, že čerpadlo běží; červená znamená, že je vypnuté.

### Nastavení teploty topení

1. Otočením voliče přejděte na obrazovku **Topení**.
2. Stisknutím voliče aktivujte úpravu žádané teploty. Nastavovaná hodnota se zobrazí červeně.
3. Otáčením nastavte požadovanou teplotu v rozsahu **35–70 °C**, po 1 °C.
4. Dalším stisknutím nastavení potvrďte a uložte.

### Nastavení teploty boileru / TUV

1. Otočením voliče přejděte na obrazovku **Boiler**.
2. Stisknutím voliče aktivujte úpravu žádané teploty. Nastavovaná hodnota se zobrazí červeně.
3. Otáčením nastavte požadovanou teplotu v rozsahu **50–65 °C**, po 1 °C.
4. Dalším stisknutím nastavení potvrďte a uložte.

Nastavené hodnoty se ukládají do EEPROM a zůstanou zachované i po restartu M5Dial. Změny se přes Modbus průběžně předávají hlavní jednotce PLC.

### Ruční zapnutí čerpadel na PLC

Tři tlačítka na jednotce STAM PLC M5 přepínají režim jednotlivých čerpadel:

| Tlačítko | Čerpadlo |
|---|---|
| A | kotel |
| B | topení |
| C | boiler / TUV |

Každý stisk přepne příslušné čerpadlo mezi:

- **`AUT`** — čerpadlo řídí automatika podle teplot a nastavení,
- **`FORCE ON`** — čerpadlo je vynuceně zapnuté.

Další stisk vrátí čerpadlo z `FORCE ON` zpět do `AUT`. Neexistuje režim vynuceného vypnutí; bezpečnostní logika má vždy přednost. Aktuální režimy všech tří čerpadel a teplota kotle jsou zobrazené přímo na displeji PLC. Po restartu začínají všechna čerpadla v režimu `AUT`.

## Nouzové chlazení a bezpečnost

Při teplotě horní části akumulační nádrže nad **85 °C** se aktivuje nouzové chlazení:

- zapnou se čerpadla kotle, topení i boileru,
- směšovací ventil začne odvádět teplo do topného okruhu a reguluje výstupní teplotu přibližně na **60 °C**,
- M5Dial automaticky přejde na obrazovku nádrže a zobrazí červené varování.

Nouzový režim se uvolní po poklesu teploty horní části nádrže na **82 °C nebo méně**. Rozdílný zapínací a vypínací práh zabraňuje rychlému přepínání alarmu.

Při neplatných nebo starších datech z čidel se všechna čerpadla bezpečně zapnou. Při úplné ztrátě měření nebo komunikace se navíc provede jeden časově omezený nouzový přejezd směšovacího ventilu ve směru `OPEN_HOT`, maximálně po dobu odpovídající jednomu plnému přejezdu pohonu. Výpadek M5Dial neblokuje samostatné řízení kotle jednotkou PLC.

> Bezpečnostní funkce firmware nenahrazují mechanické jištění, havarijní termostat, pojistný ventil ani odborně navržené a revidované elektrické a hydraulické zapojení.

## Rychlý přehled architektury

- **Jedna společná RS485 / Modbus RTU sběrnice**
- **STAM PLC M5** = master
- **M5Dial** = slave ID 1
- **NT48A08** = slave ID 2

Podrobné schéma je zde: [docs/diagram.md](docs/diagram.md)

## Technický popis

### Komunikační architektura

Komunikace běží po jedné sběrnici **RS485 / Modbus RTU** rychlostí **9600 Bd, 8N1**. STAM PLC M5 je master, M5Dial používá slave ID 1 a teplotní modul NT48A08 slave ID 2.

### Mapování teplotních čidel

| Kanál NT48A08 | Proměnná ve firmware | Význam |
|---|---|---|
| CH01 | `t_nt_tank_top` | nádrž nahoře |
| CH02 | `t_nt_tank_mid` | nádrž uprostřed |
| CH03 | `t_nt_tank_down` | nádrž dole |
| CH04 | `t_nt_kotel` | kotel |
| CH05 | `t_nt_boiler` | boiler / TUV |
| CH06 | `t_nt_topeni` | topení |

Rozhodující je skutečné mapování v `readNT48()`. Starší komentáře u deklarací proměnných mohou CH04 a CH06 uvádět opačně.

### Modbus registry M5Dial

| Adresa | Název | Význam |
|---|---|---|
| HR0 | `HR_KOTEL_TEMP` | teplota kotle |
| HR1 | `HR_TANK_TOP` | nádrž nahoře |
| HR2 | `HR_TANK_MID` | nádrž uprostřed |
| HR3 | `HR_TANK_DOWN` | nádrž dole |
| HR4 | `HR_TOPENI_ACT` | aktuální teplota topení |
| HR5 | `HR_TOPENI_SET` | nastavená teplota topení |
| HR6 | `HR_BOILER_ACT` | aktuální teplota boileru |
| HR7 | `HR_BOILER_SET` | nastavená teplota boileru |
| HR8 | `HR_KOTEL_TEMP2` | zrcadlená teplota kotle |
| Coil 0 | `CO_KOTEL_PUMP` | stav čerpadla kotle |
| Coil 1 | `CO_TOPENI_PUMP` | stav čerpadla topení |
| Coil 2 | `CO_BOILER_PUMP` | stav čerpadla boileru |

### Výstupy PLC

| Výstup | Funkce | Poznámka |
|---|---|---|
| Relay 1 | čerpadlo kotle | NC / invertovaná logika |
| Relay 2 | čerpadlo topení | NC / invertovaná logika |
| Relay 3 | běh pohonu ventilu | `RUN` |
| Relay 4 | směr pohonu ventilu | `OPEN_HOT` při aktivním směru |
| Port A G1 / GPIO1 | čerpadlo boileru / TUV | přímý GPIO výstup |

### Automatické řízení

- **Kotel:** čerpadlo se natvrdo zapíná od 75 °C a vypíná při 55 °C; uvnitř pracovního pásma se zohledňuje rozdíl teploty kotle a horní části nádrže.
- **Topení:** čerpadlo pracuje s hysterezí ±2 °C kolem hodnoty nastavené na M5Dial. Směšovací ventil upravuje výstupní teplotu topné vody stejným způsobem.
- **Boiler / TUV:** čerpadlo pracuje s hysterezí ±2 °C kolem nastavené hodnoty a zapne se jen tehdy, když je střed nádrže teplejší než boiler.
- **Ventil:** pohon ESBE ARA655 se ovládá krátkými pulzy s prodlevou pro ustálení soustavy; při změně směru je vložena ochranná prodleva.

## Náhledy instalace

### Celkový pohled na hydrauliku a rozvody

![Celkový pohled na instalaci](docs/images/01_instalace_celek.jpg)

### M5Dial ovládací jednotka

![M5Dial ovládací jednotka](docs/images/05_m5dial_box.jpg)

### Rozvaděč s PLC a Modbus modulem

![Rozvaděč s PLC](docs/images/06_rozvadec_celek.jpg)

Další fotky jsou v [galerii](docs/gallery.md).

## Struktura repozitáře

```text
.
├── Display/             # firmware M5Dial
├── JonasPLC/            # firmware STAM PLC M5
├── README.md
└── docs/
    ├── diagram.md
    ├── gallery.md
    ├── images
    │   ├── 01_instalace_celek.jpg
    │   ├── 02_cerpadlo_grundfos_bok.jpg
    │   ├── 03_pohon_ventilu_detail.jpg
    │   ├── 04_napajeni_a_baterie.jpg
    │   ├── 05_m5dial_box.jpg
    │   └── 06_rozvadec_celek.jpg
    └── source
        └── firmware_snapshot.md
```

## Sestavení

Pro každý firmware spusťte PlatformIO v jeho složce, například:

```sh
pio run --project-dir JonasPLC
pio run --project-dir Display
```

Lokální build adresáře `.pio` nejsou součástí repozitáře; PlatformIO je při sestavení vytvoří znovu.
