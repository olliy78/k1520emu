# Code-Signing-Policy (Entwurf)

> Entwurf für die Bewerbung bei der SignPath Foundation. Vor dem Einreichen Rollen und Namen prüfen
> und die Seite in der README verlinken.

Free code signing provided by [SignPath.io](https://signpath.io), certificate by
[SignPath Foundation](https://signpath.org).

## Was signiert wird

Nur das Windows-Installationsprogramm `K1520emu-<version>-win-x64-setup.exe` der Veröffentlichungen
(Release-Läufe auf GitHub-gehosteten Läufern aus dem Quelltext dieses Repositorys). Es werden keine
fremden Binärdateien signiert.

## Rollen

| Rolle | Wer |
|-------|-----|
| Autor / Committer | Olaf Krieger (Repository-Inhaber) und Mitwirkende per Pull-Request |
| Prüfer | Repository-Inhaber |
| Freigeber der Signieranträge | Repository-Inhaber |

## Datenschutz

Das Programm überträgt keine Daten an Dritte. Beim Installieren lädt das Setup Python (python-build-standalone,
GitHub) und Qt/PySide6 sowie weitere Python-Pakete (PyPI) mit festgelegten Prüfsummen nach; dabei werden die
Adressdaten übertragen, die bei jedem Download anfallen. Es gibt keine Telemetrie.
