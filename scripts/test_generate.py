"""Focused checks for BLACK's generated-code pipeline."""

from pathlib import Path
import hashlib
import json
import subprocess
from tempfile import TemporaryDirectory
from unittest.mock import patch

import generate


def test_disasm_forwards_feedback_seeds():
    with patch.object(generate, "command") as command:
        generate.run_disasm(
            Path("toolkit"), Path("game.xbe"), Path("analysis.json"),
            Path("disasm"), Path("icall-seeds.json"))

    _, module, arguments = command.call_args.args
    assert module == "tools.disasm", module
    seed_index = arguments.index("--seed-functions")
    assert arguments[seed_index + 1] == Path("icall-seeds.json"), arguments


def test_weapon_trace_hooks_are_bounded_and_repeatable():
    locations = {
        "sub_001E68B0": ["001E68B0"],
        "sub_001E7090": ["001E7090"],
        "sub_001E0EB0": ["001E0EB0", "001E0FCE"],
    }
    for name, labels in locations.items():
        body = "\n".join(f"loc_{label}: ;\n    original();" for label in labels)
        traced = generate.weapon_trace_hooks(name, body)
        assert traced.count("black_weapon_trace(") == len(labels)
        assert traced.count("original();") == len(labels)
        assert generate.weapon_trace_hooks(name, traced) == traced
        try:
            generate.weapon_trace_hooks(name, "unknown location")
        except ValueError:
            pass
        else:
            raise AssertionError("missing guest label must fail closed")
    assert generate.weapon_trace_hooks("unrelated", "unchanged") == "unchanged"


def test_viewmodel_hooks_preserve_draw_lifetime():
    cases = {
        "sub_0015A8F0": "loc_0015A8F0: ;\n    original();\n",
        "sub_001D25D0": "loc_001D25D0: ;\n    original();\n",
        "sub_00172EB0": "loc_00172EB0: ;\n    original();\n    return; /* ret 20 */\n",
        "sub_0016F610": "    PUSH32(l_esp, 0x0016F680u); sub_00269930();\n",
        "sub_000D4A50": "loc_000D4A50: ;\n    PUSH32(l_esp, 0x000D4AFDu); sub_00269930();\n",
    }
    for name, body in cases.items():
        hooked = generate.viewmodel_hooks(name, body)
        assert hooked != body
        assert generate.viewmodel_hooks(name, hooked) == hooked
        try:
            generate.viewmodel_hooks(name, "unknown location")
        except ValueError:
            pass
        else:
            raise AssertionError("changed ownership/upload boundaries must fail closed")
    draw = generate.viewmodel_hooks("sub_00172EB0", cases["sub_00172EB0"])
    assert draw.index("draw_begin") < draw.index("original()") < draw.index("draw_end") < draw.index("return;")
    assert generate.viewmodel_hooks("unrelated", "unchanged") == "unchanged"


def test_weapon_model_trace_boundaries():
    body = "loc_001C5CD0: ;\nloc_001C5D0F: ;\n    MEM32(l_ecx + 0xC) = l_eax;\n"
    fixed = generate.weapon_model_hooks("sub_001C5CD0", body)
    assert fixed.count("black_weapon_model_trace(") == 2
    assert fixed.index("black_weapon_model_trace(1u") < fixed.index("MEM32(l_ecx + 0xC) =")
    assert fixed.index("black_weapon_model_trace(2u") > fixed.index("MEM32(l_ecx + 0xC) =")
    assert generate.weapon_model_hooks("sub_001C5CD0", fixed) == fixed
    for wrong in (body.replace("loc_001C5D0F: ;", "changed:"), body + body):
        try:
            generate.weapon_model_hooks("sub_001C5CD0", wrong)
        except ValueError:
            pass
        else:
            raise AssertionError("changed constructor trace boundary must fail closed")


def test_unsigned_crt_carry_rotations():
    body = "    int _cf = 0;\n    /* TODO: rcr ebx, 1 */\n    /* TODO: rcr eax, 1 */\n"
    for name in ("sub_000A30B0", "sub_000A3380"):
        fixed = generate.unsigned_division_hooks(name, body)
        assert "TODO:" not in fixed
        assert fixed.count("black_rcr_out = ") == 2
        assert fixed.count("_cf = (int)black_rcr_out;") == 2
        assert generate.unsigned_division_hooks(name, fixed) == fixed
        for wrong in (body.replace("int _cf = 0;", ""),
                      body.replace("/* TODO: rcr eax, 1 */", ""), body + body):
            try:
                generate.unsigned_division_hooks(name, wrong)
            except ValueError:
                pass
            else:
                raise AssertionError("changed CRT rotation boundaries must fail closed")
    assert generate.unsigned_division_hooks("unrelated", body) == body


