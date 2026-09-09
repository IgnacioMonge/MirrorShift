# Documentación técnica de Mirror Shift

[English](README.md) · [Guía de usuario](../README.es.md) · [Guía del cliente Qt](../client/README.es.md)

Este directorio contiene la documentación de ingeniería mantenida de Mirror
Shift 1.0. Los contratos normativos heredados son independientes del transporte y
del cliente:

- [`wire-contract.md`](wire-contract.md): payloads, framing TCP Direct por
  líneas, topics MQTT, intercambio restore y reglas de compatibilidad.
- [`session-core-contract.md`](session-core-contract.md): estado de sesión,
  reducers, reintentos, acknowledgements y semántica entre clientes.

## Arquitectura y responsabilidades

- [`source-layout.md`](source-layout.md): límites y propiedad de los módulos.
- [`architecture-decisions.md`](architecture-decisions.md): decisiones
  transversales aceptadas y su motivo.

La implementación es común para Qt Windows/macOS/Linux, ZX Spectrum Classic y
Spectrum Next y el cartucho SpectraNext. El C común posee las reglas Reversi y el protocolo, sesión y
formato de guardado heredados; el código de escritorio los adapta a Qt y los clientes
Spectrum usan su runtime compacto por target. El parseo y la construcción del
protocolo deben permanecer en common; los dos contratos anteriores son la
fuente de verdad.

## Puntos de entrada de compilación y validación

Ejecuta desde la raíz del repositorio:

```sh
make test          # pruebas de host
make client-test   # build y pruebas Qt
make tap           # TAP/OVL/DAT de Classic
make nex           # NEX autocontenido de Next
make full-check    # guards cross-target de release
```

El cliente Linux se compila contra el Qt del sistema.
El workflow **Build Linux AppImage** empaqueta x86_64, conserva los artefactos
de ejecuciones manuales; adjunta el paquete a la release manualmente. Las
variables de configuración Spectrum son `PORT`, `MQTT_HOST`, `MQTT_PORT`
y `MQTT_CODE`; úsalas solo para un build Spectrum configurado. El flujo
público Qt usa CMake mediante los targets del repositorio; no documentamos
fallback qmake ni CMake raw.

## Referencias de producto y release

- [Notas de versión](../CHANGELOG.md): cambios visibles para el usuario en 1.0.
- [Guía SpectraNext](../packaging/spectranext/README.md): recurso, firmware y compatibilidad de instalación.

`make full-check` cubre pruebas de host, guards de módulos, ABI Classic/Next y
presupuestos de memoria. Escritorio usa `make client-test`. La compilación y
conformidad SpectraNext son independientes; usa los comandos de la guía del
cartucho. La versión documental no sustituye la evidencia de aceptación en
hardware, entre dos clientes o en cada plataforma.
