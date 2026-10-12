import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def game_entry(locale, identifier):
    path = ROOT / "localization" / "game" / f"{locale}.jsonl"
    for line in path.read_text(encoding="utf-8").splitlines():
        row = json.loads(line)
        if row["id"] == identifier:
            return row
    raise AssertionError(f"Missing localization ID {identifier} in {locale}")


def subtitle_cue(path, number):
    blocks = path.read_text(encoding="utf-8-sig").strip().split("\n\n")
    for block in blocks:
        lines = block.splitlines()
        if lines and lines[0].strip() == str(number):
            return lines[2].strip()
    raise AssertionError(f"Missing subtitle cue {number} in {path}")


class Pr12ReviewFixes(unittest.TestCase):
    def test_french_source_invariant_entries_remain_placeholders(self):
        for identifier in ("87397DC8", "8E531B04"):
            row = game_entry("fr", identifier)
            self.assertEqual(row["text"], "*")
            self.assertEqual(row["review_status"], "reviewed-source-invariant")

    def test_german_cutscene_sentence_is_split_across_matching_cues(self):
        path = ROOT / "subtitles" / "de" / "03_N.srt"
        self.assertEqual(subtitle_cue(path, 23), "Sergeant Kellar: Die Division wollte Seventh Wave ausschalten")
        self.assertEqual(subtitle_cue(path, 24), "Sergeant Kellar: mit äußerster Härte.")

    def test_interrogator_speaks_cues_five_and_seven_in_every_locale(self):
        expected = {
            "": "Interrogator",
            "pt-BR": "Interrogador",
            "es": "Interrogador",
            "fr": "Interrogateur",
            "de": "Vernehmer",
            "it": "Interrogatore",
        }
        for locale, speaker in expected.items():
            path = ROOT / "subtitles" / locale / "18_N.srt" if locale else ROOT / "subtitles" / "18_N.srt"
            for cue in (5, 7):
                self.assertTrue(subtitle_cue(path, cue).startswith(speaker + ": "), f"{path}, cue {cue}")

    def test_italian_scaling_help_uses_the_matching_launcher_term(self):
        path = ROOT / "localization" / "launcher" / "it.jsonl"
        rows = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
        help_text = next(row["text"] for row in rows if row["id"] == "0029")
        self.assertIn("Estendi riempie la finestra.", help_text)
        self.assertNotIn("Allunga riempie la finestra.", help_text)

    def test_cutscene_subtitles_do_not_show_unresolved_translator_notes(self):
        for path in (ROOT / "subtitles").rglob("07_N.srt"):
            text = path.read_text(encoding="utf-8-sig")
            self.assertNotIn("[SFOR / S4?]", text, str(path))

    def test_french_cutscene_uses_one_spelling_for_mac_carver(self):
        path = ROOT / "subtitles" / "fr" / "07_N.srt"
        text = path.read_text(encoding="utf-8-sig")
        self.assertNotIn("McCarver", text)
        self.assertIn("MacCarver voulait", text)


if __name__ == "__main__":
    unittest.main()
