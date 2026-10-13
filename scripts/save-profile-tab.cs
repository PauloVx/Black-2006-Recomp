using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Text;
using System.Windows.Forms;
namespace BlackXboxLauncher
{
    internal sealed partial class LauncherForm
    {
        private readonly ComboBox saveProfilePath = new ComboBox();
        private readonly ComboBox saveSlot = new ComboBox();
        private readonly ComboBox saveSilverMode = new ComboBox();
        private readonly CheckBox saveInfiniteAmmo = new CheckBox();
        private readonly TextBox saveProfileName = new TextBox();
        private readonly DataGridView saveMissionGrid = new DataGridView();
        private readonly Label saveProfileInfo = new Label();
        private readonly Label savePreview = new Label();
        private readonly Label saveStatus = new Label();
        private byte[] originalProfile, workingProfile;
        private string selectedProfilePath, loadedProfileHash;
        private string[] pendingProfileNames;
        private string[] saveStatusParts = new string[0];
        private bool refreshingSaveTab;

        private void BuildSaveTab(TabPage page)
        {
            AddOptionLabel(page, LauncherLanguage.Text("Profile"), 14, 15, 70);
            saveProfilePath.SetBounds(84, 11, 390, 30);
            saveProfilePath.DropDownStyle = ComboBoxStyle.DropDownList;
            saveProfilePath.BackColor = Color.FromArgb(37, 43, 39); saveProfilePath.ForeColor = ForeColor;
            saveProfilePath.AccessibleName = "Profile file";
            page.Controls.Add(saveProfilePath);
            var reload = new Button { Text = LauncherLanguage.Text("Reload"), Location = new Point(484, 10), Size = new Size(100, 31), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(39, 46, 41), ForeColor = ForeColor, AccessibleName = "Reload profiles" };
            reload.FlatAppearance.BorderSize = 0; page.Controls.Add(reload);
            var browse = new Button { Text = LauncherLanguage.Text("Browse file"), Location = new Point(594, 10), Size = new Size(168, 31), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(39, 46, 41), ForeColor = ForeColor, AccessibleName = "Browse Profile file" };
            browse.FlatAppearance.BorderSize = 0; page.Controls.Add(browse);
            // These labels contain current file data, so exclude them from the
            // launcher's static English-text cache and render them from state.
            saveProfileInfo.Tag = savePreview.Tag = saveStatus.Tag = new object();
            saveProfileInfo.SetBounds(14, 47, 700, 38); saveProfileInfo.ForeColor = muted; page.Controls.Add(saveProfileInfo);
            AddOptionLabel(page, "Slot", 14, 91, 48);
            saveSlot.SetBounds(65, 87, 120, 30); saveSlot.DropDownStyle = ComboBoxStyle.DropDownList;
            saveSlot.BackColor = Color.FromArgb(37, 43, 39); saveSlot.ForeColor = ForeColor; saveSlot.AccessibleName = "Profile slot";
            page.Controls.Add(saveSlot);
            AddOptionLabel(page, LauncherLanguage.Text("Operator name"), 205, 91, 140);
            saveProfileName.SetBounds(350, 87, 250, 30); saveProfileName.MaxLength = 79; saveProfileName.AccessibleName = "Profile name";
            saveProfileName.BackColor = Color.FromArgb(37, 43, 39); saveProfileName.ForeColor = ForeColor; page.Controls.Add(saveProfileName);
            var unlockAll = new Button { Text = LauncherLanguage.Text("Unlock all"), Location = new Point(612, 86), Size = new Size(130, 32), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(39, 46, 41), ForeColor = ForeColor };
            unlockAll.FlatAppearance.BorderSize = 0; page.Controls.Add(unlockAll);
            AddOptionLabel(page, LauncherLanguage.Text("Mission results: Empty = no result; 0 = no threshold; 1 = below target; 2 = target reached."), 14, 125, 760).ForeColor = muted;
            saveMissionGrid.SetBounds(14, 153, 600, 180); saveMissionGrid.Anchor = AnchorStyles.Top | AnchorStyles.Left;
            saveMissionGrid.AccessibleName = "Profile mission editor";
            saveMissionGrid.AllowUserToAddRows = false; saveMissionGrid.AllowUserToDeleteRows = false; saveMissionGrid.AllowUserToResizeRows = false;
            saveMissionGrid.RowHeadersVisible = false; saveMissionGrid.ScrollBars = ScrollBars.Vertical; saveMissionGrid.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.None;
            saveMissionGrid.BackgroundColor = BackColor; saveMissionGrid.GridColor = Color.FromArgb(57, 64, 59);
            saveMissionGrid.DefaultCellStyle.BackColor = Color.FromArgb(29, 34, 31); saveMissionGrid.DefaultCellStyle.ForeColor = ForeColor;
            saveMissionGrid.ColumnHeadersDefaultCellStyle.BackColor = Color.FromArgb(39, 46, 41); saveMissionGrid.ColumnHeadersDefaultCellStyle.ForeColor = ForeColor;
            saveMissionGrid.EnableHeadersVisualStyles = false; saveMissionGrid.SelectionMode = DataGridViewSelectionMode.CellSelect;
            saveMissionGrid.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = LauncherLanguage.Text("Difficulty"), ReadOnly = true, Width = 145 });
            saveMissionGrid.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = LauncherLanguage.Text("Mission"), ReadOnly = true, Width = 145 });
            saveMissionGrid.Columns.Add(new DataGridViewCheckBoxColumn { HeaderText = LauncherLanguage.Text("Available"), Width = 110 });
            var resultColumn = new DataGridViewComboBoxColumn { HeaderText = LauncherLanguage.Text("Result"), Width = 180, FlatStyle = FlatStyle.Flat };
            resultColumn.Items.AddRange("Empty", "0", "1", "2"); saveMissionGrid.Columns.Add(resultColumn);
            string[] difficulties = { "Easy", "Normal", "Hard", "Black Ops" };
            for (int diff = 0; diff < 4; diff++) for (int mission = 0; mission < 8; mission++)
                saveMissionGrid.Rows.Add(difficulties[diff], "Mission " + (mission + 1).ToString("00", CultureInfo.InvariantCulture), false, "Empty");
            page.Controls.Add(saveMissionGrid);
            var apply = new Button { Text = LauncherLanguage.Text("Apply Profile changes"), Location = new Point(14, 340), Size = new Size(210, 38), FlatStyle = FlatStyle.Flat, BackColor = accent, ForeColor = BackColor, Font = OwnFont(9, FontStyle.Bold), AccessibleName = LauncherLanguage.Text("Apply Profile changes") };
            apply.FlatAppearance.BorderSize = 0; page.Controls.Add(apply);
            var restore = new Button { Text = LauncherLanguage.Text("Restore latest backup"), Location = new Point(234, 340), Size = new Size(190, 38), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(39, 46, 41), ForeColor = ForeColor };
            restore.Text = LauncherLanguage.Text("Choose Profile backup");
            restore.FlatAppearance.BorderSize = 0; page.Controls.Add(restore);
            var resetMission = new Button { Text = LauncherLanguage.Text("Reset selected mission"), Location = new Point(434, 340), Size = new Size(190, 38), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(39, 46, 41), ForeColor = ForeColor, AccessibleName = "Reset selected mission" };
            resetMission.FlatAppearance.BorderSize = 0; page.Controls.Add(resetMission);
            AddOptionLabel(page, LauncherLanguage.Text("Silver weapons"), 14, 390, 135);
            saveSilverMode.SetBounds(150, 386, 270, 30); saveSilverMode.DropDownStyle = ComboBoxStyle.DropDownList;
            saveSilverMode.BackColor = Color.FromArgb(37, 43, 39); saveSilverMode.ForeColor = ForeColor; saveSilverMode.AccessibleName = "Silver weapons runtime mode";
            saveSilverMode.Items.Add(new DisplayChoice("native", LauncherLanguage.Text("Use Profile/game setting")));
            saveSilverMode.Items.Add(new DisplayChoice("force-on", LauncherLanguage.Text("Force on")));
            saveSilverMode.Items.Add(new DisplayChoice("force-off", LauncherLanguage.Text("Force off")));
            saveSilverMode.SelectedIndex = 0; page.Controls.Add(saveSilverMode);
            AddOptionLabel(page, LauncherLanguage.Text("Runtime only; does not edit the Profile."), 435, 390, 330).ForeColor = muted;
            saveInfiniteAmmo.Text = LauncherLanguage.Text("Infinite ammunition (runtime only)");
            saveInfiniteAmmo.SetBounds(14, 420, 300, 26); saveInfiniteAmmo.ForeColor = ForeColor;
            saveInfiniteAmmo.AccessibleName = "Infinite ammo runtime toggle"; page.Controls.Add(saveInfiniteAmmo);
            savePreview.SetBounds(14, 450, 750, 56); savePreview.ForeColor = muted; savePreview.AutoSize = false;
            savePreview.AccessibleName = "Profile preview"; savePreview.Text = LauncherLanguage.Text("Select a Profile slot to preview pending values and hash."); page.Controls.Add(savePreview);
            saveStatus.SetBounds(14, 510, 750, 48); saveStatus.ForeColor = muted; page.Controls.Add(saveStatus);
            SetSettingsTip(unlockAll, "Makes all 32 campaign entries available and marks their results as reached. This changes saved campaign progress; it does not independently prove or grant every weapon reward.");

            reload.Click += delegate { RefreshProfileFiles(SelectedProfilePath); };
            browse.Click += delegate { BrowseProfileFile(); };
            saveProfilePath.SelectedIndexChanged += delegate { if (!refreshingSaveTab) LoadSelectedProfile(); };
            saveSlot.SelectedIndexChanged += delegate { if (!refreshingSaveTab) RefreshSaveSlot(); };
            saveMissionGrid.CurrentCellDirtyStateChanged += delegate
            {
                if (saveMissionGrid.IsCurrentCellDirty) saveMissionGrid.CommitEdit(DataGridViewDataErrorContexts.Commit);
            };
            saveMissionGrid.CellValueChanged += delegate(object sender, DataGridViewCellEventArgs e) { if (!refreshingSaveTab && e.RowIndex >= 0) SaveGridCellChanged(e.RowIndex, e.ColumnIndex); };
            saveMissionGrid.DataError += delegate(object sender, DataGridViewDataErrorEventArgs e) { e.ThrowException = false; };
            saveProfileName.TextChanged += delegate
            {
                if (!refreshingSaveTab && pendingProfileNames != null && saveProfileName.Enabled && saveSlot.SelectedIndex >= 0)
                {
                    // Keep even unfinished/invalid text until the user corrects
                    // it. Validation belongs to the candidate, not UI refresh.
                    pendingProfileNames[saveSlot.SelectedIndex] = saveProfileName.Text;
                    SetSaveStatus("Profile name has pending changes. Review the preview before Apply.");
                    RefreshProfilePreview();
                }
            };
            unlockAll.Click += delegate
            {
                if (workingProfile == null || saveSlot.SelectedIndex < 0) return;
                if (MessageBox.Show(this, LauncherLanguage.Text("This marks every mission available and every mission result complete for the selected slot. It changes campaign progress. Continue?"), LauncherLanguage.Text("Unlock campaign"), MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
                try { workingProfile = BlackProfileDocument.UnlockAllCampaign(workingProfile, saveSlot.SelectedIndex); RefreshSaveSlot(); SetSaveStatus("All campaign entries are staged as available and complete. Nothing is written until Apply."); }
                catch (Exception error) { ShowSaveError(error); }
            };
            apply.Click += delegate { ApplyProfileChanges(); };
            restore.Click += delegate { ChooseProfileBackup(); };
            resetMission.Click += delegate { RestoreSelectedMission(); };

            for (int i = 0; i < 3; i++) saveSlot.Items.Add("Slot " + (i + 1).ToString(CultureInfo.InvariantCulture));
            saveSlot.SelectedIndex = 0;
            RefreshProfileFiles(null);
        }

        private void RefreshProfileFiles(string preferredPath)
        {
            bool updating = false;
            string scanError = null;
            refreshingSaveTab = true;
            try
            {
                saveProfilePath.BeginUpdate(); updating = true;
                saveProfilePath.Items.Clear();
                string udata = Path.Combine(root, "game", "UDATA");
                if (Directory.Exists(udata))
                    foreach (string file in Directory.GetFiles(udata, "*", SearchOption.AllDirectories))
                    {
                        var candidate = new FileInfo(file);
                        if (candidate.Length != BlackProfileDocument.FileSize) continue;
                        if (BlackProfileDocument.Validate(File.ReadAllBytes(file)))
                            saveProfilePath.Items.Add(new ProfileFileChoice(file, root));
                    }

                int selected = -1;
                if (!String.IsNullOrEmpty(preferredPath))
                    for (int i = 0; i < saveProfilePath.Items.Count; i++)
                    {
                        var choice = saveProfilePath.Items[i] as ProfileFileChoice;
                        if (choice != null && String.Equals(choice.Path, Path.GetFullPath(preferredPath), StringComparison.OrdinalIgnoreCase))
                        { selected = i; break; }
                    }
                if (selected < 0 && saveProfilePath.Items.Count == 1) selected = 0;
                saveProfilePath.SelectedIndex = selected;
            }
            catch (Exception error)
            {
                scanError = error.Message;
            }
            finally
            {
                if (updating) saveProfilePath.EndUpdate();
                refreshingSaveTab = false;
            }
            LoadSelectedProfile();
            if (scanError != null) SetSaveStatus("Could not scan game\\UDATA: ", "", scanError);
        }

        private void BrowseProfileFile()
        {
            string initialDirectory = String.IsNullOrEmpty(SelectedProfilePath)
                ? Path.Combine(root, "game", "UDATA") : Path.GetDirectoryName(SelectedProfilePath);
            if (!Directory.Exists(initialDirectory)) initialDirectory = root;
            using (var dialog = new OpenFileDialog())
            {
                dialog.Title = LauncherLanguage.Text("Find a Profile file");
                dialog.Filter = LauncherLanguage.Text("All files") + " (*.*)|*.*";
                dialog.CheckFileExists = true;
                dialog.Multiselect = false;
                dialog.InitialDirectory = initialDirectory;
                if (dialog.ShowDialog(this) != DialogResult.OK) return;

                try
                {
                    var candidate = new FileInfo(dialog.FileName);
                    if (candidate.Length != BlackProfileDocument.FileSize || !BlackProfileDocument.Validate(File.ReadAllBytes(candidate.FullName)))
                        throw new InvalidDataException("Selected file is not a valid 6,144-byte BLACK Profile.");

                    string fullPath = Path.GetFullPath(candidate.FullName);
                    int selected = -1;
                    refreshingSaveTab = true;
                    for (int i = 0; i < saveProfilePath.Items.Count; i++)
                    {
                        var choice = saveProfilePath.Items[i] as ProfileFileChoice;
                        if (choice != null && String.Equals(choice.Path, fullPath, StringComparison.OrdinalIgnoreCase))
                        { selected = i; break; }
                    }
                    if (selected < 0)
                    {
                        saveProfilePath.Items.Add(new ProfileFileChoice(fullPath, root));
                        selected = saveProfilePath.Items.Count - 1;
                    }
                    saveProfilePath.SelectedIndex = selected;
                    refreshingSaveTab = false;
                    LoadSelectedProfile();
                }
                catch (Exception error)
                {
                    refreshingSaveTab = false;
                    ShowSaveError(error);
                }
            }
        }

        private sealed class ProfileFileChoice
        {
            internal readonly string Path;
            private readonly string display;
            internal ProfileFileChoice(string path, string projectRoot)
            {
                Path = System.IO.Path.GetFullPath(path);
                Uri rootUri = new Uri(System.IO.Path.GetFullPath(projectRoot).TrimEnd(System.IO.Path.DirectorySeparatorChar) + System.IO.Path.DirectorySeparatorChar);
                display = Uri.UnescapeDataString(rootUri.MakeRelativeUri(new Uri(Path)).ToString()).Replace('/', System.IO.Path.DirectorySeparatorChar);
            }
            public override string ToString() { return display; }
        }

        private string SelectedProfilePath
        {
            get { var choice = saveProfilePath.SelectedItem as ProfileFileChoice; return choice == null ? null : choice.Path; }
        }

        private void LoadSelectedProfile()
        {
            selectedProfilePath = SelectedProfilePath;
            originalProfile = workingProfile = null; loadedProfileHash = null; pendingProfileNames = null;
            if (String.IsNullOrEmpty(selectedProfilePath))
            {
                RefreshProfileIdentity();
                saveMissionGrid.Enabled = false; saveProfileName.Enabled = false; saveProfileName.Clear();
                savePreview.Text = LauncherLanguage.Text("Select a Profile slot to preview pending values and hash.");
                SetSaveStatus("");
                return;
            }
            try
            {
                originalProfile = File.ReadAllBytes(selectedProfilePath);
                ProfileSnapshot snapshot = BlackProfileDocument.Read(originalProfile);
                loadedProfileHash = snapshot.Sha256; workingProfile = (byte[])originalProfile.Clone();
                pendingProfileNames = new string[snapshot.Slots.Length];
                for (int slot = 0; slot < pendingProfileNames.Length; slot++) pendingProfileNames[slot] = snapshot.Slots[slot].Name;
                RefreshProfileIdentity();
                saveMissionGrid.Enabled = true; saveProfileName.Enabled = true;
                RefreshSaveSlot();
                string[] backups = ProfileBackupStore.List(ProfileBackupStore.DirectoryFor(root, selectedProfilePath));
                SetSaveStatus("Loaded and integrity-checked. Changes stay pending until Apply.", "  " +
                    backups.Length.ToString(CultureInfo.InvariantCulture) + " ", "validated Profile backups available.");
            }
            catch (Exception error)
            {
                saveMissionGrid.Enabled = saveProfileName.Enabled = false;
                saveProfileInfo.Text = LauncherLanguage.Text("This Profile could not be safely edited."); ShowSaveError(error);
            }
        }

        private void RefreshSaveSlot()
        {
            if (workingProfile == null || saveSlot.SelectedIndex < 0) return;
            try
            {
                refreshingSaveTab = true;
                ProfileSlotSnapshot slot = BlackProfileDocument.Read(workingProfile).Slots[saveSlot.SelectedIndex];
                saveProfileName.Enabled = saveMissionGrid.Enabled = slot.Active;
                saveProfileName.Text = slot.Active ? pendingProfileNames[saveSlot.SelectedIndex] : LauncherLanguage.Text("(empty slot)");
                for (int i = 0; i < 32; i++)
                {
                    saveMissionGrid.Rows[i].Cells[2].Value = slot.Active && slot.MissionAvailable[i];
                    uint value = slot.Active ? slot.MissionResults[i] : 0xFFFFFFFF;
                    saveMissionGrid.Rows[i].Cells[3].Value = value == 0xFFFFFFFF ? LauncherLanguage.Text("Empty") : value.ToString(CultureInfo.InvariantCulture);
                }
                RefreshProfilePreview();
            }
            catch (Exception error) { saveMissionGrid.Enabled = saveProfileName.Enabled = false; ShowSaveError(error); }
            finally { refreshingSaveTab = false; }
        }

        private void SaveGridCellChanged(int row, int column)
        {
            if (workingProfile == null || saveSlot.SelectedIndex < 0 || (column != 2 && column != 3)) return;
            try
            {
                int difficulty = row / 8, mission = row % 8, slot = saveSlot.SelectedIndex;
                if (column == 2)
                    workingProfile = BlackProfileDocument.SetMissionAvailable(workingProfile, slot, difficulty, mission, Convert.ToBoolean(saveMissionGrid.Rows[row].Cells[column].Value, CultureInfo.InvariantCulture));
                else
                {
                    string value = Convert.ToString(saveMissionGrid.Rows[row].Cells[column].Value, CultureInfo.InvariantCulture);
                    uint result = value == LauncherLanguage.Text("Empty") || value == "Empty" ? 0xFFFFFFFF : UInt32.Parse(value, CultureInfo.InvariantCulture);
                    workingProfile = BlackProfileDocument.SetMissionResult(workingProfile, slot, difficulty, mission, result);
                }
                SetSaveStatus("Mission changes are pending. Review the preview before Apply.");
                RefreshProfilePreview();
            }
            catch (Exception error) { ShowSaveError(error); RefreshSaveSlot(); }
        }

        private void ApplyProfileChanges()
        {
            if (workingProfile == null || String.IsNullOrEmpty(selectedProfilePath)) return;
            if (GameSession.ExistingProcess(root) != 0) { ShowSaveError(new InvalidOperationException("Close BLACK before applying a Profile.")); return; }
            try
            {
                if (HashFile(selectedProfilePath) != loadedProfileHash) throw new IOException("The Profile changed on disk after it was loaded. Reload it before editing to avoid overwriting newer progress.");
                string beforeHash = HashFile(selectedProfilePath);
                byte[] candidate = BuildProfileCandidate();
                if (!BlackProfileDocument.Validate(candidate)) throw new InvalidDataException("Edited Profile did not pass integrity validation.");
                string candidateHash = ProfileBackupStore.HashBytes(candidate);
                string backupDirectory = ProfileBackupStore.DirectoryFor(root, selectedProfilePath);
                string backup = ProfileBackupStore.Apply(selectedProfilePath, candidate, backupDirectory, loadedProfileHash);
                if (backup == null)
                {
                    AppendProfileEditLog("NO_CHANGE", beforeHash, candidateHash, beforeHash, "No changes to apply");
                    LoadSelectedProfile();
                    SetSaveStatus("No changes to apply. Profile unchanged; no backup created. SHA-256 ", beforeHash);
                    return;
                }
                string afterHash = HashFile(selectedProfilePath);
                if (afterHash != candidateHash) throw new IOException("Applied Profile hash does not match the validated candidate.");
                LoadSelectedProfile();
                AppendProfileEditLog("APPLIED", beforeHash, candidateHash, afterHash, backup);
                SetSaveStatus("Applied and verified. Before SHA-256 ", beforeHash, "; after SHA-256 ", afterHash, "; backup: ", backup);
            }
            catch (Exception error) { ShowSaveError(error); }
        }

        private void ChooseProfileBackup()
        {
            if (String.IsNullOrEmpty(selectedProfilePath)) return;
            if (GameSession.ExistingProcess(root) != 0) { ShowSaveError(new InvalidOperationException("Close BLACK before restoring a Profile.")); return; }
            string directory = ProfileBackupStore.DirectoryFor(root, selectedProfilePath);
            string[] backups = ProfileBackupStore.List(directory);
            if (backups.Length == 0) { SetSaveStatus("No Profile backup exists yet."); return; }
            try
            {
                using (var dialog = new OpenFileDialog())
                {
                    dialog.Title = LauncherLanguage.Text("Choose Profile backup");
                    dialog.Filter = LauncherLanguage.Text("Profile backups") + " (*.bak)|*.bak";
                    dialog.CheckFileExists = true; dialog.Multiselect = false; dialog.InitialDirectory = directory;
                    if (dialog.ShowDialog(this) != DialogResult.OK) return;
                    string backup = Path.GetFullPath(dialog.FileName);
                    if (Array.IndexOf(backups, backup) < 0) throw new InvalidDataException("Selected file is not a valid backup for this Profile.");
                    if (MessageBox.Show(this, LauncherLanguage.Text("Restore this Profile backup? A backup of the current file will be created first.\n\n") + backup, LauncherLanguage.Text("Restore Profile"), MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
                    string backupCopy = ProfileBackupStore.Restore(selectedProfilePath, backup, directory);
                    LoadSelectedProfile(); SetSaveStatus("Backup restored; current file was also backed up as ", backupCopy);
                }
            }
            catch (Exception error) { ShowSaveError(error); }
        }

        private void RestoreSelectedMission()
        {
            if (workingProfile == null || originalProfile == null || saveSlot.SelectedIndex < 0 || saveMissionGrid.CurrentCell == null) return;
            int row = saveMissionGrid.CurrentCell.RowIndex;
            try
            {
                workingProfile = BlackProfileDocument.RestoreMissionRecord(workingProfile, originalProfile,
                    saveSlot.SelectedIndex, row / 8, row % 8);
                RefreshSaveSlot();
                SetSaveStatus("Selected mission restored to its values when this Profile was loaded. Apply writes the change.");
            }
            catch (Exception error) { ShowSaveError(error); }
        }

        private static string HashFile(string path) { return ProfileBackupStore.HashFile(path); }

        private byte[] BuildProfileCandidate()
        {
            byte[] candidate = (byte[])workingProfile.Clone();
            ProfileSnapshot snapshot = BlackProfileDocument.Read(candidate);
            for (int slot = 0; slot < snapshot.Slots.Length; slot++)
                if (snapshot.Slots[slot].Active && pendingProfileNames[slot] != snapshot.Slots[slot].Name)
                    candidate = BlackProfileDocument.SetName(candidate, slot, pendingProfileNames[slot]);
            return candidate;
        }

        private void RefreshProfileIdentity()
        {
            if (String.IsNullOrEmpty(selectedProfilePath))
                saveProfileInfo.Text = LauncherLanguage.Text(saveProfilePath.Items.Count == 0
                    ? "No Profile found under game\\UDATA." : "Choose one of the Profile files found in game\\UDATA.");
            else if (workingProfile == null)
                saveProfileInfo.Text = LauncherLanguage.Text("This Profile could not be safely edited.");
            else
                saveProfileInfo.Text = LauncherLanguage.Text("6,144 bytes  |  SHA-256 ") + loadedProfileHash + "  |  " + selectedProfilePath;
        }

        // Alternate localized English fragments and literal data (hashes,
        // paths, counts), preserving the latest outcome across locale changes.
        private void SetSaveStatus(params string[] parts)
        {
            saveStatusParts = parts;
            RefreshSaveStatus();
        }

        private void RefreshSaveStatus()
        {
            var text = new StringBuilder();
            for (int i = 0; i < saveStatusParts.Length; i++)
                text.Append((i & 1) == 0 ? LauncherLanguage.Text(saveStatusParts[i]) : saveStatusParts[i]);
            saveStatus.Text = text.ToString();
        }

        private void AppendProfileEditLog(string outcome, string beforeHash, string candidateHash, string afterHash, string detail)
        {
            try
            {
                string log = Path.Combine(root, "local", "profile-edit-actions.log");
                Directory.CreateDirectory(Path.GetDirectoryName(log));
                string cleanPath = (selectedProfilePath ?? "").Replace('\t', ' ').Replace('\r', ' ').Replace('\n', ' ');
                string cleanDetail = (detail ?? "").Replace('\t', ' ').Replace('\r', ' ').Replace('\n', ' ');
                string line = DateTime.UtcNow.ToString("o", CultureInfo.InvariantCulture) + "\t" + outcome + "\tSlot=" + (saveSlot.SelectedIndex + 1).ToString(CultureInfo.InvariantCulture) +
                    "\tProfile=" + cleanPath + "\tBefore=" + beforeHash + "\tCandidate=" + candidateHash + "\tAfter=" + afterHash + "\tDetail=" + cleanDetail + Environment.NewLine;
                File.AppendAllText(log, line, new UTF8Encoding(false));
            }
            catch { /* Audit-log failure must not undo or misreport a verified Profile edit. */ }
        }

        private void RefreshProfilePreview()
        {
            if (workingProfile == null || saveSlot.SelectedIndex < 0 || String.IsNullOrEmpty(selectedProfilePath))
            { savePreview.Text = LauncherLanguage.Text("Select a Profile slot to preview pending values and hash."); return; }
            try
            {
                byte[] candidate = BuildProfileCandidate();
                ProfileSlotSnapshot slot = BlackProfileDocument.Read(candidate).Slots[saveSlot.SelectedIndex];
                int available = 0, empty = 0, zero = 0, one = 0, two = 0;
                for (int i = 0; i < slot.MissionAvailable.Length; i++)
                {
                    if (slot.MissionAvailable[i]) available++;
                    uint value = slot.Active ? slot.MissionResults[i] : 0xFFFFFFFF;
                    if (value == 0xFFFFFFFF) empty++; else if (value == 0) zero++; else if (value == 1) one++; else if (value == 2) two++;
                }
                string hash = ProfileBackupStore.HashBytes(candidate);
                bool changed = hash != loadedProfileHash;
                savePreview.Text = LauncherLanguage.Text("Pending preview") + " | " + LauncherLanguage.Text("Slot ") + (saveSlot.SelectedIndex + 1).ToString(CultureInfo.InvariantCulture) +
                    " | " + LauncherLanguage.Text("Profile name") + ": " + (slot.Active ? slot.Name : LauncherLanguage.Text("(empty slot)")) + "\n" +
                    LauncherLanguage.Text(LauncherLanguage.Text("Available")) + ": " + available.ToString(CultureInfo.InvariantCulture) + "/32 | " +
                    LauncherLanguage.Text("Results") + ": " + empty + " " + LauncherLanguage.Text("Empty") + ", 0: " + zero + ", 1: " + one + ", 2: " + two +
                    " | " + (changed ? LauncherLanguage.Text("Candidate SHA-256 ") : LauncherLanguage.Text("SHA-256 ")) + hash +
                    (changed ? "" : " | " + LauncherLanguage.Text("No changes"));
            }
            catch (Exception error) { savePreview.Text = LauncherLanguage.Text("Preview unavailable: ") + LauncherLanguage.Text(error.Message); }
        }

        private void ShowSaveError(Exception error)
        {
            SetSaveStatus("Save edit stopped: ", "", error.Message);
            if (Visible && !previewOnly) MessageBox.Show(this, LauncherLanguage.Text(error.Message), LauncherLanguage.Text("Profile not changed"), MessageBoxButtons.OK, MessageBoxIcon.Error);
        }

        private void ApplySaveLabels()
        {
            int selectedSlot = Math.Max(0, saveSlot.SelectedIndex);
            refreshingSaveTab = true;
            try
            {
                saveInfiniteAmmo.Text = LauncherLanguage.Text("Infinite ammunition (runtime only)");
                saveSlot.Items.Clear();
                for (int slot = 1; slot <= 3; slot++)
                    saveSlot.Items.Add(LauncherLanguage.Text("Slot ") + slot.ToString(CultureInfo.InvariantCulture));
                if (saveSlot.Items.Count > 0) saveSlot.SelectedIndex = Math.Min(selectedSlot, saveSlot.Items.Count - 1);

                if (saveMissionGrid.Columns.Count >= 4)
                {
                    saveMissionGrid.Columns[0].HeaderText = LauncherLanguage.Text(LauncherLanguage.Text("Difficulty"));
                    saveMissionGrid.Columns[1].HeaderText = LauncherLanguage.Text(LauncherLanguage.Text("Mission"));
                    saveMissionGrid.Columns[2].HeaderText = LauncherLanguage.Text(LauncherLanguage.Text("Available"));
                    saveMissionGrid.Columns[3].HeaderText = LauncherLanguage.Text(LauncherLanguage.Text("Result"));
                    var result = saveMissionGrid.Columns[3] as DataGridViewComboBoxColumn;
                    if (result != null)
                    {
                        result.Items.Clear();
                        result.Items.AddRange(LauncherLanguage.Text("Empty"), "0", "1", "2");
                    }
                    string[] difficulties = { "Easy", "Normal", "Hard", "Black Ops" };
                    for (int row = 0; row < saveMissionGrid.Rows.Count && row < 32; row++)
                    {
                        saveMissionGrid.Rows[row].Cells[0].Value = LauncherLanguage.Text(difficulties[row / 8]);
                        saveMissionGrid.Rows[row].Cells[1].Value = LauncherLanguage.Text("Mission ") + (row % 8 + 1).ToString("00", CultureInfo.InvariantCulture);
                        object value = saveMissionGrid.Rows[row].Cells[3].Value;
                        string resultText = Convert.ToString(value, CultureInfo.InvariantCulture);
                        if (String.IsNullOrEmpty(resultText) || resultText == "Empty" || resultText == "Vazio")
                            saveMissionGrid.Rows[row].Cells[3].Value = LauncherLanguage.Text("Empty");
                    }
                }
            }
            finally { refreshingSaveTab = false; }
            RefreshProfileIdentity();
            RefreshSaveStatus();
            if (workingProfile != null) RefreshSaveSlot();
            else RefreshProfilePreview();
        }
    }
}
