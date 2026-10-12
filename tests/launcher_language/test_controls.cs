// Exercises the real launcher's language event without displaying a window.
using System;
using System.IO;
using System.Reflection;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace BlackXboxLauncher
{
    internal static class LauncherLanguageTests
    {
        private static T Field<T>(LauncherForm form, string name) where T : class
        {
            return (T)typeof(LauncherForm).GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(form);
        }

        internal static void Run(string fixtureRoot, Action<bool, string> check)
        {
            string project = Path.Combine(fixtureRoot, "language-controls");
            string settingsPath = Path.Combine(project, "local", "xbox-launcher.json");
            Directory.CreateDirectory(Path.GetDirectoryName(settingsPath));
            File.WriteAllText(settingsPath, new JavaScriptSerializer().Serialize(new LaunchOptions {
                Threads = 7, MouseSensitivity = 1.25, VerticalFov = 72.5,
                VisualTraceFrames = "120-240/30", KgpuDrawLogFrames = "120,2"
            }));
            LauncherLanguage.Set(GameLanguageBanks.English);
            try
            {
                LauncherLanguage.Set("es");
                var localizationHost = new Panel();
                var userText = new TextBox { Text = "Initial profile name" };
                var userNumber = new NumericUpDown { Value = 3 };
                var translatedLabel = new Label { Text = "Display" };
                localizationHost.Controls.Add(userText);
                localizationHost.Controls.Add(userNumber);
                localizationHost.Controls.Add(translatedLabel);
                LauncherLanguage.Apply(localizationHost);
                userText.Text = "Custom operator";
                userNumber.Value = 8;
                LauncherLanguage.Apply(localizationHost);
                check(userText.Text == "Custom operator", "Applying translations preserves user-entered text.");
                check(userNumber.Value == 8, "Applying translations preserves an edited numeric value.");
                check(translatedLabel.Text == "Pantalla", "Applying translations still updates static labels.");
                check(LauncherLanguage.Text("Status: Any XInput controller works as before.") ==
                    "Status: Cualquier mando XInput funciona como antes.",
                    "Applying a generated locale does not rewrite English fragments inside translated output.");
                check(LauncherLanguage.Text("Status: Gamepad 1 and XInput are supported.") ==
                    "Status: Gamepad 1 and XInput are supported.",
                    "Fallback translations do not replace English fragments inside unmatched words.");
                check(LauncherLanguage.Text("Input") == "Entrada",
                    "Whole-word fallback translation still handles a standalone key.");

                using (var form = new LauncherForm(project, true))
                {
                    Field<NumericUpDown>(form, "threads").Value = 3;
                    Field<NumericUpDown>(form, "mouseSensitivity").Value = 2.5m;
                    Field<NumericUpDown>(form, "verticalFov").Value = 82.5m;
                    Field<TextBox>(form, "visualTraceFrames").Text = "600-900/30";
                    Field<TextBox>(form, "kgpuDrawLogFrames").Text = "420,2";
                    var language = Field<ComboBox>(form, "menuLanguage");
                    var tabs = Field<TabControl>(form, "settingsTabs");
                    foreach (int choice in new[] { 1, 0 })
                    {
                        language.SelectedIndex = choice;
                        check(tabs.TabPages[0].Text == (choice == 1 ? "Tela" : "Display"),
                            "Changing language translates captions in both directions.");
                        var saved = LaunchOptions.FromSettingsJson(File.ReadAllText(settingsPath));
                        check(saved.MenuLanguage == (choice == 1 ? GameLanguageBanks.PortugueseBrazil : GameLanguageBanks.English),
                            "Changing language saves the selected locale.");
                        check(saved.Threads == 3 && Field<NumericUpDown>(form, "threads").Value == 3,
                            "Changing language preserves edited renderer threads in the UI and saved settings.");
                        check(saved.MouseSensitivity == 2.5 && Field<NumericUpDown>(form, "mouseSensitivity").Value == 2.5m,
                            "Changing language preserves edited mouse sensitivity.");
                        check(saved.VerticalFov == 82.5 && Field<NumericUpDown>(form, "verticalFov").Value == 82.5m,
                            "Changing language preserves edited field of view.");
                        check(saved.VisualTraceFrames == "600-900/30" && Field<TextBox>(form, "visualTraceFrames").Text == "600-900/30",
                            "Changing language preserves edited trace frames.");
                        check(saved.KgpuDrawLogFrames == "420,2" && Field<TextBox>(form, "kgpuDrawLogFrames").Text == "420,2",
                            "Changing language preserves previously populated GPU draw frames.");
                    }
                }
            }
            finally { LauncherLanguage.Set(GameLanguageBanks.English); }
        }
    }
}
