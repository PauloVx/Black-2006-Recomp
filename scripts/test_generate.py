"""Focused checks for BLACK's generated-code pipeline."""

from pathlib import Path
import hashlib
import json
import subprocess
from tempfile import TemporaryDirectory
from unittest.mock import patch

import generate


def test_scope_mask_native_boundary():
    markers = ["loc_00121B50: ;", "loc_00121B69: ;", "loc_00121B89: ;",
               "loc_00121BA9: ;", "l_eax = MEM32(ebp + 0xC);",
               "RECOMP_ABI_CALL(0x000DA2B0u, sub_000DA2B0)",
               "RECOMP_ABI_CALL(0x000DA3A0u, sub_000DA3A0)"]
    operands = ["l_edx = MEM32(ebp + 8);", "l_ecx = l_eax + 0x3740;"]
    body = "\n".join(markers + operands * 2)
    assert generate.scope_mask_boundary("sub_00121B50_gen", body) == body
    assert generate.scope_mask_boundary("unrelated", "unchanged") == "unchanged"
    for marker in markers + operands:
        for changed in (body.replace(marker, "changed", 1), body + "\n" + marker):
            try:
                generate.scope_mask_boundary("sub_00121B50_gen", changed)
            except ValueError:
                pass
            else:
                raise AssertionError("Changed native scope draw contract must fail closed")
    # The two native callbacks have identical leaf ABIs. Pin the actual shape
    # registration and consumer as well, so a sibling glyph hook cannot pass.
    registration = "MEM32(0x562BD8) = 0x121B50;\nMEM32(0x562BA8) = 0x121BC0;"
    branch = "\n".join(("loc_00102D59: ;", "edx = MEM32(ebp + 0xC);",
                         "eax = MEM32(ecx + 0x18);", "PUSH32(esp, edx);",
                         "PUSH32(esp, eax);",
                         "_icall_target = MEM32(0x562BD8); PUSH32(esp, 0x00102D67u);",
                         "loc_00102D67: ;"))
    for name, native in (("sub_00121CB0", registration), ("sub_00102840_gen", branch)):
        assert generate.scope_mask_boundary(name, native) == native
        for changed in (native.replace("0x562BD8", "0x562BA8"),
                        native.replace("0x121B50", "0x121BC0") if native == registration
                        else native.replace("PUSH32(esp, edx);", "PUSH32(esp, eax);")):
            try:
                generate.scope_mask_boundary(name, changed)
            except ValueError:
                pass
            else:
                raise AssertionError("Wrong native callback binding/argument must fail closed")


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
    for name, label, hook in (
            ("sub_001D25D0", "001D25D0", "black_weapon_attachment_trace(1u, l_ecx);"),
            ("sub_001C6150", "001C6150", "black_weapon_attachment_trace(2u, l_ecx);"),
            ("sub_001C5490", "001C5490", "black_weapon_attachment_trace(3u, l_edi);"),
            ("sub_0016B1E0", "0016B1E0", "black_viewmodel_muzzle_gate(l_esi);")):
        body = f"loc_{label}: ;\n    original();\n"
        traced = generate.weapon_trace_hooks(name, body)
        assert traced.count(hook) == 1
        assert traced.index(hook) < traced.index("original();")
        assert generate.weapon_trace_hooks(name, traced) == traced
        for wrong in ("unknown location", body + body):
            try:
                generate.weapon_trace_hooks(name, wrong)
            except ValueError:
                pass
            else:
                raise AssertionError("changed render observation boundary must fail closed")


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


def test_native_lab_loadout_boundary():
    body = "loc_001B78A0: ;\n    l_edx = MEM32(ebp + 0x28);\n    native_inventory_resolve();\n"
    fixed = generate.test_weapon_hooks("sub_001B7800", body)
    assert fixed.count("black_test_weapon_loadout(l_esi);") == 1
    assert fixed.index("black_test_weapon_loadout") < fixed.index("l_edx =") < fixed.index("native_inventory_resolve")
    assert generate.test_weapon_hooks("sub_001B7800", fixed) == fixed
    for wrong in (body.replace("001B78A0", "001B78A1"),
                  body.replace("0x28", "0x2C"), body + body,
                  fixed.replace("0x28", "0x2C")):
        try:
            generate.test_weapon_hooks("sub_001B7800", wrong)
        except ValueError:
            pass
        else:
            raise AssertionError("changed native loadout boundary must fail closed")
    assert generate.test_weapon_hooks("unrelated", body) == body


def test_native_lab_streaming_boundary():
    body = "loc_001EDD8F: ;\n    MEM32(ebp + 0x5AF4) = l_eax;\n    native_streaming();\n"
    fixed = generate.test_weapon_hooks("sub_001EDC90", body)
    assert fixed.count("black_test_stream_unit(ebp);") == 1
    assert fixed.index("0x5AF4") < fixed.index("black_test_stream_unit") < fixed.index("native_streaming")
    assert generate.test_weapon_hooks("sub_001EDC90", fixed) == fixed
    for wrong in (body.replace("001EDD8F", "001EDD90"), body.replace("0x5AF4", "0x5AF8"), body+body):
        try:
            generate.test_weapon_hooks("sub_001EDC90", wrong)
        except ValueError:
            pass
        else:
            raise AssertionError("changed native streaming boundary must fail closed")


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
    test_native_lab_loadout_boundary()
    test_native_lab_streaming_boundary()
    test_scope_mask_native_boundary()
    print("12 checks passed")
