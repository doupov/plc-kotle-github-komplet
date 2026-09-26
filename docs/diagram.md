# Schéma systému řízení kotle — STAM PLC + NT48A08 + M5Dial

> Funkční / systémové schéma vytvořené podle firmware a fotografií instalace.
> Není to revizní ani svorkové 230V schéma. Zobrazuje hlavně logické vazby, komunikaci a funkce jednotlivých částí.

```mermaid
flowchart LR
    AC["230 V AC"] --> PSU["Zdroj Mean Well<br/>24 V DC / 1 A"]
    PSU --> PLC["STAM PLC M5<br/>hlavní řídicí jednotka"]

    PLC --- RS485[("RS485 / Modbus RTU<br/>9600 8N1")]
    RS485 -->|"Slave ID 2"| NT48["Eletechsup NT48A08<br/>8× teplotní vstup"]
    RS485 -->|"Slave ID 1"| DIAL["M5Dial<br/>displej + nastavení"]

    S1["CH01 · Nádrž nahoře"] --- NT48
    S2["CH02 · Nádrž střed"] --- NT48
    S3["CH03 · Nádrž dole"] --- NT48
    S4["CH04 · Kotel"] --- NT48
    S5["CH05 · Boiler / TUV"] --- NT48
    S6["CH06 · Topení"] --- NT48

    PLC -->|"R1 · NC / invertovaná logika"| SSR1["Spínací stupeň / SSR<br/>čerpadlo kotle"] --> PK["Čerpadlo kotle<br/>primární okruh"]
    PLC -->|"R2 · NC / invertovaná logika"| SSR2["Spínací stupeň / SSR<br/>čerpadlo topení"] --> PT["Čerpadlo topení"]
    PLC -->|"Port A G1 / GPIO1"| PB["Čerpadlo boileru / TUV"]
    PLC -->|"R3 = RUN<br/>R4 = směr"| VALVE["Tříbodový pohon směšovacího ventilu<br/>OPEN_HOT podle firmware"]

    DIALMAP["Registry M5Dial:<br/>HR0 kotel<br/>HR1-3 nádrž nahoře/střed/dole<br/>HR4 topení aktuální<br/>HR5 topení nastavení<br/>HR6 boiler aktuální<br/>HR7 boiler nastavení<br/>HR8 zrcadlo kotle<br/>Coil 0-2 stav čerpadel"]
    DIAL -. používá .-> DIALMAP

    SAFE["Bezpečnostní logika:<br/>• neplatná / stará data → všechna čerpadla ON<br/>• úplná ztráta čidel → omezený nouzový přejezd ventilu do OPEN_HOT<br/>• přehřátí → všechna čerpadla ON + heat dump<br/>• výpadek M5Dial nesmí zastavit řízení"] -. firmware .-> PLC
```

## Mapování teplotních čidel NT48A08

Schéma vychází z **reálně vykonávaného mapování v `readNT48()`**, tedy:

| Kanál | Význam |
|---|---|
| CH01 | Nádrž nahoře |
| CH02 | Nádrž střed |
| CH03 | Nádrž dole |
| CH04 | Kotel |
| CH05 | Boiler / TUV |
| CH06 | Topení |

> Poznámka: ve zdrojáku jsou u deklarací proměnných starší komentáře, které prohazují CH04 a CH06. Pro dokumentaci je důležité držet se skutečného mapování v `readNT48()`.

## Modbus komunikace

- **Jedna společná sběrnice RS485 / Modbus RTU**
- **STAM PLC M5** je master
- **M5Dial** je slave **ID 1**
- **NT48A08** je slave **ID 2**

## Výstupy PLC

| Výstup | Funkce |
|---|---|
| Relay 1 | Čerpadlo kotle (NC / invertovaná logika v softwaru) |
| Relay 2 | Čerpadlo topení (NC / invertovaná logika v softwaru) |
| Relay 3 | RUN pro pohon směšovacího ventilu |
| Relay 4 | Směr pohonu ventilu; `OPEN_HOT` podle firmware |
| Port A G1 / GPIO1 | Čerpadlo boileru / TUV |

## Registry M5Dial

| Adresa | Význam |
|---|---|
| HR0 | Teplota kotle |
| HR1 | Teplota nádrž nahoře |
| HR2 | Teplota nádrž střed |
| HR3 | Teplota nádrž dole |
| HR4 | Aktuální teplota topení |
| HR5 | Nastavená teplota topení |
| HR6 | Aktuální teplota boileru / TUV |
| HR7 | Nastavená teplota boileru / TUV |
| HR8 | Zrcadlená teplota kotle |
| Coil 0 | Stav čerpadla kotle |
| Coil 1 | Stav čerpadla topení |
| Coil 2 | Stav čerpadla boileru / TUV |

## Poznámky pro GitHub README

- Tohle schéma je ideální pro `README.md` nebo `docs/diagram.md`.
- Mermaid verze se na GitHubu dobře zobrazuje a dá se snadno upravovat.
- Záměrně neobsahuje svorkové ani detailní 230V zapojení tam, kde ho nelze ze snímků a kódu jednoznačně prokázat.
