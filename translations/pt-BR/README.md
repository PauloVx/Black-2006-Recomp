# Brazilian Portuguese game strings

`MainUS.json` is the editable Brazilian Portuguese translation source. It stores only entries whose Portuguese text differs from the original English text. Each entry is keyed by the original string hash; its value is the translated Portuguese text. English source text is intentionally omitted because it comes from the user's supplied game files.

The table was extracted by matching the English and PT-BR `MainUS.bin` banks by hash. The source banks had 1,724 matching unique keys; 1,126 entries differ and are included here, while 598 unchanged entries fall back to the supplied English bank.

Reference English bank SHA-256: `B23EEB4AD86A72894B40E7239D0EA8842E5A6BC05936A936401B608070E8C7BB`.

This file is a translation source for review and future language work. The launcher's current language selector still expects a PT-BR `MainUS.bin` in `local/language-banks/pt-BR/`; generating that runtime bank from this JSON is a separate integration step.
