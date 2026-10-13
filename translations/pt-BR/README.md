# Brazilian Portuguese game strings

`MainUS.json` is the editable Brazilian Portuguese translation source. It stores only entries whose Portuguese text differs from the original English text. Each entry is keyed by the original string hash; its value is the translated Portuguese text. English source text is intentionally omitted because it comes from the user's supplied game files.

The table was extracted by matching the English and PT-BR `MainUS.bin` banks by hash. The source banks had 1,724 matching unique keys; 1,126 entries differ and are included here, while 598 unchanged entries fall back to the supplied English bank.

Reference English bank SHA-256: `B23EEB4AD86A72894B40E7239D0EA8842E5A6BC05936A936401B608070E8C7BB`.

This is the editable PT-BR translation source. The launcher build generates `local/language-banks/pt-BR/MainUS.bin` from it and the verified English bank. The generated bank keeps every English record ID and ordering, applying these translations and retaining English for omitted entries. The licensed English bank and generated PT-BR bank remain local and are not committed.
