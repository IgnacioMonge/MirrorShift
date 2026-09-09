# Notas de la versión Mirror Shift 1.0

[English](release-notes-1.0.md)

**Two sides. One reality.**

Mirror Shift lleva Reversi en red a ZX Spectrum Classic, Spectrum Next, el
cartucho SpectraNext, Windows, macOS y Linux. Todas las ediciones comparten las
mismas reglas y el mismo protocolo, por lo que los clientes retro y de
escritorio pueden jugar entre sí mediante Direct TCP o MQTT.

## Primera versión

- Reversi estándar con capturas automáticas, Silence forzado, pistas legales,
  puntuación SELF/ECHO y empates PARADOX.
- Partidas Direct TCP y MQTT entre cualquier cliente Spectrum o de escritorio
  compatible.
- Chat Echoes, relojes de partida y jugada, deshacer, abandono y revanchas.
- Partidas guardadas localmente con restauración sincronizada y aprobada por el
  rival.
- Tres conjuntos de fichas Spectrum, cinco paletas Classic/SpectraNext, temas
  Next a todo color, giro del tablero, animaciones de captura y brillo del lado
  que tiene el turno.
- Paquetes nativos para Classic, Next, SpectraNext y sistemas Qt de escritorio.

## Archivos de la release

| Plataforma | Archivos |
| --- | --- |
| ZX Spectrum Classic | `MIRSHIFT.tap`, `MIRSHIFT.OVL`, `MIRSHIFT.DAT` |
| ZX Spectrum Next | `MIRSHIFT.nex` |
| Cartucho SpectraNext | Directorio de recurso completo con `boot.zx`, `MIRSHIFT.INS`, `MIRSHIFT.PKG` y `MIRSHIFT.SCR` |
| Windows | ZIP portable x86_64 |
| macOS | Paquete de aplicación |
| Linux | AppImage x86_64 |
| Código fuente | Archivo correspondiente al commit de release |

Usa todos los archivos de una misma release. Los archivos auxiliares de
Classic no sustituyen a la edición SpectraNext.

## Requisitos y compatibilidad

- Classic requiere un ZX Spectrum de 48K, almacenamiento divMMC/esxDOS
  escribible y un enlace UART-ESP compatible con ESP-AT.
- Spectrum Next requiere ESP-AT y carga el NEX autocontenido.
- SpectraNext requiere el último firmware estable del cartucho y se instala en
  el almacenamiento XFS local.
- Los guardados Mirror Shift v2 y sus pares Reversi no son compatibles con los
  guardados ni los clientes de ajedrez Shatranj.

Mirror Shift se publica bajo la GNU General Public License v2.0. Consulta
[`LICENSE`](../LICENSE) y [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).
