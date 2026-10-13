using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace BlackXboxLauncher
{
    internal sealed class ProfileSlotSnapshot
    {
        internal int Index;
        internal string Name;
        internal bool Active;
        internal bool[] MissionAvailable;
        internal uint[] MissionResults;
    }

    internal sealed class ProfileSnapshot
    {
        internal string Sha256;
        internal ProfileSlotSnapshot[] Slots;
    }

    internal static class BlackProfileDocument
    {
        internal const int FileSize = 0x1800;
        internal const int SlotSize = 0x778;
        private const int PayloadSize = 0x770;
        private const int SlotCount = 3;
        private const int SignatureSize = 20;
        private const int CampaignOffset = 0x124;
        private const int RecordSize = 0x20;
        private const int NameOffset = 0x0D0;
        private const int NameSize = 0x50;
        private const int EmptyFlagOffset = 0x76D;

        internal static ProfileSnapshot Read(byte[] data)
        {
            ValidateOrThrow(data);
            var result = new ProfileSnapshot();
            using (SHA256 sha = SHA256.Create()) result.Sha256 = Hex(sha.ComputeHash(data));
            result.Slots = new ProfileSlotSnapshot[SlotCount];
            for (int slot = 0; slot < SlotCount; slot++)
            {
                int baseOffset = slot * SlotSize;
                bool active = IsActiveSlot(data, slot);
                var snapshot = new ProfileSlotSnapshot {
                    Index = slot, Active = active, Name = String.Empty,
                    MissionAvailable = new bool[32], MissionResults = new uint[32]
                };
                if (active)
                {
                    int nameStart = baseOffset + NameOffset;
                    int nameLength = 0;
                    while (nameLength < NameSize && data[nameStart + nameLength] != 0) nameLength++;
                    if (nameLength == NameSize) throw new InvalidDataException("Profile slot " + slot + " has no terminator in its name field.");
                    for (int i = 0; i < nameLength; i++)
                        if (data[nameStart + i] < 0x20 || data[nameStart + i] > 0x7E)
                            throw new InvalidDataException("Profile slot " + slot + " has a non-printable name byte.");
                    snapshot.Name = Encoding.ASCII.GetString(data, nameStart, nameLength);
                    for (int mission = 0; mission < 32; mission++)
                    {
                        int record = baseOffset + CampaignOffset + mission * RecordSize;
                        uint resultValue = ReadUInt32(data, record);
                        if (resultValue != 0xFFFFFFFF && resultValue > 2)
                            throw new InvalidDataException("Profile slot " + slot + " has an unknown result value at campaign index " + mission + ".");
                        byte available = data[record + 0x1A];
                        if (available > 1)
                            throw new InvalidDataException("Profile slot " + slot + " has an unknown availability value at campaign index " + mission + ".");
                        snapshot.MissionResults[mission] = resultValue;
                        snapshot.MissionAvailable[mission] = available == 1;
                    }
                }
                result.Slots[slot] = snapshot;
            }
            return result;
        }

        internal static byte[] SetName(byte[] source, int slot, string name)
        {
            if (String.IsNullOrEmpty(name) || name.Length >= NameSize) throw new ArgumentException("Profile names must contain 1 to 79 printable ASCII characters.", "name");
            for (int i = 0; i < name.Length; i++)
                if (name[i] < 0x20 || name[i] > 0x7E) throw new ArgumentException("Profile names must contain printable single-byte ASCII only.", "name");
            byte[] data = BeginEdit(source, slot);
            int start = slot * SlotSize + NameOffset;
            Array.Clear(data, start, NameSize);
            byte[] encoded = Encoding.ASCII.GetBytes(name);
            Array.Copy(encoded, 0, data, start, encoded.Length);
            FinishEdit(data, slot);
            return data;
        }

        internal static byte[] SetMissionAvailable(byte[] source, int slot, int difficulty, int position, bool available)
        {
            int record = RecordOffset(slot, difficulty, position);
            byte[] data = BeginEdit(source, slot);
            data[record + 0x1A] = available ? (byte)1 : (byte)0;
            FinishEdit(data, slot);
            return data;
        }

        internal static byte[] SetMissionResult(byte[] source, int slot, int difficulty, int position, uint result)
        {
            if (result != 0xFFFFFFFF && result > 2) throw new ArgumentOutOfRangeException("result", "Known mission results are empty, 0, 1, or 2.");
            int record = RecordOffset(slot, difficulty, position);
            byte[] data = BeginEdit(source, slot);
            WriteUInt32(data, record, result);
            FinishEdit(data, slot);
            return data;
        }

        internal static byte[] RestoreMissionRecord(byte[] source, byte[] original, int slot, int difficulty, int position)
        {
            int record = RecordOffset(slot, difficulty, position);
            byte[] data = BeginEdit(source, slot);
            ValidateOrThrow(original);
            if (!IsActiveSlot(original, slot))
                throw new InvalidDataException("The originally loaded Profile has no active selected slot.");
            Array.Copy(original, record, data, record, RecordSize);
            FinishEdit(data, slot);
            return data;
        }

        internal static byte[] UnlockAllCampaign(byte[] source, int slot)
        {
            byte[] data = BeginEdit(source, slot);
            int slotBase = slot * SlotSize;
            for (int mission = 0; mission < 32; mission++)
            {
                int record = slotBase + CampaignOffset + mission * RecordSize;
                WriteUInt32(data, record, 2);
                data[record + 0x1A] = 1;
            }
            FinishEdit(data, slot);
            return data;
        }

        internal static bool Validate(byte[] data)
        {
            try { ValidateOrThrow(data); return true; }
            catch (InvalidDataException) { return false; }
            catch (ArgumentException) { return false; }
        }

        private static byte[] BeginEdit(byte[] source, int slot)
        {
            if (slot < 0 || slot >= SlotCount) throw new ArgumentOutOfRangeException("slot");
            ValidateOrThrow(source);
            if (!IsActiveSlot(source, slot)) throw new InvalidDataException("Selected Profile slot is empty.");
            return (byte[])source.Clone();
        }

        private static bool IsActiveSlot(byte[] data, int slot)
        {
            int baseOffset = slot * SlotSize;
            return ReadUInt32(data, baseOffset) == 0x26 && data[baseOffset + EmptyFlagOffset] == 0;
        }

        private static int RecordOffset(int slot, int difficulty, int position)
        {
            if (slot < 0 || slot >= SlotCount) throw new ArgumentOutOfRangeException("slot");
            if (difficulty < 0 || difficulty > 3) throw new ArgumentOutOfRangeException("difficulty");
            if (position < 0 || position > 7) throw new ArgumentOutOfRangeException("position");
            return slot * SlotSize + CampaignOffset + ((difficulty * 8) + position) * RecordSize;
        }

        private static void FinishEdit(byte[] data, int slot)
        {
            WriteUInt32(data, slot * SlotSize + PayloadSize, SlotChecksum(data, slot * SlotSize));
            byte[] signature = VirtualSignature(data);
            Array.Copy(signature, 0, data, FileSize - SignatureSize, SignatureSize);
            Array.Clear(signature, 0, signature.Length);
            ValidateOrThrow(data);
        }

        private static void ValidateOrThrow(byte[] data)
        {
            if (data == null || data.Length != FileSize) throw new InvalidDataException("Profile must be exactly 6,144 bytes.");
            for (int slot = 0; slot < SlotCount; slot++)
            {
                int baseOffset = slot * SlotSize;
                uint marker = ReadUInt32(data, baseOffset);
                if (marker != 0x26 && marker != 0) throw new InvalidDataException("Unexpected marker in Profile slot " + slot + ".");
                if (ReadUInt32(data, baseOffset + 4) != SlotSize) throw new InvalidDataException("Unexpected size in Profile slot " + slot + ".");
                if (marker == 0x26 && ReadUInt32(data, baseOffset + PayloadSize) != SlotChecksum(data, baseOffset))
                    throw new InvalidDataException("Checksum mismatch in Profile slot " + slot + ".");
            }
            byte[] expected = VirtualSignature(data);
            for (int i = 0; i < SignatureSize; i++)
                if (expected[i] != data[FileSize - SignatureSize + i])
                {
                    Array.Clear(expected, 0, expected.Length);
                    throw new InvalidDataException("Profile does not have the local Recomp signature.");
                }
            Array.Clear(expected, 0, expected.Length);
        }

        private static uint SlotChecksum(byte[] data, int baseOffset)
        {
            ulong sum = 0;
            for (int offset = 0; offset < PayloadSize; offset += 4) sum = (sum + ReadUInt32(data, baseOffset + offset)) & 0xFFFFFFFFUL;
            return (uint)sum;
        }

        private static byte[] VirtualSignature(byte[] data)
        {
            byte[] key = new byte[16];
            byte[] inner;
            using (HMACSHA1 hmac = new HMACSHA1(key)) inner = hmac.ComputeHash(data, 0, FileSize - SignatureSize);
            byte[] outer;
            using (HMACSHA1 hmac = new HMACSHA1(key)) outer = hmac.ComputeHash(inner);
            Array.Clear(key, 0, key.Length);
            Array.Clear(inner, 0, inner.Length);
            return outer;
        }

        private static uint ReadUInt32(byte[] data, int offset)
        {
            return (uint)(data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24));
        }

        private static void WriteUInt32(byte[] data, int offset, uint value)
        {
            data[offset] = (byte)value; data[offset + 1] = (byte)(value >> 8);
            data[offset + 2] = (byte)(value >> 16); data[offset + 3] = (byte)(value >> 24);
        }

        private static string Hex(byte[] bytes)
        {
            return BitConverter.ToString(bytes).Replace("-", "");
        }
    }

    internal static class ProfileBackupStore
    {
        internal static string DirectoryFor(string projectRoot, string profilePath)
        {
            if (String.IsNullOrWhiteSpace(projectRoot)) throw new ArgumentException("Project root is required.", "projectRoot");
            if (String.IsNullOrWhiteSpace(profilePath)) throw new ArgumentException("Profile path is required.", "profilePath");
            string normalizedPath = Path.GetFullPath(profilePath).ToUpperInvariant();
            byte[] pathBytes = Encoding.UTF8.GetBytes(normalizedPath);
            byte[] digest;
            using (SHA256 sha = SHA256.Create()) digest = sha.ComputeHash(pathBytes);
            Array.Clear(pathBytes, 0, pathBytes.Length);
            string key = BitConverter.ToString(digest, 0, 12).Replace("-", "");
            Array.Clear(digest, 0, digest.Length);
            return Path.Combine(Path.GetFullPath(projectRoot), "local", "profile-backups", "Profile_" + key);
        }

        internal static string[] List(string backupDirectory)
        {
            if (String.IsNullOrWhiteSpace(backupDirectory)) throw new ArgumentException("Backup directory is required.", "backupDirectory");
            if (!Directory.Exists(backupDirectory)) return new string[0];
            var valid = new System.Collections.Generic.List<string>();
            foreach (string path in Directory.GetFiles(backupDirectory, "*.bak"))
            {
                try
                {
                    byte[] bytes = File.ReadAllBytes(path);
                    if (BlackProfileDocument.Validate(bytes)) valid.Add(Path.GetFullPath(path));
                }
                catch (IOException) { }
                catch (UnauthorizedAccessException) { }
            }
            valid.Sort(delegate(string left, string right)
            {
                int byTime = File.GetLastWriteTimeUtc(right).CompareTo(File.GetLastWriteTimeUtc(left));
                return byTime != 0 ? byTime : StringComparer.OrdinalIgnoreCase.Compare(left, right);
            });
            return valid.ToArray();
        }

        internal static string Apply(string targetPath, byte[] candidate, string backupDirectory, string expectedCurrentHash)
        {
            string target = Path.GetFullPath(targetPath);
            if (!File.Exists(target)) throw new FileNotFoundException("Profile file no longer exists.", target);
            if (!BlackProfileDocument.Validate(candidate)) throw new InvalidDataException("Refusing to apply a Profile that failed integrity checks.");
            if (HashFile(target) != expectedCurrentHash) throw new IOException("Profile changed after it was loaded; reload before applying.");
            if (HashFile(target) == HashBytes(candidate)) return null;
            Directory.CreateDirectory(backupDirectory);
            string backup = CopyToUniqueBackup(target, backupDirectory, "Profile_");
            string temporary = target + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                WriteAndFlush(temporary, candidate);
                if (!BlackProfileDocument.Validate(File.ReadAllBytes(temporary))) throw new InvalidDataException("Temporary Profile failed integrity checks.");
                if (HashFile(target) != expectedCurrentHash) throw new IOException("Profile changed during Apply; the original was left in place.");
                File.Replace(temporary, target, null);
                if (!BlackProfileDocument.Validate(File.ReadAllBytes(target))) throw new InvalidDataException("Replaced Profile failed integrity checks; restore the dated backup.");
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
            return backup;
        }

        internal static string Restore(string targetPath, string backupPath, string backupDirectory)
        {
            string target = Path.GetFullPath(targetPath);
            string backup = Path.GetFullPath(backupPath);
            string directory = Path.GetFullPath(backupDirectory).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) + Path.DirectorySeparatorChar;
            if (!backup.StartsWith(directory, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Selected backup is outside this Profile's backup folder.");
            if (!File.Exists(target)) throw new FileNotFoundException("Target Profile file no longer exists.", target);
            byte[] restore = File.ReadAllBytes(backup);
            if (!BlackProfileDocument.Validate(restore)) throw new InvalidDataException("Selected backup failed Profile integrity checks.");
            Directory.CreateDirectory(backupDirectory);
            string beforeRestore = CopyToUniqueBackup(target, backupDirectory, "Profile_before_restore_");
            string temporary = target + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                WriteAndFlush(temporary, restore);
                File.Replace(temporary, target, null);
                if (!BlackProfileDocument.Validate(File.ReadAllBytes(target))) throw new InvalidDataException("Restored Profile failed integrity checks; the pre-restore backup is " + beforeRestore);
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
            return beforeRestore;
        }

        internal static string HashFile(string path)
        {
            using (SHA256 sha = SHA256.Create()) using (FileStream stream = File.OpenRead(path))
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "");
        }

        internal static string HashBytes(byte[] bytes)
        {
            using (SHA256 sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "");
        }

        private static string CopyToUniqueBackup(string source, string directory, string prefix)
        {
            string candidate;
            do { candidate = Path.Combine(directory, prefix + DateTime.Now.ToString("yyyyMMdd_HHmmss_fff", System.Globalization.CultureInfo.InvariantCulture) + "_" + Guid.NewGuid().ToString("N").Substring(0, 8) + ".bak"); }
            while (File.Exists(candidate));
            File.Copy(source, candidate, false);
            return candidate;
        }

        private static void WriteAndFlush(string path, byte[] data)
        {
            using (var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            { stream.Write(data, 0, data.Length); stream.Flush(true); }
        }
    }
}
