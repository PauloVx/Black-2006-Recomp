using System;
using System.IO;

namespace BlackXboxLauncher
{
    internal static class SaveProfileDocumentTests
    {
        private static int checks;
        private static int failures;

        private static void Check(bool condition, string message)
        {
            checks++;
            if (!condition) { failures++; Console.Error.WriteLine("FAIL: " + message); }
        }

        private static bool Equal(byte[] left, byte[] right)
        {
            if (left.Length != right.Length) return false;
            for (int i = 0; i < left.Length; i++) if (left[i] != right[i]) return false;
            return true;
        }

        private static void RejectEmpty(Action edit, byte[] source, byte[] original, string message)
        {
            byte[] before = (byte[])source.Clone();
            byte[] originalBefore = original == null ? null : (byte[])original.Clone();
            bool rejected = false;
            try { edit(); }
            catch (InvalidDataException) { rejected = true; }
            Check(rejected, message + " rejects a native empty slot.");
            Check(Equal(source, before), message + " preserves source bytes.");
            if (original != null) Check(Equal(original, originalBefore), message + " preserves original bytes.");
        }

        private static void TestNativeEmptySlots()
        {
            byte[] source = ProfileTestData.Create();
            Check(BlackProfileDocument.Validate(source), "Synthetic native empty Profile has a correct checksum and signature.");
            ProfileSnapshot snapshot = BlackProfileDocument.Read(source);
            Check(snapshot.Slots[0].Active && snapshot.Slots[1].Active, "Native nonempty slots are active.");
            Check(!snapshot.Slots[2].Active && snapshot.Slots[2].Name == String.Empty,
                "Marker 0x26 with the native empty flag does not expose the retained name as an active operator.");
            RejectEmpty(delegate { BlackProfileDocument.SetName(source, 2, "Rename"); }, source, null, "Rename");
            RejectEmpty(delegate { BlackProfileDocument.SetMissionAvailable(source, 2, 0, 0, true); }, source, null, "Availability edit");
            RejectEmpty(delegate { BlackProfileDocument.SetMissionResult(source, 2, 0, 0, 2); }, source, null, "Result edit");
            RejectEmpty(delegate { BlackProfileDocument.UnlockAllCampaign(source, 2); }, source, null, "Campaign unlock");
            RejectEmpty(delegate { BlackProfileDocument.RestoreMissionRecord(source, source, 2, 0, 0); }, source, source, "Restore into empty slot");

            byte[] active = ProfileTestData.Create(new[] { true, true, true }, new[] { "Alpha", "Bravo", "Charlie" });
            RejectEmpty(delegate { BlackProfileDocument.RestoreMissionRecord(active, source, 2, 0, 0); }, active, source,
                "Restore from originally empty slot");

            byte[] nonzeroFlag = (byte[])source.Clone();
            nonzeroFlag[2 * 0x778 + 0x76D] = 2;
            ProfileTestData.UpdateIntegrity(nonzeroFlag);
            Check(!BlackProfileDocument.Read(nonzeroFlag).Slots[2].Active, "Any nonzero native empty flag is treated as empty.");
        }

        private static void TestActiveRoundTrips()
        {
            byte[] source = ProfileTestData.Create();
            byte[] sourceBefore = (byte[])source.Clone();
            byte[] renamed = BlackProfileDocument.SetName(source, 0, "New Alpha");
            Check(BlackProfileDocument.Validate(renamed) && BlackProfileDocument.Read(renamed).Slots[0].Name == "New Alpha",
                "Active rename updates the name and integrity fields.");
            Check(Equal(source, sourceBefore), "Active rename leaves the input unchanged.");
            Check(Equal(source, BlackProfileDocument.SetName(renamed, 0, "Alpha")), "Rename and restore roundtrip preserves all bytes.");

            byte[] candidate = BlackProfileDocument.SetMissionResult(source, 1, 3, 7, 2);
            candidate = BlackProfileDocument.SetMissionAvailable(candidate, 1, 3, 7, true);
            ProfileSlotSnapshot edited = BlackProfileDocument.Read(candidate).Slots[1];
            Check(edited.MissionResults[31] == 2 && edited.MissionAvailable[31], "Active final mission edits target the selected record.");
            Check(Equal(source, BlackProfileDocument.RestoreMissionRecord(candidate, source, 1, 3, 7)),
                "Mission edit and restore roundtrip preserves all bytes.");
            Check(Equal(source, sourceBefore), "Mission edits and restore leave the original unchanged.");
        }

        private static void TestBackups(string outputDirectory)
        {
            string fixture = Path.Combine(Path.GetFullPath(outputDirectory), "document-fixture-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(fixture);
            string path = Path.Combine(fixture, "Profile");
            byte[] source = ProfileTestData.Create();
            File.WriteAllBytes(path, source);
            string directory = ProfileBackupStore.DirectoryFor(fixture, path);
            string hash = ProfileBackupStore.HashBytes(source);
            byte[] candidate = BlackProfileDocument.SetName(source, 0, "Applied Alpha");
            string backup = ProfileBackupStore.Apply(path, candidate, directory, hash);
            Check(Equal(File.ReadAllBytes(backup), source), "Apply preserves the original in its dated backup.");
            Check(Equal(File.ReadAllBytes(path), candidate), "Apply writes the complete validated candidate.");
            int count = ProfileBackupStore.List(directory).Length;
            Check(ProfileBackupStore.Apply(path, candidate, directory, ProfileBackupStore.HashBytes(candidate)) == null &&
                ProfileBackupStore.List(directory).Length == count, "Unchanged Apply creates no additional backup.");
            bool rejected = false;
            try { ProfileBackupStore.Apply(path, source, directory, hash); }
            catch (IOException) { rejected = true; }
            Check(rejected && Equal(File.ReadAllBytes(path), candidate), "Stale Apply preserves newer on-disk content.");
            string replaced = ProfileBackupStore.Restore(path, backup, directory);
            Check(Equal(File.ReadAllBytes(path), source) && Equal(File.ReadAllBytes(replaced), candidate),
                "Restore recovers the original and backs up the replaced candidate.");
        }

        private static int Main(string[] args)
        {
            try
            {
                if (args.Length != 1) throw new ArgumentException("Expected a fixture output directory.");
                TestNativeEmptySlots();
                TestActiveRoundTrips();
                TestBackups(args[0]);
                Console.WriteLine("Save Profile document tests: " + checks + " checks, " + failures + " failures.");
                return failures == 0 ? 0 : 1;
            }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
    }
}