def test_post_transform_audit_describes_final_sources():
    with TemporaryDirectory() as temp:
        root = Path(temp)
        generated = root / "generated"
        output = root / "output"
        generated.mkdir()
        output.mkdir()
        source = (
            "void sub_00001000(void)\n{\n    /* TODO: cli */\n}\n"
            "/* cmp was a TODO */\n/* FAILED: sub_00002000 */\n"
            "void sub_00003000(void)\n{ RECOMP_UNRESOLVED_STUB_HIT(0x3000u); }\n"
            # Exact single-line shape emitted by _render_unresolved_stub.
            "void sub_00124F2C(void) { RECOMP_UNRESOLVED_STUB_HIT(0x00124F2Cu); "
            "g_esp += 4; /* 0x00124F2C: not detected */ }\n")
        for directory in (generated, output):
            (directory / "recomp_0000.c").write_text(source, encoding="utf-8")
            (directory / "recomp_funcs.h").write_text(
                "#define RECOMP_UNRESOLVED_STUB_HIT(va) ((void)0)\n"
                "void sub_00001000(void);\n", encoding="utf-8")
        raw_summary = root / "summary.json"
        original_summary_bytes = b'{"unimplemented":{"cli":[1,2]}}'
        raw_summary.write_bytes(original_summary_bytes)

        audit = generate.write_post_transform_audit(generated, output, raw_summary)

        assert audit["generated_function_definitions"] == 3
        assert audit["remaining_todo_comments"] == 1
        assert audit["failed_translation_markers"] == 1
        assert audit["unresolved_stub_trace_sites"] == 2
        assert len(audit["files"]) == 2
        assert len(audit["source_sha256"]) == 64
        summary = json.loads(raw_summary.read_text(encoding="utf-8"))
        assert summary["summary_stage"] == "raw_recompiler_before_black_transforms"
        assert summary["post_transform_audit"]["source_sha256"] == audit["source_sha256"]
        raw_reference = audit["raw_recompiler_summary"]
        archived_summary = Path(raw_reference["path"])
        assert archived_summary == root / "summary.raw.json"
        assert archived_summary.read_bytes() == original_summary_bytes
        assert raw_reference["sha256"] == hashlib.sha256(
            archived_summary.read_bytes()).hexdigest().upper()
        assert raw_summary.read_bytes() != original_summary_bytes


def test_empty_feedback_is_a_noop():
    # This standalone check needs no installed toolkit. Integration below uses
    # its actual parser against all three empty dump shapes when available.
    with patch.object(generate.subprocess, "run", return_value=
                      subprocess.CompletedProcess([], 0, stdout="false\n")):
        with patch.object(generate, "command") as command:
            assert not generate.merge_icall_feedback(
                Path("toolkit"), Path("db.json"), [Path("empty.dump")])
            command.assert_not_called()


def test_feedback_failures_are_not_ignored():
    with patch.object(generate.subprocess, "run", side_effect=
                      subprocess.CalledProcessError(1, ["feedback-probe"])):
        with patch.object(generate, "command") as command:
            try:
                generate.merge_icall_feedback(
                    Path("toolkit"), Path("db.json"), [Path("bad.dump")])
            except subprocess.CalledProcessError:
                pass
            else:
                raise AssertionError("feedback parser failure was swallowed")
            command.assert_not_called()

    with patch.object(generate.subprocess, "run", return_value=
                      subprocess.CompletedProcess([], 0, stdout="true\n")):
        with patch.object(generate, "command", side_effect=
                          subprocess.CalledProcessError(1, ["feedback-merge"])):
            try:
                generate.merge_icall_feedback(
                    Path("toolkit"), Path("db.json"), [Path("targets.dump")])
            except subprocess.CalledProcessError:
                pass
            else:
                raise AssertionError("feedback merge failure was swallowed")


def test_feedback_merge_with_runtime_parser():
    toolkit = Path(__file__).resolve().parents[1] / "third_party/xboxrecomp"
    if not (toolkit / "tools/recomp/icall_feedback.py").is_file():
        print("Runtime parser integration skipped: toolkit is not installed")
        return
    with TemporaryDirectory() as temp:
        root = Path(temp)
        dump = root / "targets.dump"
        db = root / "icall_targets.json"
        prior_bytes = b'[{"start":"0x00012000","flags":1}]\n'
        empty_dumps = ("", "# icall-feedback v1\n# va flags\n", "00013000 0\n")
        for contents in empty_dumps:
            dump.write_text(contents, encoding="utf-8")
            # Without a prior DB, empty dumps must not manufacture one.
            absent_db = root / ("absent-" + str(len(contents)) + ".json")
            assert not generate.merge_icall_feedback(toolkit, absent_db, [dump])
            assert not absent_db.exists()
            # With a prior DB, every byte of its observations must survive.
            db.write_bytes(prior_bytes)
            assert not generate.merge_icall_feedback(toolkit, db, [dump])
            assert db.read_bytes() == prior_bytes

        dump.write_text("00013000 2\n", encoding="utf-8")
        assert generate.merge_icall_feedback(toolkit, db, [dump])
        merged = {int(entry["start"], 16): entry["flags"]
                  for entry in json.loads(db.read_text(encoding="utf-8"))}
        assert merged == {0x12000: 1, 0x13000: 2}, merged


if __name__ == "__main__":
    test_disasm_forwards_feedback_seeds()
    test_post_transform_audit_describes_final_sources()
    test_empty_feedback_is_a_noop()
    test_feedback_failures_are_not_ignored()
    test_feedback_merge_with_runtime_parser()
    test_weapon_trace_hooks_are_bounded_and_repeatable()
    test_viewmodel_hooks_preserve_draw_lifetime()
    test_unsigned_crt_carry_rotations()
    test_weapon_model_trace_boundaries()
    print("9 checks passed")
