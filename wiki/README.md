# Wiki-Seiten

Dieses Verzeichnis enthält die manuell gepflegten Seiten des GitHub-Wikis.
`Home.md` ist der deutsche Einstieg, `Home-English.md` der englische. Weitere
Markdown-Seiten können hier ergänzt werden; `_Sidebar.md` steuert die Navigation.

Architektur und Handbücher werden bei jeder Änderung an ihrer Quelldatei durch
`tools/sync_wiki.py` in das Wiki kopiert. Änderungen an diesen automatisch
erzeugten Wiki-Seiten müssen daher in den jeweiligen Quelldokumenten erfolgen.

Damit die Synchronisierung funktioniert, muss das GitHub-Wiki des Repositorys
aktiviert und mindestens einmal initialisiert sein. Die Workflow-Berechtigung
`contents: write` muss in den Actions-Einstellungen erlaubt sein.
