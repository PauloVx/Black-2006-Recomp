# Localization sources and review status

The existing pull request provides the English and Brazilian Portuguese launcher and in-game text baseline. This follow-up adds Spanish, French, German, and Italian catalogs and cutscene subtitle tracks. These four languages are still machine-draft/editorial-review material; edited text has not had a complete native-speaker review. The launcher keeps draft languages visibly labeled.

The four `localization/game/<locale>.jsonl` files preserve all 1,724 MainUS IDs. The launcher build preserves a recognized original English bank from `game/language/strings/MainUS.bin` at `local/language-banks/en-US/MainUS.bin` before generating these runtime banks, including on a clean setup. For a previously translated installation, provide the original English bank at that fallback path first. The build helper writes generated banks under ignored `local/language-banks/<locale>/`. No licensed game bank is included here. The launcher UI catalogs are generated from `localization/launcher/` and can be reviewed offline.

Cutscene subtitles are provided for nine dialogue-bearing video stems in English, Brazilian Portuguese, Spanish, French, German, and Italian. The ES/FR/DE/IT tracks are pending fluent-speaker and complete audio review. Speaker-attribution uncertainties are listed in `subtitles/SPEAKER_ATTRIBUTION_REVIEW.csv`. The experimental retimed candidates are not included as runtime tracks.

Gameplay dialogue candidates are not included or loaded by this change. Their ASR confidence, bank identity, and synchronization remain unverified.
