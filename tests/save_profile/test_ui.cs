// Compile with production launcher sources; all saves are synthetic and isolated.
using System;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

namespace BlackXboxLauncher
{
    internal static class SaveProfileUiTests
    {
        private static int checks;
        private static int failures;
        private static void Check(bool value, string description)
        { checks++; if (!value) { failures++; Console.Error.WriteLine("FAIL: " + description); } }
        private static T Field<T>(LauncherForm form, string name)
        { return (T)typeof(LauncherForm).GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(form); }
        private static void Invoke(LauncherForm form, string name, params object[] args)
        { typeof(LauncherForm).GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(form, args); }
        private static void Language(LauncherForm form, int index)
        { Field<ComboBox>(form, "menuLanguage").SelectedIndex = index; }
        private static string Prepare(string output, string name, byte[] profile)
        {
            string root = Path.Combine(output, name + "-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(Path.Combine(root, "game", "UDATA"));
            File.WriteAllBytes(Path.Combine(root, "game", "UDATA", "FirstProfile"), profile);
            return root;
        }
        private static void NamesSurviveRefreshes(string output)
        {
            string root = Prepare(output, "names", ProfileTestData.Create());
            string path = Path.Combine(root, "game", "UDATA", "FirstProfile");
            byte[] original = File.ReadAllBytes(path);
            LauncherLanguage.Set(GameLanguageBanks.English);
            using (var form = new LauncherForm(root, true))
            {
                var name = Field<TextBox>(form, "saveProfileName");
                var slots = Field<ComboBox>(form, "saveSlot");
                var grid = Field<DataGridView>(form, "saveMissionGrid");
                name.Text = "RenamedAlpha";
                grid.Rows[0].Cells[3].Value = "2";
                grid.CurrentCell = grid.Rows[0].Cells[3];
                Invoke(form, "RestoreSelectedMission");
                Check(name.Text == "RenamedAlpha", "Reset mission must preserve the staged operator name.");
                slots.SelectedIndex = 1;
                name.Text = "RenamedBravo";
                slots.SelectedIndex = 0;
                Check(name.Text == "RenamedAlpha", "Returning to a slot must preserve its name draft.");
                slots.SelectedIndex = 1;
                Check(name.Text == "RenamedBravo", "Each slot must retain its independent name draft.");
                for (int language = 1; language <= 5; language++)
                {
                    Language(form, language);
                    Check(name.Text == "RenamedBravo", "Language refresh must preserve the selected name draft.");
                }
                Language(form, 0);
                slots.SelectedIndex = 2;
                Check(!name.Enabled && !grid.Enabled, "The native empty slot must disable name and mission editing.");
                Check(Field<Label>(form, "savePreview").Text.Contains("32 Empty, 0: 0"), "An empty slot's preview must agree with its empty mission grid.");
                Language(form, 1);
                Check(!name.Enabled && !grid.Enabled, "Empty slot controls must remain disabled after language changes.");
                Invoke(form, "ApplyProfileChanges");
                var saved = BlackProfileDocument.Read(File.ReadAllBytes(path));
                Check(saved.Slots[0].Name == "RenamedAlpha" && saved.Slots[1].Name == "RenamedBravo", "Apply must save every active slot's staged name, even with an empty slot selected.");
                Check(!saved.Slots[2].Active, "Applying other slots must preserve the native empty slot.");
                Check(saved.Slots[0].MissionResults[0] == BlackProfileDocument.Read(original).Slots[0].MissionResults[0], "Reset mission must restore the original mission record.");
                Language(form, 0);
                string status = Field<Label>(form, "saveStatus").Text;
                Check(status.Contains(ProfileBackupStore.HashFile(path)), "Language switches must keep the latest applied hash in the status.");
                Check(status.Contains("Applied and verified."), "The latest Apply outcome must be rendered in the selected language.");
            }
        }
        private static void InvalidNamesRemainEditable(string output)
        {
            string root = Prepare(output, "invalid-name", ProfileTestData.Create());
            string path = Path.Combine(root, "game", "UDATA", "FirstProfile");
            string beforeHash = ProfileBackupStore.HashFile(path);
            LauncherLanguage.Set(GameLanguageBanks.English);
            using (var form = new LauncherForm(root, true))
            {
                var name = Field<TextBox>(form, "saveProfileName");
                var slots = Field<ComboBox>(form, "saveSlot");
                var grid = Field<DataGridView>(form, "saveMissionGrid");
                name.Text = "";
                slots.SelectedIndex = 1;
                slots.SelectedIndex = 0;
                Check(name.Text == "", "An unfinished empty name must survive slot changes for correction.");
                Language(form, 1);
                Check(name.Text == "", "An unfinished empty name must survive locale changes.");
                grid.CurrentCell = grid.Rows[0].Cells[3];
                Invoke(form, "RestoreSelectedMission");
                Check(name.Text == "", "Resetting a mission must preserve an unfinished empty name.");
                Invoke(form, "ApplyProfileChanges");
                Check(ProfileBackupStore.HashFile(path) == beforeHash && name.Text == "", "Invalid Apply must preserve both disk bytes and the editable empty draft.");
                Check(ProfileBackupStore.List(ProfileBackupStore.DirectoryFor(root, path)).Length == 0, "Invalid names must be rejected before backup or write.");
                name.Text = "J\u00f6rg";
                slots.SelectedIndex = 1;
                Language(form, 0);
                Invoke(form, "ApplyProfileChanges");
                Check(ProfileBackupStore.HashFile(path) == beforeHash, "An invalid draft on a different slot must prevent the entire Apply.");
                slots.SelectedIndex = 0;
                Check(name.Text == "J\u00f6rg", "Rejected non-ASCII drafts must remain available for correction.");
                name.Text = "Corrected";
                slots.SelectedIndex = 1;
                Invoke(form, "ApplyProfileChanges");
                Check(BlackProfileDocument.Read(File.ReadAllBytes(path)).Slots[0].Name == "Corrected", "Correcting the invalid draft must allow Apply from another active slot.");
            }
        }
        private static void CurrentIdentitySurvivesLocales(string output)
        {
            string root = Prepare(output, "identity", ProfileTestData.Create());
            string firstPath = Path.Combine(root, "game", "UDATA", "FirstProfile");
            string firstHash = ProfileBackupStore.HashFile(firstPath);
            string secondPath = Path.Combine(root, "game", "UDATA", "SecondProfile");
            LauncherLanguage.Set(GameLanguageBanks.English);
            using (var form = new LauncherForm(root, true))
            {
                File.WriteAllBytes(secondPath, ProfileTestData.Create(new[] { true, true, true }, new[] { "SecondAlpha", "SecondBravo", "SecondCharlie" }));
                Invoke(form, "RefreshProfileFiles", secondPath);
                string secondHash = ProfileBackupStore.HashFile(secondPath);
                for (int language = 1; language <= 5; language++)
                {
                    Language(form, language);
                    string info = Field<Label>(form, "saveProfileInfo").Text;
                    Check(info.Contains(secondPath) && info.Contains(secondHash), "Current identity must retain the selected file's literal path and hash across locales.");
                    Check(!info.Contains(firstPath) && !info.Contains(firstHash), "Language refresh must never restore the initially loaded file's identity.");
                    Check(Field<string>(form, "selectedProfilePath") == secondPath, "Displayed and edited profile identities must agree.");
                }
                Language(form, 0);
                Field<TextBox>(form, "saveProfileName").Text = "SecondEdited";
                Invoke(form, "ApplyProfileChanges");
                string appliedHash = ProfileBackupStore.HashFile(secondPath);
                for (int language = 1; language <= 5; language++)
                {
                    Language(form, language);
                    string info = Field<Label>(form, "saveProfileInfo").Text;
                    Check(info.Contains(secondPath) && info.Contains(appliedHash), "Identity must show the current loaded hash after Apply and localization.");
                    Check(Field<Label>(form, "saveStatus").Text.Contains(appliedHash), "Apply status must retain the latest resulting hash across locale changes.");
                }
                Check(ProfileBackupStore.HashFile(firstPath) == firstHash, "Applying the second profile must preserve the first profile bytes.");
                Check(BlackProfileDocument.Read(File.ReadAllBytes(secondPath)).Slots[0].Name == "SecondEdited", "Apply must target the same profile shown by the identity display.");
            }
        }
        [STAThread]
        private static int Main(string[] args)
        {
            try
            {
                if (args.Length != 1) throw new ArgumentException("Expected an isolated output directory.");
                string output = Path.GetFullPath(args[0]);
                Directory.CreateDirectory(output);
                NamesSurviveRefreshes(output);
                InvalidNamesRemainEditable(output);
                CurrentIdentitySurvivesLocales(output);
                Console.WriteLine("Save Profile UI: " + checks + " checks, " + failures + " failures.");
                return failures == 0 ? 0 : 1;
            }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        }
    }
}
