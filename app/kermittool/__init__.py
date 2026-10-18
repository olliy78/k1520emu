"""k1520Kermit — Datenaustausch Host-PC <-> WEGA (P8000) ueber Kermit.

Entwurf: doc/design/36_kermit.md.  Dieses Paket enthaelt (Stand AP K4) nur die
Bindung ``binding`` an ``libk1520kermit``; Kommandozeile und Oberflaeche folgen
(AP K5/K6).  Der Import des Pakets laedt die Bibliothek NICHT — erst der erste
Aufruf aus ``binding`` tut es, damit ``--paths`` und Werkzeuge ohne gebaute
Bibliothek weiterlaufen.
"""
