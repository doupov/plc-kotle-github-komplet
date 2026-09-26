# PLC kotle — STAM PLC + NT48A08 + M5Dial

Dokumentační balíček pro publikaci projektu na GitHubu.

Tento repozitář popisuje domácí systém řízení kotle postavený na:
- **STAM PLC M5** jako hlavním řídicím prvku,
- **NT48A08** jako Modbus teplotním modulu,
- **M5Dial** jako lokálním displeji a panelu nastavení,
- a výkonových výstupech pro **čerpadla** a **pohon směšovacího ventilu**.

> Dokumentace je sestavená podle dodaného firmware a přiložených fotografií instalace.
> Není to svorkové ani revizní schéma 230 V. U výkonové části záměrně popisuje jen to, co jde z kódu a fotek rozumně doložit.

## Obsah dokumentace

- [Schéma systému (Mermaid)](docs/diagram.md)
- [Technický popis a mapování signálů](docs/code-reference.md)
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

## Rychlý přehled architektury

- **Jedna společná RS485 / Modbus RTU sběrnice**
- **STAM PLC M5** = master
- **M5Dial** = slave ID 1
- **NT48A08** = slave ID 2

Podrobné schéma je zde: [docs/diagram.md](docs/diagram.md)

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
    ├── code-reference.md
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

### Stav ověření

- `Display`: sestavení je úspěšné.
- `JonasPLC`: s knihovnou M5StamPLC 1.2.0 sestavení aktuálně končí na použití `M5StamPLC.Display` místo metody `M5StamPLC.Display()`.
